#include "boundfin/support/sha256.hpp"

#include <openssl/evp.h>

#include <array>
#include <fstream>
#include <stdexcept>

namespace boundfin::support {

namespace {

std::string to_lower_hex(std::span<const unsigned char> digest) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(digest.size() * 2);
  for (std::size_t i = 0; i < digest.size(); ++i) {
    out[2 * i] = kHex[(digest[i] >> 4) & 0x0F];
    out[2 * i + 1] = kHex[digest[i] & 0x0F];
  }
  return out;
}

class Sha256Context {
public:
  Sha256Context() : ctx_(EVP_MD_CTX_new()) {
    if (ctx_ == nullptr || EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) != 1) {
      EVP_MD_CTX_free(ctx_);
      throw std::runtime_error("boundfin::support: failed to initialize OpenSSL SHA-256 context");
    }
  }

  Sha256Context(const Sha256Context &) = delete;
  Sha256Context &operator=(const Sha256Context &) = delete;

  ~Sha256Context() { EVP_MD_CTX_free(ctx_); }

  void update(const void *data, std::size_t size) {
    if (size > 0 && EVP_DigestUpdate(ctx_, data, size) != 1) {
      throw std::runtime_error("boundfin::support: OpenSSL SHA-256 update failed");
    }
  }

  std::string finalize_hex() {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx_, digest.data(), &digest_len) != 1 || digest_len != 32) {
      throw std::runtime_error("boundfin::support: OpenSSL SHA-256 finalize failed");
    }
    return to_lower_hex(std::span<const unsigned char>(digest.data(), digest_len));
  }

private:
  EVP_MD_CTX *ctx_;
};

} // namespace

std::string sha256_hex(std::span<const std::byte> bytes) {
  Sha256Context ctx;
  ctx.update(bytes.data(), bytes.size());
  return ctx.finalize_hex();
}

std::string sha256_hex(const std::string &text) {
  Sha256Context ctx;
  ctx.update(text.data(), text.size());
  return ctx.finalize_hex();
}

std::string sha256_hex_of_file(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::filesystem::filesystem_error("boundfin::support: cannot open file for hashing", path,
                                             std::make_error_code(std::errc::no_such_file_or_directory));
  }

  Sha256Context ctx;
  std::array<char, 1 << 16> buffer{};
  while (file) {
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto read_count = static_cast<std::size_t>(file.gcount());
    if (read_count == 0) {
      break;
    }
    ctx.update(buffer.data(), read_count);
  }
  return ctx.finalize_hex();
}

} // namespace boundfin::support
