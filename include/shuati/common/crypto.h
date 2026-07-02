#pragma once

#include <cstddef>
#include <string>

namespace shuati::common {

std::string sha256Hex(const std::string& value);
std::string randomBytes(std::size_t byteCount);
std::string randomHex(std::size_t byteCount);

}  // namespace shuati::common
