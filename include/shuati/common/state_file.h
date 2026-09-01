#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace shuati::common {

std::string hexEncode(const std::string& value);
std::string hexDecode(const std::string& value);
std::vector<std::string> splitStateLine(const std::string& value);
std::int64_t epochMilliseconds(std::chrono::system_clock::time_point value);
std::chrono::system_clock::time_point timeFromEpochMilliseconds(
    std::int64_t value);
std::string readStateFile(const std::filesystem::path& path);
void atomicWriteStateFile(const std::filesystem::path& path,
                          const std::string& content);

}  // namespace shuati::common
