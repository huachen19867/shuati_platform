#pragma once

#include <string>

#include "httplib.h"
#include "shuati/app/app_config.h"
#include "shuati/auth/auth_service.h"
#include "shuati/common/logging.h"
#include "shuati/judge/local_cpp_runner.h"
#include "shuati/judge/submission_service.h"
#include "shuati/problem/problem_service.h"
#include "shuati/problem/testcase_service.h"

namespace shuati::app {

class AppLoggers {
 public:
  explicit AppLoggers(const LogConfig& config);

  shuati::common::FileLogger access;
  shuati::common::FileLogger error;
  shuati::common::FileLogger judge;
};

std::string buildHealthResponse(const AppConfig& config);
void configureServer(httplib::Server& server,
                     const AppConfig& config,
                     AppLoggers& loggers,
                     shuati::auth::AuthService* authService = nullptr,
                     shuati::problem::ProblemService* problemService = nullptr,
                     shuati::problem::TestcaseService* testcaseService = nullptr,
                     shuati::judge::SubmissionService* submissionService = nullptr,
                     shuati::judge::LocalCppRunner* runner = nullptr);

}  // namespace shuati::app
