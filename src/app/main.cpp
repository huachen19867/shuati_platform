#include <exception>
#include <iostream>
#include <string>

#include "httplib.h"
#include "shuati/app/app_config.h"
#include "shuati/app/server.h"

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

    httplib::Server server;
    shuati::app::configureServer(server, config, loggers);

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
