#include "shuati/auth/password_hasher.h"

#include <crypt.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "shuati/common/crypto.h"

namespace shuati::auth {
namespace {

constexpr const char* kLegacyPrefix = "sha256";
constexpr const char* kBcryptPrefix = "$2b$";
constexpr int kBcryptCost = 10;
constexpr char kBcryptAlphabet[] =
    "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

bool splitHash(const std::string& encoded,
               std::string& salt,
               std::string& digest) {
  const auto first = encoded.find('$');
  if (first == std::string::npos ||
      encoded.substr(0, first) != kLegacyPrefix) {
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

std::string bcryptBase64(const std::string& seed) {
  std::array<unsigned char, 16> bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = i < seed.size() ? static_cast<unsigned char>(seed[i])
                               : static_cast<unsigned char>(i * 17 + 31);
  }

  std::string encoded;
  encoded.reserve(22);
  std::uint32_t buffer = 0;
  int bits = 0;
  for (const auto byte : bytes) {
    buffer |= static_cast<std::uint32_t>(byte) << bits;
    bits += 8;
    while (bits >= 6 && encoded.size() < 22) {
      encoded.push_back(kBcryptAlphabet[buffer & 0x3fU]);
      buffer >>= 6;
      bits -= 6;
    }
  }
  if (bits > 0 && encoded.size() < 22) {
    encoded.push_back(kBcryptAlphabet[buffer & 0x3fU]);
  }
  while (encoded.size() < 22) {
    encoded.push_back('.');
  }
  return encoded;
}

std::string bcryptSalt(const std::string& seed) {
  return std::string(kBcryptPrefix) + std::to_string(kBcryptCost) + "$" +
         bcryptBase64(seed);
}

std::string cryptPassword(const std::string& password, const std::string& salt) {
  crypt_data data{};
  data.initialized = 0;
  const auto hashed = crypt_r(password.c_str(), salt.c_str(), &data);
  return hashed == nullptr ? std::string{} : std::string(hashed);
}

bool constantTimeEquals(const std::string& left, const std::string& right) {
  if (left.size() != right.size()) {
    return false;
  }
  unsigned char diff = 0;
  for (std::size_t i = 0; i < left.size(); ++i) {
    diff |= static_cast<unsigned char>(left[i] ^ right[i]);
  }
  return diff == 0;
}

}  // namespace

PasswordHasher::PasswordHasher()
    : saltGenerator_([] { return shuati::common::randomBytes(16); }) {}

PasswordHasher::PasswordHasher(SaltGenerator saltGenerator)
    : saltGenerator_(std::move(saltGenerator)) {}

std::string PasswordHasher::hashPassword(const std::string& password) const {
  const auto salt = bcryptSalt(saltGenerator_());
  const auto hashed = cryptPassword(password, salt);
  if (hashed.rfind(kBcryptPrefix, 0) != 0) {
    const auto fallbackSalt = shuati::common::randomHex(16);
    const auto digest = shuati::common::sha256Hex(fallbackSalt + ":" + password);
    return std::string(kLegacyPrefix) + "$" + fallbackSalt + "$" + digest;
  }
  return hashed;
}

bool PasswordHasher::verifyPassword(const std::string& password,
                                    const std::string& encodedHash) const {
  if (encodedHash.rfind(kBcryptPrefix, 0) == 0) {
    const auto hashed = cryptPassword(password, encodedHash);
    return !hashed.empty() && constantTimeEquals(hashed, encodedHash);
  }

  std::string salt;
  std::string digest;
  if (!splitHash(encodedHash, salt, digest)) {
    return false;
  }
  return constantTimeEquals(shuati::common::sha256Hex(salt + ":" + password),
                            digest);
}

}  // namespace shuati::auth
