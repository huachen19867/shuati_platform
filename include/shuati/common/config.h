#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace shuati::common {

class Config {
 public:
  static Config loadFromFile(const std::string& path);

  [[nodiscard]] std::string getString(
      const std::string& key,
      const std::string& defaultValue = "") const;
  [[nodiscard]] int getInt(const std::string& key, int defaultValue = 0) const;
  [[nodiscard]] bool getBool(const std::string& key,
                             bool defaultValue = false) const;
  [[nodiscard]] bool contains(const std::string& key) const;

 private:
  explicit Config(std::unordered_map<std::string, std::string> values);

  [[nodiscard]] std::optional<std::string> find(
      const std::string& key) const;

  std::unordered_map<std::string, std::string> values_;
};

}  // namespace shuati::common
