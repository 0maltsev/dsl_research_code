#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace boundfin::support {

struct SchemaValidationError {
  std::string instance_pointer;
  std::string message;
};

struct SchemaValidationResult {
  std::vector<SchemaValidationError> errors;

  [[nodiscard]] bool valid() const { return errors.empty(); }
};

// Validates `instance` against the subschema found at `schema_pointer` (an
// RFC 6901 JSON pointer; "" selects the document root) inside the schema
// document at `schema_path`. Every "$ref" in that document -- including refs
// into its own "$defs" -- is resolved against the document's declared "$id".
// The document must declare a string "$id"; this function does not fetch or
// follow references to any other document.
//
// This performs instance validation against one of this repository's own
// schemas (schemas/*.schema.json), which use only non-recursive Draft
// 2020-12 constructs ($ref, $defs, pattern, oneOf, const, propertyNames,
// additionalProperties, ...). It does not implement $dynamicRef/$dynamicAnchor
// and must not be used to check whether a schema *document* itself conforms
// to the Draft 2020-12 meta-schema; that check is the dev-only tool recorded
// in tools/schema-validate/.
[[nodiscard]] SchemaValidationResult validate_instance_against_schema_fragment(
    const std::filesystem::path &schema_path, const std::string &schema_pointer,
    const nlohmann::json &instance);

} // namespace boundfin::support
