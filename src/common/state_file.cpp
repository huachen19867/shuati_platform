#include "shuati/common/state_file.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace shuati::common {
namespace {

int hexValue(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  throw std::runtime_error("invalid hexadecimal state data");
}

}  // namespace

std::string hexEncode(const std::string& value) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string encoded;
  encoded.reserve(value.size() * 2);
  for (const auto ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    encoded.push_back(digits[byte >> 4]);
    encoded.push_back(digits[byte & 0x0f]);
  }
  return encoded;
}

std::string hexDecode(const std::string& value) {
  if (value.size() % 2 != 0) {
    throw std::runtime_error("invalid hexadecimal state length");
  }
  std::string decoded;
  decoded.reserve(value.size() / 2);
  for (std::size_t i = 0; i < value.size(); i += 2) {
    decoded.push_back(static_cast<char>((hexValue(value[i]) << 4) |
                                        hexValue(value[i + 1])));
  }
  return decoded;
}

std::vector<std::string> splitStateLine(const std::string& value) {
  std::vector<std::string> fields;
  std::size_t start = 0;
  while (true) {
    const auto separator = value.find('\t', start);
    fields.push_back(value.substr(start, separator == std::string::npos
                                            ? std::string::npos
                                            : separator - start));
    if (separator == std::string::npos) break;
    start = separator + 1;
  }
  return fields;
}

std::int64_t epochMilliseconds(std::chrono::system_clock::time_point value) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             value.time_since_epoch())
      .count();
}

std::chrono::system_clock::time_point timeFromEpochMilliseconds(
    std::int64_t value) {
  return std::chrono::system_clock::time_point(std::chrono::milliseconds(value));
}

std::string readStateFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return "";
  std::ostringstream content;
  content << in.rdbuf();
  if (!in.good() && !in.eof()) {
    throw std::runtime_error("cannot read state file: " + path.string());
  }
  return content.str();
}

void atomicWriteStateFile(const std::filesystem::path& path,
                          const std::string& content) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  const auto temporary = path.string() + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      throw std::runtime_error("cannot write state file: " + path.string());
    }
    out << content;
    out.flush();
    if (!out) {
      throw std::runtime_error("cannot flush state file: " + path.string());
    }
  }
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
  }
  if (error) {
    throw std::runtime_error("cannot replace state file: " + path.string());
  }
}

}  // namespace shuati::common
