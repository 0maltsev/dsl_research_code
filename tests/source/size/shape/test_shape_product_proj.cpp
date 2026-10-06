#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::shape_of_product;
using boundfin::source::size::shape::shape_of_proj;
using boundfin::source::size::shape::ShapePtr;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

ShapePtr array_like_shape() {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape array_data;
  array_data.exact = std::nullopt;
  array_data.upper = boundfin::source::size::certificate::make_literal(4);
  array_data.capacity = 4;
  array_data.element = scalar_shape();
  shape->data = array_data;
  return shape;
}

// Table 8's "product/proj" row, first half: "component shapes kappa_i" ->
// "prod(kappa-bar)" -- components are collected in declared order.
void shape_of_product_collects_component_shapes_in_declared_order() {
  const auto second = array_like_shape();
  const auto product = shape_of_product({scalar_shape(), second});
  BOUNDFIN_CHECK(product->kind == ShapeKind::Product);
  const auto &components = std::get<boundfin::source::size::shape::ProductShape>(product->data).components;
  BOUNDFIN_CHECK_EQ(components.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(components[0]->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK(components[1]->kind == ShapeKind::Array);
  BOUNDFIN_CHECK(components[1].get() == second.get());
}

// Table 8's "product/proj" row, second half: "valid field j" -> "kappa_j"
// -- a 1-based projection selects the matching component, confirmed
// against a real ast::ProjExpr's own `index` field (not a hand-chosen
// literal), and confirmed to distinguish first vs. second (not always
// returning the same component).
void shape_of_proj_selects_the_matching_one_based_component() {
  auto module = parse_and_typecheck("fn f(n: i32, m: i32): i32 = proj<1>((n, m));\nexport f;\n");
  const auto &proj_expr = std::get<ProjExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK_EQ(proj_expr.index, static_cast<std::uint32_t>(1));

  const auto second = array_like_shape();
  const auto product = shape_of_product({scalar_shape(), second});

  const auto first_outcome = shape_of_proj(product, proj_expr.index, module.functions[0].body->span);
  BOUNDFIN_CHECK(first_outcome.ok);
  BOUNDFIN_CHECK((*first_outcome.result)->kind == ShapeKind::Scalar);

  const auto second_outcome = shape_of_proj(product, 2, module.functions[0].body->span);
  BOUNDFIN_CHECK(second_outcome.ok);
  BOUNDFIN_CHECK((*second_outcome.result)->kind == ShapeKind::Array);
  BOUNDFIN_CHECK((*second_outcome.result).get() == second.get());
}

// INT001: a non-Product operand is a caller precondition violation
// (Phase 3.2's TYP006 already rejects this before shape derivation
// runs), not a SIZ-coded rejection of this program.
void shape_of_proj_rejects_with_INT001_for_a_non_product_operand() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = n;\nexport f;\n");
  const auto outcome = shape_of_proj(scalar_shape(), 1, module.functions[0].body->span);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null operand shape is likewise a caller precondition
// violation -- the null-pointer half of shape_of_proj's defensive
// `!operand_shape || ...` check, distinct from the non-null-but-wrong-
// kind case above.
void shape_of_proj_rejects_with_INT001_for_a_null_operand() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = n;\nexport f;\n");
  const auto outcome = shape_of_proj(nullptr, 1, module.functions[0].body->span);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: an out-of-range index (both above and below the valid
// 1-based range) is likewise a caller precondition violation (Phase
// 3.2's TYP007 already rejects this before shape derivation runs).
void shape_of_proj_rejects_with_INT001_for_an_out_of_range_index() {
  auto module = parse_and_typecheck("fn f(n: i32, m: i32): i32 = n;\nexport f;\n");
  const auto product = shape_of_product({scalar_shape(), scalar_shape()});

  const auto too_high = shape_of_proj(product, 3, module.functions[0].body->span);
  BOUNDFIN_CHECK(!too_high.ok);
  BOUNDFIN_CHECK_EQ(too_high.diagnostic->code, std::string("INT001"));

  const auto too_low = shape_of_proj(product, 0, module.functions[0].body->span);
  BOUNDFIN_CHECK(!too_low.ok);
  BOUNDFIN_CHECK_EQ(too_low.diagnostic->code, std::string("INT001"));
}

void boundfin_shape_product_proj() {
  shape_of_product_collects_component_shapes_in_declared_order();
  shape_of_proj_selects_the_matching_one_based_component();
  shape_of_proj_rejects_with_INT001_for_a_non_product_operand();
  shape_of_proj_rejects_with_INT001_for_a_null_operand();
  shape_of_proj_rejects_with_INT001_for_an_out_of_range_index();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_product_proj)
