#include "shuati/judge/local_cpp_runner.h"

#include <sys/wait.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace shuati::judge {
namespace {

std::string shellQuote(const std::string& value) {
  std::string quoted = "'";
  for (const auto ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int timeoutSeconds(int timeoutMs) {
  return std::max(1, (timeoutMs + 999) / 1000);
}

int commandExitCode(int status) {
  if (status == -1) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return status;
}

std::string readFileLimited(const std::filesystem::path& path,
                            std::size_t limitBytes) {
  std::ifstream in(path, std::ios::binary);
  std::string value;
  if (!in) {
    return value;
  }
  value.resize(limitBytes + 1);
  in.read(value.data(), static_cast<std::streamsize>(value.size()));
  value.resize(static_cast<std::size_t>(in.gcount()));
  if (value.size() > limitBytes) {
    value.resize(limitBytes);
  }
  return value;
}

void writeFile(const std::filesystem::path& path, const std::string& content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << content;
}

JudgeCaseResult caseResult(int caseIndex,
                           SubmissionStatus status,
                           int timeMs,
                           const std::string& message,
                           const std::string& stderrText = "") {
  JudgeCaseResult result;
  result.caseIndex = caseIndex;
  result.status = status;
  result.timeMs = timeMs;
  result.memoryKb = 0;
  result.errorType = status == SubmissionStatus::Accepted ? "" : toString(status);
  result.message = message;
  result.stderrText = stderrText;
  return result;
}

}  // namespace

LocalCppRunner::LocalCppRunner(LocalCppRunnerConfig config)
    : config_(std::move(config)) {}

JudgeRunResult LocalCppRunner::judge(const JudgeRunRequest& request) const {
  JudgeRunResult result;
  result.status = SubmissionStatus::SystemError;
  if (request.submissionId <= 0 || request.source.empty() ||
      request.testcases.empty()) {
    result.compileMessage = "invalid judge request";
    return result;
  }

  const auto workDir = std::filesystem::path(config_.tempDir) /
                       ("submission_" + std::to_string(request.submissionId));
  const auto sourcePath = workDir / "main.cpp";
  const auto executablePath = workDir / "main";
  const auto compileLogPath = workDir / "compile.log";

  try {
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);
    writeFile(sourcePath, request.source);

    std::ostringstream compileCommand;
    compileCommand << "timeout " << timeoutSeconds(config_.compileTimeoutMs)
                   << ' ' << config_.compiler
                   << " -std=c++17 -O2 -pipe "
                   << shellQuote(sourcePath.generic_string()) << " -o "
                   << shellQuote(executablePath.generic_string()) << " > "
                   << shellQuote(compileLogPath.generic_string()) << " 2>&1";

    const auto compileExit = commandExitCode(std::system(
        compileCommand.str().c_str()));
    if (compileExit != 0) {
      result.status = SubmissionStatus::CompileError;
      result.compileMessage =
          readFileLimited(compileLogPath,
                          static_cast<std::size_t>(
                              config_.compileMessageLimitKb) *
                              1024U);
      std::filesystem::remove_all(workDir);
      return result;
    }

    result.status = SubmissionStatus::Accepted;
    for (const auto& testcase : request.testcases) {
      const auto actualPath =
          workDir / ("case_" + std::to_string(testcase.caseIndex) + ".actual");
      const auto stderrPath =
          workDir / ("case_" + std::to_string(testcase.caseIndex) + ".stderr");

      std::ostringstream runCommand;
      runCommand << "timeout " << timeoutSeconds(config_.runTimeoutMs) << ' '
                 << shellQuote(executablePath.generic_string()) << " < "
                 << shellQuote(testcase.inputPath) << " > "
                 << shellQuote(actualPath.generic_string()) << " 2> "
                 << shellQuote(stderrPath.generic_string());

      const auto started = std::chrono::steady_clock::now();
      const auto runExit =
          commandExitCode(std::system(runCommand.str().c_str()));
      const auto finished = std::chrono::steady_clock::now();
      const auto timeMs = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              finished - started)
              .count());
      result.totalTimeMs += timeMs;

      const auto stderrText =
          readFileLimited(stderrPath,
                          static_cast<std::size_t>(config_.stderrLimitKb) *
                              1024U);
      if (runExit == 124) {
        result.status = SubmissionStatus::TimeLimitExceeded;
        result.cases.push_back(caseResult(
            testcase.caseIndex, SubmissionStatus::TimeLimitExceeded, timeMs,
            "time limit exceeded", stderrText));
        break;
      }
      if (runExit != 0) {
        result.status = SubmissionStatus::RuntimeError;
        result.cases.push_back(caseResult(
            testcase.caseIndex, SubmissionStatus::RuntimeError, timeMs,
            "runtime error", stderrText));
        break;
      }
      if (std::filesystem::exists(actualPath) &&
          std::filesystem::file_size(actualPath) >
              static_cast<std::uintmax_t>(config_.outputLimitKb) * 1024U) {
        result.status = SubmissionStatus::OutputLimitExceeded;
        result.cases.push_back(caseResult(
            testcase.caseIndex, SubmissionStatus::OutputLimitExceeded, timeMs,
            "output limit exceeded", stderrText));
        break;
      }

      const auto actual = readFileLimited(
          actualPath,
          static_cast<std::size_t>(config_.outputLimitKb) * 1024U + 1U);
      const auto expected =
          readFileLimited(testcase.outputPath,
                          static_cast<std::size_t>(config_.outputLimitKb) *
                              1024U + 1U);
      if (normalizeJudgeOutput(actual) != normalizeJudgeOutput(expected)) {
        result.status = SubmissionStatus::WrongAnswer;
        result.cases.push_back(caseResult(
            testcase.caseIndex, SubmissionStatus::WrongAnswer, timeMs,
            "wrong answer", stderrText));
        break;
      }

      result.cases.push_back(caseResult(testcase.caseIndex,
                                        SubmissionStatus::Accepted, timeMs,
                                        "ok", stderrText));
    }

    std::filesystem::remove_all(workDir);
    return result;
  } catch (const std::exception& ex) {
    result.status = SubmissionStatus::SystemError;
    result.compileMessage = ex.what();
    std::filesystem::remove_all(workDir);
    return result;
  }
}

std::string normalizeJudgeOutput(const std::string& output) {
  std::string normalized;
  normalized.reserve(output.size());
  for (const auto ch : output) {
    if (ch != '\r') {
      normalized.push_back(ch);
    }
  }
  while (!normalized.empty()) {
    const auto ch = normalized.back();
    if (ch == '\n' || ch == ' ' || ch == '\t') {
      normalized.pop_back();
    } else {
      break;
    }
  }
  return normalized;
}

}  // namespace shuati::judge
