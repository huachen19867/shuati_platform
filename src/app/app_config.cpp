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

  config.database.host =
      raw.getString("database.host", config.database.host);
  config.database.port = raw.getInt("database.port", config.database.port);
  config.database.name =
      raw.getString("database.name", config.database.name);
  config.database.user =
      raw.getString("database.user", config.database.user);
  config.database.password =
      raw.getString("database.password", config.database.password);
  config.database.poolSize =
      raw.getInt("database.pool_size", config.database.poolSize);
  config.database.acquireTimeoutMs = raw.getInt(
      "database.acquire_timeout_ms", config.database.acquireTimeoutMs);

  config.logs.accessPath = raw.getString("logs.access", config.logs.accessPath);
  config.logs.errorPath = raw.getString("logs.error", config.logs.errorPath);
  config.logs.judgePath = raw.getString("logs.judge", config.logs.judgePath);
  config.logs.level =
      shuati::common::parseLogLevel(raw.getString("logs.level", "info"));

  config.security.sessionTtlHours = raw.getInt(
      "security.session_ttl_hours", config.security.sessionTtlHours);
  config.security.submitIntervalSeconds = raw.getInt(
      "security.submit_interval_seconds",
      config.security.submitIntervalSeconds);
  config.security.uploadMaxMb =
      raw.getInt("security.upload_max_mb", config.security.uploadMaxMb);

  config.superAdmin.enabled = raw.getBool(
      "bootstrap.super_admin.enabled", config.superAdmin.enabled);
  config.superAdmin.username = raw.getString(
      "bootstrap.super_admin.username", config.superAdmin.username);
  config.superAdmin.password = raw.getString(
      "bootstrap.super_admin.password", config.superAdmin.password);

  return config;
}

}  // namespace shuati::app
