#include "shuati/app/server.h"

#include <string>

#include "shuati/common/json_response.h"

namespace shuati::app {
namespace {

constexpr const char* kVersion = "0.1.0";
constexpr const char* kJsonContentType = "application/json; charset=utf-8";

std::string quotedField(const std::string& name, const std::string& value) {
  return "\"" + name + "\":\"" + shuati::common::escapeJsonString(value) + "\"";
}

}  // namespace

AppLoggers::AppLoggers(const LogConfig& config)
    : access(config.accessPath, config.level),
      error(config.errorPath, config.level),
      judge(config.judgePath, config.level) {}

std::string buildHealthResponse(const AppConfig& config) {
  const std::string data =
      "{" + quotedField("status", "ok") + "," +
      quotedField("service", config.appName) + "," +
      quotedField("environment", config.environment) + "," +
      quotedField("version", kVersion) + "}";

  return shuati::common::makeJsonResponse(0, "ok", data);
}

void configureServer(httplib::Server& server,
                     const AppConfig& config,
                     AppLoggers& loggers) {
  server.Get("/api/health", [&config](const httplib::Request&,
                                      httplib::Response& response) {
    response.status = 200;
    response.set_content(buildHealthResponse(config), kJsonContentType);
  });

  server.set_error_handler([](const httplib::Request&,
                              httplib::Response& response) {
    if (response.status == 404) {
      response.set_content(
          shuati::common::makeJsonResponse(404, "not found"),
          kJsonContentType);
    }
  });

  server.set_logger([&loggers](const httplib::Request& request,
                               const httplib::Response& response) {
    loggers.access.info(request.method + " " + request.path + " " +
                        std::to_string(response.status));
  });

  if (!server.set_mount_point("/", config.server.publicDir)) {
    loggers.error.error("failed to mount static directory: " +
                        config.server.publicDir);
  }
}

}  // namespace shuati::app
