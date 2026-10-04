#include "boundfin/support/record_schema.hpp"

#include <nlohmann/json-schema.hpp>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace boundfin::support {

namespace {

nlohmann::json load_json_file(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file) {
    throw std::filesystem::filesystem_error("boundfin::support: cannot open schema file", path,
                                             std::make_error_code(std::errc::no_such_file_or_directory));
  }
  nlohmann::json document;
  file >> document;
  return document;
}

class CollectingErrorHandler final : public nlohmann::json_schema::error_handler {
public:
  std::vector<SchemaValidationError> errors;

  void error(const nlohmann::json::json_pointer &pointer, const nlohmann::json & /*instance*/,
             const std::string &message) override {
    errors.push_back(SchemaValidationError{pointer.to_string(), message});
  }
};

} // namespace

SchemaValidationResult validate_instance_against_schema_fragment(
    const std::filesystem::path &schema_path, const std::string &schema_pointer,
    const nlohmann::json &instance) {
  nlohmann::json document = load_json_file(schema_path);

  const auto id_it = document.find("$id");
  if (id_it == document.end() || !id_it->is_string()) {
    throw std::invalid_argument("boundfin::support: schema document has no string \"$id\": " +
                                 schema_path.string());
  }
  const std::string schema_id = id_it->get<std::string>();

  // The document may use "format" elsewhere (e.g. "date-time" on a
  // timestamp field) even when the targeted subschema does not; the
  // library's own default_string_format_check must be supplied or it
  // refuses to validate any instance against a schema containing "format"
  // anywhere in its compiled tree.
  nlohmann::json_schema::json_validator validator(nullptr, nlohmann::json_schema::default_string_format_check);
  validator.set_root_schema(document);

  CollectingErrorHandler handler;
  const std::string fragment = "#" + schema_pointer;
  const nlohmann::json_uri target_uri = nlohmann::json_uri(schema_id).derive(fragment);
  validator.validate(instance, handler, target_uri);

  return SchemaValidationResult{std::move(handler.errors)};
}

} // namespace boundfin::support
