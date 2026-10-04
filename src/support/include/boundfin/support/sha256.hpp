#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

namespace boundfin::support {

// Lowercase 64-hex-digit SHA-256 digest, matching the `sha256` pattern used
// throughout schemas/*.schema.json (`^[0-9a-f]{64}$`). Computed via OpenSSL's
// EVP digest interface rather than a from-scratch implementation.
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);

// Convenience overload for a string's bytes.
[[nodiscard]] std::string sha256_hex(const std::string &text);

// Digest of a file's bytes, streamed rather than loaded whole.
// Throws std::filesystem::filesystem_error if the file cannot be opened.
[[nodiscard]] std::string sha256_hex_of_file(const std::filesystem::path &path);

} // namespace boundfin::support
