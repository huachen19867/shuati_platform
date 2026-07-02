#include "shuati/app/app_config.h"

#include "shuati/common/config.h"

namespace shuati::app {

AppConfig AppConfig::loadFromFile(const std::string& path) {
  const auto raw = shuati::common::Config::loadFromFile(path);
  AppConfig config;

  config.appName = raw.getString("app.name", config.appName);
  config.environment = raw.getString("app.environment", config.environment);

  config.server.host = raw.getString("server.host", config.server.host);
  config.server.port = raw.getInt("server.port", config.server.port);
  config.server.publicDir =
      raw.getString("server.public_dir", config.server.publicDir);

  config.logs.accessPath = raw.getString("logs.access", config.logs.accessPath);
  config.logs.errorPath = raw.getString("logs.error", config.logs.errorPath);
  config.logs.judgePath = raw.getString("logs.judge", config.logs.judgePath);
  config.logs.level =
      shuati::common::parseLogLevel(raw.getString("logs.level", "info"));

  return config;
}

}  // namespace shuati::app
