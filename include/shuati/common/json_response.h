#pragma once

#include <string>

namespace shuati::common {

std::string escapeJsonString(const std::string& value);
std::string makeJsonResponse(int code,
                             const std::string& message,
                             const std::string& dataJson = "{}");

}  // namespace shuati::common
