#pragma once

#include <string>

#include "shuati/judge/local_cpp_runner.h"

namespace shuati::judge {

struct DockerCppRunnerConfig {
  std::string dockerBinary = "docker";
  std::string image = "shuati-cpp-judge:latest";
  std::string tempDir = "data/judge_tmp";
  int compileTimeoutMs = 10000;
  int runTimeoutMs = 2000;
  int memoryLimitMb = 128;
  int outputLimitKb = 1024;
  int compileMessageLimitKb = 8;
  int stderrLimitKb = 4;
};

// Executes untrusted submissions in a short-lived, networkless Docker
// container. The backend must never use LocalCppRunner on a public service.
class DockerCppRunner final : public LocalCppRunner {
 public:
  explicit DockerCppRunner(DockerCppRunnerConfig config);

  JudgeRunResult judge(const JudgeRunRequest& request) const override;

 private:
  DockerCppRunnerConfig dockerConfig_;
};

}  // namespace shuati::judge
