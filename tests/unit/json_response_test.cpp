#include "shuati/common/json_response.h"

#include <gtest/gtest.h>

#include <string>

TEST(JsonResponseTest, BuildsUnifiedResponseWithRawDataObject) {
  const auto response = shuati::common::makeJsonResponse(
      0, "ok", R"({"status":"ok","service":"shuati-platform"})");

  EXPECT_EQ(
      response,
      R"({"code":0,"message":"ok","data":{"status":"ok","service":"shuati-platform"}})");
}

TEST(JsonResponseTest, EscapesJsonStringContent) {
  EXPECT_EQ(shuati::common::escapeJsonString("bad \"input\"\npath\\tmp"),
            R"(bad \"input\"\npath\\tmp)");
}

TEST(JsonResponseTest, DefaultsDataToEmptyObject) {
  EXPECT_EQ(shuati::common::makeJsonResponse(404, "not found"),
            R"({"code":404,"message":"not found","data":{}})");
}
