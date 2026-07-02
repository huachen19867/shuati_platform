#pragma once

#include <string>

#include "httplib.h"
#include "shuati/app/app_config.h"
#include "shuati/auth/auth_service.h"
#include "shuati/common/logging.h"

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
                     shuati::auth::AuthService* authService = nullptr);

}  // namespace shuati::app
