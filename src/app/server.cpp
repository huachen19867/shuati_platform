#include "shuati/app/server.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

#include "shuati/auth/role.h"
#include "shuati/common/json_response.h"

namespace shuati::app {
namespace {

constexpr const char* kVersion = "0.1.0";
constexpr const char* kJsonContentType = "application/json; charset=utf-8";

std::string quotedField(const std::string& name, const std::string& value) {
  return "\"" + name + "\":\"" + shuati::common::escapeJsonString(value) + "\"";
}

std::string userJson(const shuati::auth::User& user) {
  return "{\"id\":" + std::to_string(user.id) + "," +
         quotedField("username", user.username) + "," +
         quotedField("role", shuati::auth::toString(user.role)) + "}";
}

std::string usersJson(const std::vector<shuati::auth::User>& users) {
  std::ostringstream out;
  out << "{\"users\":[";
  for (std::size_t i = 0; i < users.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << userJson(users[i]);
  }
  out << "]}";
  return out.str();
}

int authStatus(shuati::auth::AuthError error) {
  switch (error) {
    case shuati::auth::AuthError::None:
      return 200;
    case shuati::auth::AuthError::InvalidInput:
      return 400;
    case shuati::auth::AuthError::AlreadyExists:
      return 409;
    case shuati::auth::AuthError::InvalidCredentials:
    case shuati::auth::AuthError::Unauthorized:
      return 401;
    case shuati::auth::AuthError::Forbidden:
      return 403;
    case shuati::auth::AuthError::NotFound:
      return 404;
  }
  return 500;
}

void setJson(httplib::Response& response,
             int status,
             const std::string& body) {
  response.status = status;
  response.set_content(body, kJsonContentType);
}

void setAuthError(httplib::Response& response, shuati::auth::AuthError error) {
  const auto status = authStatus(error);
  setJson(response, status,
          shuati::common::makeJsonResponse(status,
                                           shuati::auth::authErrorMessage(error)));
}

std::optional<std::string> extractJsonString(const std::string& body,
                                             const std::string& field) {
  const auto key = "\"" + field + "\"";
  const auto keyPos = body.find(key);
  if (keyPos == std::string::npos) {
    return std::nullopt;
  }
  const auto colon = body.find(':', keyPos + key.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  auto quote = body.find('"', colon + 1);
  if (quote == std::string::npos) {
    return std::nullopt;
  }

  std::string value;
  bool escaped = false;
  for (std::size_t i = quote + 1; i < body.size(); ++i) {
    const char ch = body[i];
    if (escaped) {
      switch (ch) {
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          value.push_back(ch);
          break;
      }
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }
  return std::nullopt;
}

std::string sessionToken(const httplib::Request& request) {
  return request.get_header_value("X-Session-Token");
}

std::string unixSeconds(std::chrono::system_clock::time_point value) {
  return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                            value.time_since_epoch())
                            .count());
}

void registerAuthRoutes(httplib::Server& server,
                        shuati::auth::AuthService& authService) {
  server.Post("/api/auth/register", [&authService](const httplib::Request& req,
                                                   httplib::Response& res) {
    const auto username = extractJsonString(req.body, "username");
    const auto password = extractJsonString(req.body, "password");
    if (!username.has_value() || !password.has_value()) {
      setAuthError(res, shuati::auth::AuthError::InvalidInput);
      return;
    }

    const auto result = authService.registerUser(*username, *password);
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }

    setJson(res, 200,
            shuati::common::makeJsonResponse(
                0, "ok", "{\"user\":" + userJson(result.user) + "}"));
  });

  server.Post("/api/auth/login", [&authService](const httplib::Request& req,
                                                httplib::Response& res) {
    const auto username = extractJsonString(req.body, "username");
    const auto password = extractJsonString(req.body, "password");
    if (!username.has_value() || !password.has_value()) {
      setAuthError(res, shuati::auth::AuthError::InvalidInput);
      return;
    }

    const auto result = authService.login(*username, *password);
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }

    res.set_header("X-Session-Token", result.token);
    const std::string data = "{\"token\":\"" +
                             shuati::common::escapeJsonString(result.token) +
                             "\",\"expires_at\":" +
                             unixSeconds(result.expiresAt) + ",\"user\":" +
                             userJson(result.user) + "}";
    setJson(res, 200, shuati::common::makeJsonResponse(0, "ok", data));
  });

  server.Post("/api/auth/logout", [&authService](const httplib::Request& req,
                                                 httplib::Response& res) {
    const auto result = authService.logout(sessionToken(req));
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }
    setJson(res, 200, shuati::common::makeJsonResponse(0, "ok"));
  });

  server.Get("/api/auth/me", [&authService](const httplib::Request& req,
                                            httplib::Response& res) {
    const auto result = authService.currentUser(sessionToken(req));
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }
    setJson(res, 200,
            shuati::common::makeJsonResponse(
                0, "ok", "{\"user\":" + userJson(result.user) + "}"));
  });

  server.Get("/api/admin/users", [&authService](const httplib::Request& req,
                                                httplib::Response& res) {
    const auto result = authService.listUsers(sessionToken(req));
    if (!result.ok) {
      setAuthError(res, result.error);
      return;
    }
    setJson(res, 200,
            shuati::common::makeJsonResponse(0, "ok",
                                             usersJson(result.users)));
  });

  server.Put(R"(/api/admin/users/(\d+)/role)",
             [&authService](const httplib::Request& req,
                            httplib::Response& res) {
               if (req.matches.size() < 2) {
                 setAuthError(res, shuati::auth::AuthError::NotFound);
                 return;
               }
               const auto role = extractJsonString(req.body, "role");
               if (!role.has_value()) {
                 setAuthError(res, shuati::auth::AuthError::InvalidInput);
                 return;
               }

               try {
                 const auto targetId = std::stoll(req.matches[1].str());
                 const auto result = authService.updateUserRole(
                     sessionToken(req), targetId, shuati::auth::parseRole(*role));
                 if (!result.ok) {
                   setAuthError(res, result.error);
                   return;
                 }
                 setJson(res, 200,
                         shuati::common::makeJsonResponse(
                             0, "ok",
                             "{\"user\":" + userJson(result.user) + "}"));
               } catch (const std::exception&) {
                 setAuthError(res, shuati::auth::AuthError::InvalidInput);
               }
             });
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
                     AppLoggers& loggers,
                     shuati::auth::AuthService* authService) {
  server.Get("/api/health", [&config](const httplib::Request&,
                                      httplib::Response& response) {
    response.status = 200;
    response.set_content(buildHealthResponse(config), kJsonContentType);
  });

  if (authService != nullptr) {
    registerAuthRoutes(server, *authService);
  }

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
