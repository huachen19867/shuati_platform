#include "shuati/common/json_response.h"

#include <iomanip>
#include <sstream>

namespace shuati::common {

std::string escapeJsonString(const std::string& value) {
  std::ostringstream out;
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(ch) << std::dec;
        } else {
          out << static_cast<char>(ch);
        }
        break;
    }
  }
  return out.str();
}

std::string makeJsonResponse(int code,
                             const std::string& message,
                             const std::string& dataJson) {
  std::ostringstream out;
  out << "{\"code\":" << code << ",\"message\":\""
      << escapeJsonString(message) << "\",\"data\":" << dataJson << '}';
  return out.str();
}

}  // namespace shuati::common
