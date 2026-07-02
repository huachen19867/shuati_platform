#include "shuati/common/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace shuati::common {
namespace {

std::string trim(const std::string& value) {
  const auto begin = std::find_if_not(value.begin(), value.end(),
                                      [](unsigned char ch) {
                                        return std::isspace(ch) != 0;
                                      });
  const auto end = std::find_if_not(value.rbegin(), value.rend(),
                                    [](unsigned char ch) {
                                      return std::isspace(ch) != 0;
                                    })
                       .base();
  if (begin >= end) {
    return "";
  }
  return std::string(begin, end);
}

std::string stripInlineComment(const std::string& line) {
  bool inSingleQuotes = false;
  bool inDoubleQuotes = false;

  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (ch == '\'' && !inDoubleQuotes) {
      inSingleQuotes = !inSingleQuotes;
    } else if (ch == '"' && !inSingleQuotes) {
      inDoubleQuotes = !inDoubleQuotes;
    } else if (ch == '#' && !inSingleQuotes && !inDoubleQuotes) {
      return line.substr(0, i);
    }
  }

  return line;
}

int leadingSpaces(const std::string& line) {
  int count = 0;
  for (const char ch : line) {
    if (ch != ' ') {
      break;
    }
    ++count;
  }
  return count;
}

std::string unquote(const std::string& value) {
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::string joinKey(const std::vector<std::pair<int, std::string>>& sections,
                    const std::string& leaf) {
  std::ostringstream out;
  for (const auto& [indent, section] : sections) {
    (void)indent;
    if (out.tellp() > 0) {
      out << '.';
    }
    out << section;
  }
  if (out.tellp() > 0) {
    out << '.';
  }
  out << leaf;
  return out.str();
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

}  // namespace

Config::Config(std::unordered_map<std::string, std::string> values)
    : values_(std::move(values)) {}

Config Config::loadFromFile(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open config file: " + path);
  }

  std::unordered_map<std::string, std::string> values;
  std::vector<std::pair<int, std::string>> sections;
  std::string rawLine;

  while (std::getline(input, rawLine)) {
    const std::string withoutComment = stripInlineComment(rawLine);
    const std::string line = trim(withoutComment);
    if (line.empty()) {
      continue;
    }

    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      throw std::runtime_error("invalid config line: " + rawLine);
    }

    const int indent = leadingSpaces(withoutComment);
    const std::string key = trim(line.substr(0, colon));
    const std::string value = trim(line.substr(colon + 1));
    if (key.empty()) {
      throw std::runtime_error("empty config key: " + rawLine);
    }

    while (!sections.empty() && sections.back().first >= indent) {
      sections.pop_back();
    }

    if (value.empty()) {
      sections.emplace_back(indent, key);
      continue;
    }

    values[joinKey(sections, key)] = unquote(value);
  }

  return Config(std::move(values));
}

std::optional<std::string> Config::find(const std::string& key) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::string Config::getString(const std::string& key,
                              const std::string& defaultValue) const {
  return find(key).value_or(defaultValue);
}

int Config::getInt(const std::string& key, int defaultValue) const {
  const auto value = find(key);
  if (!value.has_value()) {
    return defaultValue;
  }
  return std::stoi(*value);
}

bool Config::getBool(const std::string& key, bool defaultValue) const {
  const auto value = find(key);
  if (!value.has_value()) {
    return defaultValue;
  }

  const auto normalized = lower(*value);
  if (normalized == "true" || normalized == "1" || normalized == "yes") {
    return true;
  }
  if (normalized == "false" || normalized == "0" || normalized == "no") {
    return false;
  }
  throw std::runtime_error("invalid boolean config value for key: " + key);
}

bool Config::contains(const std::string& key) const {
  return values_.find(key) != values_.end();
}

}  // namespace shuati::common
