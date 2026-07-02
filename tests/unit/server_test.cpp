#include "shuati/app/server.h"

#include <gtest/gtest.h>

#include "shuati/app/app_config.h"

TEST(ServerTest, BuildsHealthResponseUsingUnifiedJsonShape) {
  shuati::app::AppConfig config;
  config.appName = "shuati-platform";
  config.environment = "test";

  const auto response = shuati::app::buildHealthResponse(config);

  EXPECT_EQ(
      response,
      R"({"code":0,"message":"ok","data":{"status":"ok","service":"shuati-platform","environment":"test","version":"0.1.0"}})");
}
