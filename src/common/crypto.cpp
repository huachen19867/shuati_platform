#include "shuati/common/crypto.h"

#include <openssl/sha.h>

#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace shuati::common {
namespace {

std::string bytesToHex(const unsigned char* bytes, std::size_t size) {
  std::ostringstream out;
  for (std::size_t i = 0; i < size; ++i) {
    out << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(bytes[i]);
  }
  return out.str();
}

}  // namespace

std::string sha256Hex(const std::string& value) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(),
         digest);
  return bytesToHex(digest, SHA256_DIGEST_LENGTH);
}

std::string randomBytes(std::size_t byteCount) {
  if (byteCount == 0) {
    throw std::invalid_argument("random byte count must be positive");
  }

  std::random_device device;
  std::uniform_int_distribution<int> dist(0, 255);
  std::string bytes;
  bytes.resize(byteCount);
  for (auto& ch : bytes) {
    ch = static_cast<char>(dist(device));
  }
  return bytes;
}

std::string randomHex(std::size_t byteCount) {
  const auto bytes = randomBytes(byteCount);
  return bytesToHex(reinterpret_cast<const unsigned char*>(bytes.data()),
                    bytes.size());
}

}  // namespace shuati::common
