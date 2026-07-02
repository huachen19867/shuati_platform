#include <exception>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "httplib.h"
#include "shuati/app/app_config.h"
#include "shuati/app/server.h"
#include "shuati/auth/auth_service.h"
#include "shuati/auth/password_hasher.h"
#include "shuati/auth/session_manager.h"
#include "shuati/auth/user_repository.h"

namespace {

std::string configPathFromArgs(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--config") {
    return argv[2];
  }
  return "config/app.yaml";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto config = shuati::app::AppConfig::loadFromFile(
        configPathFromArgs(argc, argv));
    shuati::app::AppLoggers loggers(config.logs);
    auto users = std::make_shared<shuati::auth::InMemoryUserRepository>();
    auto passwordHasher =
        std::make_shared<shuati::auth::Sha256PasswordHasher>();
    auto sessions = std::make_shared<shuati::auth::SessionManager>(
        std::chrono::hours(config.security.sessionTtlHours));
    shuati::auth::AuthService authService(users, passwordHasher, sessions);

    if (config.superAdmin.enabled) {
      const auto bootstrapped = authService.bootstrapSuperAdmin(
          config.superAdmin.username, config.superAdmin.password);
      if (bootstrapped.ok) {
        loggers.error.info("bootstrapped super admin: " +
                           bootstrapped.user.username);
      } else if (bootstrapped.error !=
                 shuati::auth::AuthError::AlreadyExists) {
        loggers.error.warn("failed to bootstrap super admin: " +
                           bootstrapped.message);
      }
    }

    httplib::Server server;
    shuati::app::configureServer(server, config, loggers, &authService);

    std::cout << "shuati_platform listening on http://" << config.server.host
              << ':' << config.server.port << '\n';
    if (!server.listen(config.server.host, config.server.port)) {
      loggers.error.error("server listen failed on " + config.server.host +
                          ":" + std::to_string(config.server.port));
      return 1;
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "failed to start shuati_platform: " << ex.what() << '\n';
    return 1;
  }
}
