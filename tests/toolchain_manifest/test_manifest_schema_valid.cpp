#include "boundfin/support/record_schema.hpp"
#include "boundfin/toolchain_manifest/manifest.hpp"
#include "boundfin_test.hpp"

#include <filesystem>
#include <iostream>

namespace {

void toolchain_manifest_output_validates_against_schema() {
  const auto manifest = boundfin::toolchain_manifest::build_default_manifest(
      BOUNDFIN_CXX_COMPILER, BOUNDFIN_OPENSSL_VERSION, BOUNDFIN_OPENSSL_CRYPTO_LIBRARY, BOUNDFIN_REPO_ROOT);

  const std::filesystem::path schema_path =
      std::filesystem::path(BOUNDFIN_SCHEMAS_DIR) / "compiler-certificate.schema.json";
  const auto result =
      boundfin::support::validate_instance_against_schema_fragment(schema_path, "/properties/toolchain_versions",
                                                                     manifest);

  if (!result.valid()) {
    for (const auto &error : result.errors) {
      std::cerr << error.instance_pointer << ": " << error.message << "\n";
    }
  }
  BOUNDFIN_CHECK(result.valid());
}

} // namespace

BOUNDFIN_TEST_MAIN(toolchain_manifest_output_validates_against_schema)
