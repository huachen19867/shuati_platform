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

  config.judge.dockerImage =
      raw.getString("judge.docker_image", config.judge.dockerImage);
  config.judge.workers = raw.getInt("judge.workers", config.judge.workers);
  config.judge.sourceSizeLimitKb = raw.getInt(
      "judge.source_size_limit_kb", config.judge.sourceSizeLimitKb);
  config.judge.compileTimeoutMs = raw.getInt(
      "judge.compile_timeout_ms", config.judge.compileTimeoutMs);
  config.judge.runTimeoutMs =
      raw.getInt("judge.run_timeout_ms", config.judge.runTimeoutMs);
  config.judge.memoryLimitMb =
      raw.getInt("judge.memory_limit_mb", config.judge.memoryLimitMb);
  config.judge.outputLimitKb =
      raw.getInt("judge.output_limit_kb", config.judge.outputLimitKb);
  config.judge.compileMessageLimitKb = raw.getInt(
      "judge.compile_message_limit_kb", config.judge.compileMessageLimitKb);
  config.judge.stderrLimitKb =
      raw.getInt("judge.stderr_limit_kb", config.judge.stderrLimitKb);
  config.judge.tempDir = raw.getString("judge.temp_dir", config.judge.tempDir);

  config.storage.testcaseDir =
      raw.getString("storage.testcase_dir", config.storage.testcaseDir);
  config.storage.submissionDir =
      raw.getString("storage.submission_dir", config.storage.submissionDir);
  config.storage.sourceRetentionHours = raw.getInt(
      "storage.source_retention_hours", config.storage.sourceRetentionHours);

  config.superAdmin.enabled = raw.getBool(
      "bootstrap.super_admin.enabled", config.superAdmin.enabled);
  config.superAdmin.username = raw.getString(
      "bootstrap.super_admin.username", config.superAdmin.username);
  config.superAdmin.password = raw.getString(
      "bootstrap.super_admin.password", config.superAdmin.password);

  return config;
}

}  // namespace shuati::app
