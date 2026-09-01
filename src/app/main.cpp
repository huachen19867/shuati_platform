#include <exception>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
    // learn the admin API first. Missing built-in problems are added by title,
    // so an existing deployment can receive new starter content on restart.
    {
      const auto admin = users->findByUsername(config.superAdmin.username);
      if (admin.has_value()) {
        struct SeedProblem {
          shuati::problem::ProblemDraft draft;
          std::vector<shuati::problem::TestcaseFile> testcases;
        };
        const std::vector<SeedProblem> seeds = {
            {{"A + B", "读入两个整数 a 和 b，输出它们的和。",
              "一行两个整数 a 和 b。", "输出一个整数，表示 a + b。",
              R"([{"input":"1 2\n","output":"3\n"}])",
              shuati::problem::Difficulty::Easy, {"入门", "数学"}},
             {{"1.in", "1 2\n"}, {"1.out", "3\n"},
              {"2.in", "-10 7\n"}, {"2.out", "-3\n"}}},
            {{"C语言：最大公约数",
              "使用辗转相除法，计算两个正整数的最大公约数。",
              "一行输入两个正整数 a 和 b（1 ≤ a, b ≤ 10^9）。",
              "输出 a 和 b 的最大公约数。",
              R"([{"input":"48 18\n","output":"6\n"}])",
              shuati::problem::Difficulty::Easy, {"C语言", "数学", "入门"}},
             {{"1.in", "48 18\n"}, {"1.out", "6\n"},
              {"2.in", "7 5\n"}, {"2.out", "1\n"},
              {"3.in", "100 25\n"}, {"3.out", "25\n"},
              {"4.in", "270 192\n"}, {"4.out", "6\n"},
              {"5.in", "1 999999937\n"}, {"5.out", "1\n"}}},
            {{"C语言：数组逆序",
              "读入一个整数数组，将数组中的元素按相反顺序输出。",
              "第一行输入 n（1 ≤ n ≤ 100），第二行输入 n 个整数。",
              "按逆序输出 n 个整数，整数之间用一个空格分隔。",
              R"([{"input":"5\n1 2 3 4 5\n","output":"5 4 3 2 1\n"}])",
              shuati::problem::Difficulty::Easy, {"C语言", "数组", "入门"}},
             {{"1.in", "5\n1 2 3 4 5\n"},
              {"1.out", "5 4 3 2 1\n"},
              {"2.in", "4\n-2 0 7 9\n"},
              {"2.out", "9 7 0 -2\n"},
              {"3.in", "1\n42\n"}, {"3.out", "42\n"},
              {"4.in", "6\n3 3 -1 8 0 5\n"},
              {"4.out", "5 0 8 -1 3 3\n"}}},
            {{"C语言：统计正负数",
              "统计一组整数中正数、负数和零的个数。",
              "第一行输入 n（1 ≤ n ≤ 1000），第二行输入 n 个整数。",
              "依次输出正数个数、负数个数和零的个数，数字之间用一个空格分隔。",
              R"([{"input":"6\n-1 0 3 4 -2 0\n","output":"2 2 2\n"}])",
              shuati::problem::Difficulty::Easy, {"C语言", "循环", "统计"}},
             {{"1.in", "6\n-1 0 3 4 -2 0\n"},
              {"1.out", "2 2 2\n"},
              {"2.in", "5\n1 2 3 4 5\n"}, {"2.out", "5 0 0\n"},
              {"3.in", "4\n-8 -3 -1 -9\n"}, {"3.out", "0 4 0\n"},
              {"4.in", "3\n0 0 0\n"}, {"4.out", "0 0 3\n"}}},
            {{"C语言：回文字符串",
              "判断一个只包含小写英文字母的字符串是否是回文字符串。",
              "输入一个长度不超过 100 的非空小写字母字符串。",
              "如果字符串正读和反读相同，输出 YES，否则输出 NO。",
              R"([{"input":"level\n","output":"YES\n"}])",
              shuati::problem::Difficulty::Easy, {"C语言", "字符串", "入门"}},
             {{"1.in", "level\n"}, {"1.out", "YES\n"},
              {"2.in", "hello\n"}, {"2.out", "NO\n"},
              {"3.in", "a\n"}, {"3.out", "YES\n"},
              {"4.in", "abccba\n"}, {"4.out", "YES\n"},
              {"5.in", "abcd\n"}, {"5.out", "NO\n"}}},
            {{"C语言：数字各位和",
              "计算一个非负整数所有数位上的数字之和。",
              "输入一个非负整数 n（0 ≤ n ≤ 10^18）。",
              "输出 n 的各位数字之和。",
              R"([{"input":"12345\n","output":"15\n"}])",
              shuati::problem::Difficulty::Easy, {"C语言", "循环", "数学"}},
             {{"1.in", "12345\n"}, {"1.out", "15\n"},
              {"2.in", "0\n"}, {"2.out", "0\n"},
              {"3.in", "900000000000\n"}, {"3.out", "9\n"},
              {"4.in", "987654321\n"}, {"4.out", "45\n"},
              {"5.in", "1000000000000000000\n"}, {"5.out", "1\n"}}}};

        const auto existing = problemService.listProblems({}).problems;
        for (const auto& seed : seeds) {
          bool alreadyExists = false;
          for (const auto& problem : existing) {
            if (problem.title == seed.draft.title) {
              alreadyExists = true;
              break;
            }
          }
          if (alreadyExists) {
            continue;
          }
          const auto seeded = problemService.createProblem(
              {admin->id, admin->role}, seed.draft);
          if (seeded.ok) {
            testcaseService.replaceTestcases(seeded.problem.id, seed.testcases);
            loggers.error.info("seeded starter problem: " + seed.draft.title);
          }
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
