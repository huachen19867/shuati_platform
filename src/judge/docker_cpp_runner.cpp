#include "shuati/judge/docker_cpp_runner.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

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

int timeoutSeconds(int timeoutMs) {
  return std::max(1, (timeoutMs + 999) / 1000);
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
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("cannot write judge file: " + path.string());
  }
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
  result.errorType = status == SubmissionStatus::Accepted ? "" : toString(status);
  result.message = message;
  result.stderrText = stderrText;
  return result;
}

std::string sandboxPrefix(const DockerCppRunnerConfig& config,
                          const std::filesystem::path& workDir,
                          int memoryMb) {
  std::ostringstream command;
  command << shellQuote(config.dockerBinary)
          << " run --rm --network none --cpus 1.0 --memory " << memoryMb
          << "m --memory-swap " << memoryMb
          << "m --pids-limit 64 --read-only"
          << " --tmpfs /tmp:rw,noexec,nosuid,size=64m"
          << " --security-opt no-new-privileges --cap-drop ALL"
          << " --user " << static_cast<long long>(getuid()) << ':'
          << static_cast<long long>(getgid())
          << " -v " << shellQuote(workDir.string() + ":/work:rw")
          << " -w /work " << shellQuote(config.image) << " /bin/sh -lc ";
  return command.str();
}

}  // namespace

DockerCppRunner::DockerCppRunner(DockerCppRunnerConfig config)
    : LocalCppRunner(LocalCppRunnerConfig{}),
      dockerConfig_(std::move(config)) {}

JudgeRunResult DockerCppRunner::judge(const JudgeRunRequest& request) const {
  JudgeRunResult result;
  result.status = SubmissionStatus::SystemError;
  if (request.submissionId <= 0 || request.source.empty() ||
      request.testcases.empty()) {
    result.compileMessage = "invalid judge request";
    return result;
  }

  const auto workDir = std::filesystem::absolute(dockerConfig_.tempDir) /
                       ("submission_" + std::to_string(request.submissionId));
  try {
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);
    writeFile(workDir / "main.cpp", request.source);

    const auto compileCommand =
        sandboxPrefix(dockerConfig_, workDir,
                      std::max(256, dockerConfig_.memoryLimitMb)) +
        shellQuote("timeout " +
                   std::to_string(timeoutSeconds(
                       dockerConfig_.compileTimeoutMs)) +
                   " g++ -std=c++17 -O2 -pipe main.cpp -o main > "
                   "compile.log 2>&1");
    const auto compileExit = commandExitCode(std::system(compileCommand.c_str()));
    if (compileExit != 0) {
      result.status = compileExit == 124 ? SubmissionStatus::SystemError
                                         : SubmissionStatus::CompileError;
      result.compileMessage = readFileLimited(
          workDir / "compile.log",
          static_cast<std::size_t>(dockerConfig_.compileMessageLimitKb) * 1024U);
      if (result.compileMessage.empty()) {
        result.compileMessage = "Docker compile sandbox failed (exit " +
                                std::to_string(compileExit) + ")";
      }
      std::filesystem::remove_all(workDir);
      return result;
    }

    result.status = SubmissionStatus::Accepted;
    for (const auto& testcase : request.testcases) {
      const auto caseName = "case_" + std::to_string(testcase.caseIndex);
      std::filesystem::copy_file(testcase.inputPath, workDir / (caseName + ".in"),
                                 std::filesystem::copy_options::overwrite_existing);
      std::filesystem::copy_file(testcase.outputPath, workDir / (caseName + ".out"),
                                 std::filesystem::copy_options::overwrite_existing);

      // POSIX sh exposes RLIMIT_FSIZE in 512-byte blocks.
      const auto blocks = std::max(1, dockerConfig_.outputLimitKb * 2);
      const auto inner = "ulimit -f " + std::to_string(blocks) +
                         "; timeout " +
                         std::to_string(timeoutSeconds(
                             dockerConfig_.runTimeoutMs)) +
                         " ./main < " + caseName + ".in > " + caseName +
                         ".actual 2> " + caseName + ".stderr";
      const auto command =
          sandboxPrefix(dockerConfig_, workDir, dockerConfig_.memoryLimitMb) +
          shellQuote(inner);
      const auto started = std::chrono::steady_clock::now();
      const auto runExit = commandExitCode(std::system(command.c_str()));
      const auto elapsed = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - started)
              .count());
      result.totalTimeMs += elapsed;
      const auto stderrText = readFileLimited(
          workDir / (caseName + ".stderr"),
          static_cast<std::size_t>(dockerConfig_.stderrLimitKb) * 1024U);

      if (runExit == 124) {
        result.status = SubmissionStatus::TimeLimitExceeded;
        result.cases.push_back(caseResult(testcase.caseIndex, result.status,
                                          elapsed, "time limit exceeded",
                                          stderrText));
        break;
      }
      if (runExit == 137) {
        result.status = SubmissionStatus::MemoryLimitExceeded;
        result.cases.push_back(caseResult(testcase.caseIndex, result.status,
                                          elapsed, "memory limit exceeded",
                                          stderrText));
        break;
      }
      if (runExit == 153) {
        result.status = SubmissionStatus::OutputLimitExceeded;
        result.cases.push_back(caseResult(testcase.caseIndex, result.status,
                                          elapsed, "output limit exceeded",
                                          stderrText));
        break;
      }
      if (runExit != 0) {
        result.status = SubmissionStatus::RuntimeError;
        result.cases.push_back(caseResult(testcase.caseIndex, result.status,
                                          elapsed, "runtime error",
                                          stderrText));
        break;
      }

      const auto actualPath = workDir / (caseName + ".actual");
      if (std::filesystem::exists(actualPath) &&
          std::filesystem::file_size(actualPath) >
              static_cast<std::uintmax_t>(dockerConfig_.outputLimitKb) * 1024U) {
        result.status = SubmissionStatus::OutputLimitExceeded;
        result.cases.push_back(caseResult(testcase.caseIndex, result.status,
                                          elapsed, "output limit exceeded",
                                          stderrText));
        break;
      }
      const auto limit =
          static_cast<std::size_t>(dockerConfig_.outputLimitKb) * 1024U + 1U;
      const auto actual = readFileLimited(actualPath, limit);
      const auto expected =
          readFileLimited(workDir / (caseName + ".out"), limit);
      if (normalizeJudgeOutput(actual) != normalizeJudgeOutput(expected)) {
        result.status = SubmissionStatus::WrongAnswer;
        result.cases.push_back(caseResult(testcase.caseIndex, result.status,
                                          elapsed, "wrong answer",
                                          stderrText));
        break;
      }
      result.cases.push_back(caseResult(testcase.caseIndex,
                                        SubmissionStatus::Accepted, elapsed,
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

}  // namespace shuati::judge
