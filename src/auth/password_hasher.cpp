#include "shuati/auth/password_hasher.h"

#include <string>

#include "shuati/common/crypto.h"

namespace shuati::auth {
namespace {

constexpr const char* kPrefix = "sha256";

bool splitHash(const std::string& encoded,
               std::string& salt,
               std::string& digest) {
  const auto first = encoded.find('$');
  if (first == std::string::npos || encoded.substr(0, first) != kPrefix) {
    return false;
  }
  const auto second = encoded.find('$', first + 1);
  if (second == std::string::npos) {
    return false;
  }
  salt = encoded.substr(first + 1, second - first - 1);
  digest = encoded.substr(second + 1);
  return !salt.empty() && !digest.empty();
}

}  // namespace

Sha256PasswordHasher::Sha256PasswordHasher()
    : saltGenerator_([] { return shuati::common::randomHex(16); }) {}

Sha256PasswordHasher::Sha256PasswordHasher(SaltGenerator saltGenerator)
    : saltGenerator_(std::move(saltGenerator)) {}

std::string Sha256PasswordHasher::hashPassword(
    const std::string& password) const {
  const auto salt = saltGenerator_();
  const auto digest = shuati::common::sha256Hex(salt + ":" + password);
  return std::string(kPrefix) + "$" + salt + "$" + digest;
}

bool Sha256PasswordHasher::verifyPassword(
    const std::string& password,
    const std::string& encodedHash) const {
  std::string salt;
  std::string digest;
  if (!splitHash(encodedHash, salt, digest)) {
    return false;
  }
  return shuati::common::sha256Hex(salt + ":" + password) == digest;
}

}  // namespace shuati::auth
