#pragma once

#include <functional>
#include <string>

namespace shuati::auth {

class PasswordHasher {
 public:
  using SaltGenerator = std::function<std::string()>;

  PasswordHasher();
  explicit PasswordHasher(SaltGenerator saltGenerator);

  std::string hashPassword(const std::string& password) const;
  bool verifyPassword(const std::string& password,
                      const std::string& encodedHash) const;

 private:
  SaltGenerator saltGenerator_;
};

using Sha256PasswordHasher = PasswordHasher;

}  // namespace shuati::auth
