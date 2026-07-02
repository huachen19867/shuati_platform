#pragma once

#include <functional>
#include <string>

namespace shuati::auth {

class Sha256PasswordHasher {
 public:
  using SaltGenerator = std::function<std::string()>;

  Sha256PasswordHasher();
  explicit Sha256PasswordHasher(SaltGenerator saltGenerator);

  std::string hashPassword(const std::string& password) const;
  bool verifyPassword(const std::string& password,
                      const std::string& encodedHash) const;

 private:
  SaltGenerator saltGenerator_;
};

}  // namespace shuati::auth
