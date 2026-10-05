#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::Module;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;

Module parse_ok(const std::string &source) {
  auto result = parse_module(source);
  BOUNDFIN_CHECK(result.ok);
  return std::move(*result.module);
}

void export_resolves_to_the_named_functions_rank() {
  auto module = parse_ok("fn f(): i32 = i32bits(0x00000001);\n"
                          "fn g(): i32 = i32bits(0x00000002);\n"
                          "export g;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(module.export_decl.resolved_target_rank.has_value());
  BOUNDFIN_CHECK_EQ(*module.export_decl.resolved_target_rank, std::size_t{1}); // g's rank
}

void NAM005_export_names_no_declaration() {
  auto module = parse_ok("fn f(): i32 = i32bits(0x00000001);\nexport g;\n");
  const auto result = resolve_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("NAM005"));
}

void boundfin_resolve_export() {
  export_resolves_to_the_named_functions_rank();
  NAM005_export_names_no_declaration();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_resolve_export)
