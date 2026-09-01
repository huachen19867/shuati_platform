#include <exception>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "httplib.h"
#include "shuati/app/app_config.h"
#include "shuati/app/server.h"
#include "shuati/auth/auth_service.h"
#include "shuati/auth/password_hasher.h"
#include "shuati/auth/session_manager.h"
#include "shuati/auth/user_repository.h"
#include "shuati/judge/local_cpp_runner.h"
#include "shuati/judge/docker_cpp_runner.h"
#include "shuati/judge/submission_service.h"
#include "shuati/problem/problem_service.h"
#include "shuati/problem/testcase_service.h"

namespace {

std::string configPathFromArgs(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--config") {
    return argv[2];
  }
  return "config/app.yaml";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto config = shuati::app::AppConfig::loadFromFile(
        configPathFromArgs(argc, argv));
    if (config.environment == "production" && config.judge.runner != "docker") {
      throw std::runtime_error(
          "production requires judge.runner=docker; local execution is unsafe");
    }
    if (config.environment == "production" && config.superAdmin.enabled &&
        (config.superAdmin.password == "change-me-now" ||
         config.superAdmin.password == "REPLACE_WITH_A_RANDOM_PASSWORD" ||
         config.superAdmin.password.size() < 12)) {
      throw std::runtime_error(
          "production bootstrap password must be changed");
    }
    shuati::app::AppLoggers loggers(config.logs);
    const auto stateDir = std::filesystem::path(config.storage.stateDir);
    auto users = std::make_shared<shuati::auth::InMemoryUserRepository>(
        stateDir / "users.state");
    auto passwordHasher =
        std::make_shared<shuati::auth::Sha256PasswordHasher>();
    auto sessions = std::make_shared<shuati::auth::SessionManager>(
        std::chrono::hours(config.security.sessionTtlHours));
    shuati::auth::AuthService authService(users, passwordHasher, sessions);
    auto problems = std::make_shared<shuati::problem::InMemoryProblemRepository>(
        stateDir / "problems.state");
    shuati::problem::ProblemService problemService(problems);
    shuati::problem::TestcaseLimits testcaseLimits;
    testcaseLimits.maxPackageBytes =
        static_cast<std::uintmax_t>(config.security.uploadMaxMb) * 1024U * 1024U;
    shuati::problem::TestcaseService testcaseService(config.storage.testcaseDir,
                                                     testcaseLimits);
    auto submissions = std::make_shared<shuati::judge::InMemorySubmissionRepository>(
        [] { return std::chrono::system_clock::now(); },
        stateDir / "submissions.state");
    shuati::judge::SubmissionService submissionService(
        submissions, config.judge.sourceSizeLimitKb,
        std::chrono::seconds(config.security.submitIntervalSeconds));
    const auto recovered = submissionService.recoverInterruptedSubmissions();
    if (recovered > 0) {
      loggers.judge.warn("recovered interrupted submissions: " +
                         std::to_string(recovered));
    }
    submissionService.cleanupExpiredSources(
        std::chrono::hours(config.storage.sourceRetentionHours));
    std::unique_ptr<shuati::judge::LocalCppRunner> runner;
    if (config.judge.runner == "docker") {
      runner = std::make_unique<shuati::judge::DockerCppRunner>(
          shuati::judge::DockerCppRunnerConfig{
              config.judge.dockerBinary, config.judge.dockerImage,
              config.judge.tempDir, config.judge.compileTimeoutMs,
              config.judge.runTimeoutMs, config.judge.memoryLimitMb,
              config.judge.outputLimitKb, config.judge.compileMessageLimitKb,
              config.judge.stderrLimitKb});
    } else if (config.judge.runner == "local") {
      runner = std::make_unique<shuati::judge::LocalCppRunner>(
          shuati::judge::LocalCppRunnerConfig{
              "g++", config.judge.tempDir, config.judge.compileTimeoutMs,
              config.judge.runTimeoutMs, config.judge.outputLimitKb,
              config.judge.compileMessageLimitKb, config.judge.stderrLimitKb});
    } else {
      throw std::runtime_error("unknown judge.runner: " + config.judge.runner);
    }

    if (config.superAdmin.enabled) {
      const auto bootstrapped = authService.bootstrapSuperAdmin(
          config.superAdmin.username, config.superAdmin.password);
      if (bootstrapped.ok) {
        loggers.error.info("bootstrapped super admin: " +
                           bootstrapped.user.username);
      } else if (bootstrapped.error !=
                 shuati::auth::AuthError::AlreadyExists) {
        loggers.error.warn("failed to bootstrap super admin: " +
                           bootstrapped.message);
      }
    }

    // Keep a fresh deployment immediately usable without forcing the owner to
    // learn the admin API first. The seed is added only when no problem exists.
    if (problemService.listProblems({}).problems.empty()) {
      const auto admin = users->findByUsername(config.superAdmin.username);
      if (admin.has_value()) {
        shuati::problem::ProblemDraft draft;
        draft.title = "A + B";
        draft.statement = "读入两个整数 a 和 b，输出它们的和。";
        draft.inputDescription = "一行两个整数 a 和 b。";
        draft.outputDescription = "输出一个整数，表示 a + b。";
        draft.samplesJson = R"([{"input":"1 2\n","output":"3\n"}])";
        draft.difficulty = shuati::problem::Difficulty::Easy;
        draft.tags = {"入门", "数学"};
        const auto seeded = problemService.createProblem(
            {admin->id, admin->role}, draft);
        if (seeded.ok) {
          testcaseService.replaceTestcases(
              seeded.problem.id, {{"1.in", "1 2\n"}, {"1.out", "3\n"},
                                  {"2.in", "-10 7\n"}, {"2.out", "-3\n"}});
          loggers.error.info("seeded starter problem: A + B");
        }
      }
    }

    httplib::Server server;
    shuati::app::configureServer(server, config, loggers, &authService,
                                 &problemService, &testcaseService,
                                 &submissionService, runner.get());

    std::cout << "shuati_platform listening on http://" << config.server.host
              << ':' << config.server.port << '\n';
    if (!server.listen(config.server.host, config.server.port)) {
      loggers.error.error("server listen failed on " + config.server.host +
                          ":" + std::to_string(config.server.port));
      return 1;
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "failed to start shuati_platform: " << ex.what() << '\n';
    return 1;
  }
}
