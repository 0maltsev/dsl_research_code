#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_resolve(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  const auto resolved = resolve_module(module);
  BOUNDFIN_CHECK(resolved.ok);
  return module;
}

// T-Prod/Proj: "e_i:tau_i, product type; 1<=j<=k" -> "tuple: prod(tau_i); projection:tau_j".
void TProd_result_type_has_each_components_type_in_order() {
  auto module =
      parse_and_resolve("fn f(): prod<i32, bool> = (i32bits(0x00000001), true);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &type = *module.functions[0].body->inferred_type;
  BOUNDFIN_CHECK(type->kind == TypeKind::Product);
  const auto &product_type = std::get<ProductType>(type->data);
  BOUNDFIN_CHECK_EQ(product_type.components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(product_type.components[0]->kind == TypeKind::I32);
  BOUNDFIN_CHECK(product_type.components[1]->kind == TypeKind::Bool);
}

void TProj_result_type_is_the_jth_component_type() {
  auto module = parse_and_resolve(
      "fn f(): bool = proj<2>((i32bits(0x00000001), true));\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::Bool);
}

void TYP006_proj_rejects_non_product_operand() {
  auto module = parse_and_resolve("fn f(): i32 = proj<1>(i32bits(0x00000001));\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP006"));
}

void TYP007_proj_index_outside_arity_is_rejected() {
  auto module = parse_and_resolve(
      "fn f(): i32 = proj<3>((i32bits(0x00000001), true));\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP007"));
}

void boundfin_typecheck_products() {
  TProd_result_type_has_each_components_type_in_order();
  TProj_result_type_is_the_jth_component_type();
  TYP006_proj_rejects_non_product_operand();
  TYP007_proj_index_outside_arity_is_rejected();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_products)
