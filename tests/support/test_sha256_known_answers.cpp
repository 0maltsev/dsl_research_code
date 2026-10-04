// SHA-256 known-answer tests. Expected digests were independently computed
// on this host via `sha256sum` (GNU coreutils), not transcribed from memory,
// and cross-checked here as 64 lowercase hex characters each.
#include "boundfin/support/sha256.hpp"
#include "boundfin_test.hpp"

#include <cstddef>
#include <span>
#include <string>

namespace {

void SHA256_known_answers() {
  using boundfin::support::sha256_hex;

  BOUNDFIN_CHECK_EQ(sha256_hex(std::string("")),
                     std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  BOUNDFIN_CHECK_EQ(sha256_hex(std::string("abc")),
                     std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  BOUNDFIN_CHECK_EQ(sha256_hex(std::string("The quick brown fox jumps over the lazy dog")),
                     std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
  BOUNDFIN_CHECK_EQ(sha256_hex(std::string(1000000, 'a')),
                     std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));

  // std::span<const std::byte> overload must agree with the std::string overload.
  const std::string abc = "abc";
  const auto *abc_bytes = reinterpret_cast<const std::byte *>(abc.data());
  BOUNDFIN_CHECK_EQ(sha256_hex(std::span<const std::byte>(abc_bytes, abc.size())),
                     std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

}  // namespace

BOUNDFIN_TEST_MAIN(SHA256_known_answers)
