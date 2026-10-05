#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;

void module_with_parameters_and_scalar_types() {
  const auto result = parse_module("fn f(a: bool, b: i32, c: i64, d: f64): bool = a;\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &module = *result.module;
  BOUNDFIN_CHECK_EQ(module.functions.size(), std::size_t{1});
  const auto &fn = module.functions[0];
  BOUNDFIN_CHECK_EQ(fn.name, std::string("f"));
  BOUNDFIN_CHECK_EQ(fn.parameters.size(), std::size_t{4});
  BOUNDFIN_CHECK(fn.parameters[0].type->kind == TypeKind::Bool);
  BOUNDFIN_CHECK(fn.parameters[1].type->kind == TypeKind::I32);
  BOUNDFIN_CHECK(fn.parameters[2].type->kind == TypeKind::I64);
  BOUNDFIN_CHECK(fn.parameters[3].type->kind == TypeKind::F64);
  BOUNDFIN_CHECK(fn.result_type->kind == TypeKind::Bool);
  BOUNDFIN_CHECK_EQ(module.export_decl.name, std::string("f"));
}

void nullary_parameter_list() {
  const auto result = parse_module("fn f(): i32 = i32bits(0x00000001);\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.module->functions[0].parameters.empty());
}

void array_and_product_types() {
  const auto result =
      parse_module("fn f(xs: arr<i32, 4>, p: prod<i32, i64, f64>): bool = true;\nexport f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &fn = result.module->functions[0];
  BOUNDFIN_CHECK(fn.parameters[0].type->kind == TypeKind::Array);
  const auto &array_type = std::get<ArrayType>(fn.parameters[0].type->data);
  BOUNDFIN_CHECK(array_type.element->kind == TypeKind::I32);
  BOUNDFIN_CHECK_EQ(array_type.capacity, std::uint32_t{4});

  BOUNDFIN_CHECK(fn.parameters[1].type->kind == TypeKind::Product);
  const auto &product_type = std::get<ProductType>(fn.parameters[1].type->data);
  BOUNDFIN_CHECK_EQ(product_type.components.size(), std::size_t{3});
}

void multiple_function_declarations_in_order() {
  const auto result = parse_module("fn f(): i32 = i32bits(0x00000001);\n"
                                    "fn g(): i32 = i32bits(0x00000002);\n"
                                    "export f;\n");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.module->functions.size(), std::size_t{2});
  BOUNDFIN_CHECK_EQ(result.module->functions[0].name, std::string("f"));
  BOUNDFIN_CHECK_EQ(result.module->functions[1].name, std::string("g"));
}

void SYN005_malformed_function_declaration() {
  // Missing ":" before the result type.
  {
    const auto result = parse_module("fn f() i32 = i32bits(0x00000001);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN005"));
  }
  // Missing ";" terminating the declaration.
  {
    const auto result = parse_module("fn f(): i32 = i32bits(0x00000001)\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN005"));
  }
  // Malformed parameter: missing ":" before the type.
  {
    const auto result = parse_module("fn f(x i32): i32 = x;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN005"));
  }
}

void SYN007_export_missing_or_trailing() {
  // No export declaration at all.
  {
    const auto result = parse_module("fn f(): i32 = i32bits(0x00000001);\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN007"));
  }
  // Source follows the export declaration.
  {
    const auto result = parse_module("fn f(): i32 = i32bits(0x00000001);\n"
                                      "export f;\n"
                                      "fn g(): i32 = i32bits(0x00000002);\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN007"));
  }
  // Export names a token, not an identifier at all.
  {
    const auto result = parse_module("fn f(): i32 = i32bits(0x00000001);\nexport;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN007"));
  }
}

void boundfin_parse_declarations_types_and_module() {
  module_with_parameters_and_scalar_types();
  nullary_parameter_list();
  array_and_product_types();
  multiple_function_declarations_in_order();
  SYN005_malformed_function_declaration();
  SYN007_export_missing_or_trailing();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_declarations_types_and_module)
