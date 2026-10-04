#include "boundfin/support/record_schema.hpp"
#include "boundfin_test.hpp"

#include <filesystem>

namespace {

std::filesystem::path compiler_certificate_schema_path() {
  return std::filesystem::path(BOUNDFIN_SCHEMAS_DIR) / "compiler-certificate.schema.json";
}

void record_schema_toolchain_versions_subschema() {
  using boundfin::support::validate_instance_against_schema_fragment;

  const nlohmann::json valid_instance = {
      {"cmake", {{"version", "4.3.3"}, {"sha256", std::string(64, 'a')}}},
  };
  const auto valid_result =
      validate_instance_against_schema_fragment(compiler_certificate_schema_path(), "/properties/toolchain_versions",
                                                 valid_instance);
  BOUNDFIN_CHECK(valid_result.valid());

  // Negative: sha256 does not match the schema's 64-lowercase-hex pattern.
  const nlohmann::json wrong_hash_shape = {
      {"cmake", {{"version", "4.3.3"}, {"sha256", std::string("not-a-hash")}}},
  };
  const auto wrong_hash_result = validate_instance_against_schema_fragment(
      compiler_certificate_schema_path(), "/properties/toolchain_versions", wrong_hash_shape);
  BOUNDFIN_CHECK(!wrong_hash_result.valid());

  // Negative: missing required "sha256" field.
  const nlohmann::json missing_field = {
      {"cmake", {{"version", "4.3.3"}}},
  };
  const auto missing_field_result = validate_instance_against_schema_fragment(
      compiler_certificate_schema_path(), "/properties/toolchain_versions", missing_field);
  BOUNDFIN_CHECK(!missing_field_result.valid());

  // Negative: the subschema requires minProperties: 1.
  const nlohmann::json empty_instance = nlohmann::json::object();
  const auto empty_result = validate_instance_against_schema_fragment(
      compiler_certificate_schema_path(), "/properties/toolchain_versions", empty_instance);
  BOUNDFIN_CHECK(!empty_result.valid());

  // Negative: tool name does not match propertyNames pattern "^[A-Za-z][A-Za-z0-9_.-]*$".
  const nlohmann::json bad_name = {
      {"1cmake", {{"version", "4.3.3"}, {"sha256", std::string(64, 'a')}}},
  };
  const auto bad_name_result = validate_instance_against_schema_fragment(
      compiler_certificate_schema_path(), "/properties/toolchain_versions", bad_name);
  BOUNDFIN_CHECK(!bad_name_result.valid());
}

} // namespace

BOUNDFIN_TEST_MAIN(record_schema_toolchain_versions_subschema)
