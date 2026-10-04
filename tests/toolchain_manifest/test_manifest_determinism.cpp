#include "boundfin/toolchain_manifest/manifest.hpp"
#include "boundfin_test.hpp"

namespace {

void toolchain_manifest_emit_is_byte_identical_across_runs() {
  const auto first = boundfin::toolchain_manifest::build_default_manifest(
      BOUNDFIN_CXX_COMPILER, BOUNDFIN_OPENSSL_VERSION, BOUNDFIN_OPENSSL_CRYPTO_LIBRARY, BOUNDFIN_REPO_ROOT);
  const auto second = boundfin::toolchain_manifest::build_default_manifest(
      BOUNDFIN_CXX_COMPILER, BOUNDFIN_OPENSSL_VERSION, BOUNDFIN_OPENSSL_CRYPTO_LIBRARY, BOUNDFIN_REPO_ROOT);

  BOUNDFIN_CHECK_EQ(first.dump(2), second.dump(2));
  // nlohmann::json's default object_t sorts keys, so determinism does not
  // depend on the probe order inside build_default_manifest.
  BOUNDFIN_CHECK(first.contains("cmake"));
  BOUNDFIN_CHECK(first.contains("ninja"));
  BOUNDFIN_CHECK(first.contains("cxx_compiler"));
  BOUNDFIN_CHECK(first.contains("openssl_crypto"));
}

} // namespace

BOUNDFIN_TEST_MAIN(toolchain_manifest_emit_is_byte_identical_across_runs)
