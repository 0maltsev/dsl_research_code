#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin/source/size/infer/infer.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::infer::infer_shape;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::ProductShape;
using boundfin::source::size::shape::ShapeContext;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

const Expr &body(const Module &module) { return *module.functions[0].body; }

// Table 8's "scalar/variable" row, first half: a literal is
// unconditionally scalar.
void infer_shape_of_a_literal_is_scalar() {
  const auto module = parse_and_typecheck("fn f(): i32 = i32bits(0x00000005);\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// A binary primitive is unconditionally scalar too, regardless of its
// operands' own shapes -- the operands are not recursed into at all.
void infer_shape_of_a_binary_primitive_is_scalar() {
  const auto module = parse_and_typecheck("fn f(a: i32, b: i32): i32 = a + b;\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// Table 8's "scalar/variable" row, second half: a variable resolves via
// a caller-supplied ShapeContext.
void infer_shape_of_a_variable_resolves_via_the_context() {
  const auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  auto array_shape = std::make_shared<boundfin::source::size::shape::Shape>();
  array_shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = make_literal(8);
  data.upper = make_literal(8);
  data.capacity = 8;
  data.element = boundfin::source::size::shape::scalar_shape();
  array_shape->data = data;

  ShapeContext shape_context;
  shape_context[*param.binding] = array_shape;

  // infer_shape dispatches Len -> recurses into the array operand (a
  // Var) -> shape_of_var reads the context -> shape_of_len.
  const auto outcome = infer_shape(body(module), shape_context);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar); // len's own Table 8 shape is always scalar
}

// Table 8's "length/index" row, second half: indexing returns the
// array's own element summary. The array operand carries an Array-kind
// element shape specifically (not a scalar), so that recursing into the
// wrong operand (the index value, a scalar literal) would be
// distinguishable: shape_of_index would then see a Scalar `operand_shape`
// and reject with INT001 instead of returning the real element shape.
void infer_shape_of_an_index_expression_recurses_into_the_array_operand() {
  const auto module = parse_and_typecheck("fn f(xs: arr<arr<i32, 4>, 8>): arr<i32, 4> = xs[i32bits(0x00000000)];\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  auto inner_shape = std::make_shared<boundfin::source::size::shape::Shape>();
  inner_shape->kind = ShapeKind::Array;
  ArrayShape inner_data;
  inner_data.exact = make_literal(2);
  inner_data.upper = make_literal(2);
  inner_data.capacity = 4;
  inner_data.element = boundfin::source::size::shape::scalar_shape();
  inner_shape->data = inner_data;

  auto outer_shape = std::make_shared<boundfin::source::size::shape::Shape>();
  outer_shape->kind = ShapeKind::Array;
  ArrayShape outer_data;
  outer_data.exact = make_literal(6);
  outer_data.upper = make_literal(6);
  outer_data.capacity = 8;
  outer_data.element = inner_shape;
  outer_shape->data = outer_data;

  ShapeContext shape_context;
  shape_context[*param.binding] = outer_shape;

  const auto outcome = infer_shape(body(module), shape_context);
  BOUNDFIN_CHECK(outcome.ok);
  // The result is the outer array's own element shape, by pointer
  // identity -- confirms shape_of_index's own element-passthrough design,
  // not a coincidentally-equal independently-built shape.
  BOUNDFIN_CHECK(outcome.result->get() == inner_shape.get());
}

// Product/proj: a product's own shape is the component shapes, in
// order; projection selects the matching one-based field.
void infer_shape_of_a_product_and_its_projection() {
  const auto module = parse_and_typecheck("fn f(): i32 = proj<2>((i32bits(0x00000001), i32bits(0x00000002)));\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar); // proj<2> of (i32,i32) is scalar
}

// An array literal's own shape has lambda=u=m (the literal element
// count), matching the "literal" row exactly.
void infer_shape_of_an_array_literal() {
  const auto module =
      parse_and_typecheck("fn f(): arr<i32, 4> = array<4>[i32bits(0x00000001), i32bits(0x00000002)];\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(2)));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(2)));
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(4));
}

// Conditional: the guard's own shape is not visited; the branches' own
// shapes are joined (here, two scalars, trivially scalar).
void infer_shape_of_a_conditional() {
  const auto module =
      parse_and_typecheck("fn f(): i32 = if true then i32bits(0x00000001) else i32bits(0x00000002);\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// Let: the bound expression's own shape is recorded in the context the
// body is evaluated under.
void infer_shape_of_a_let_binding() {
  const auto module = parse_and_typecheck(
      "fn f(): arr<i32, 2> = let x = array<2>[i32bits(0x00000001), i32bits(0x00000002)] in x;\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(2)));
}

// Fold: AM-033's fixed-point design -- a scalar accumulator that never
// changes shape yields the accumulator's own (scalar) shape.
void infer_shape_of_a_fold_with_a_scalar_accumulator() {
  const auto module =
      parse_and_typecheck("fn f(): i32 = fold<16>(i32bits(0x00000005); acc, idx; i32bits(0x00000000); acc);\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
}

// Build: the count's own (exact,upper) pair, obtained live from
// count::infer_count via check_count_admissibility/infer_count, becomes
// the result array's own (lambda,u); the index binder's own body yields
// a scalar element shape.
void infer_shape_of_a_build() {
  const auto module = parse_and_typecheck("fn f(): arr<i32, 8> = build<8>(i32bits(0x00000005); i; i32bits(0x00000000));\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Array);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**result.exact, *make_literal(5)));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(5)));
  BOUNDFIN_CHECK_EQ(result.capacity, static_cast<std::uint32_t>(8));
  BOUNDFIN_CHECK(result.element->kind == ShapeKind::Scalar);
}

// SIZ005 (Table 6's own count-admissibility failure, via
// check_count_admissibility) genuinely propagates through infer_shape's
// own Build dispatch, not just a Table-8-level failure.
void infer_shape_of_a_build_with_an_over_capacity_count_propagates_SIZ005() {
  const auto module =
      parse_and_typecheck("fn f(): arr<i32, 4> = build<4>(i32bits(0x00000009); i; i32bits(0x00000000));\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// Fold's own count admissibility is checked BEFORE `initial`'s own shape
// is computed, matching E-Fold's left-to-right sequential evaluation
// (main.pdf p.42) and count.cpp's walk_for_admissibility's identical,
// already-shipped order. A spec-auditor review of this slice's first
// version found this reversed; this test reproduces the exact
// counterexample that proved it: count (9) exceeds capacity (2), so a
// count-first order yields SIZ005; `initial` is itself an unimplemented
// CallExpr, which would yield SIZ013 if (wrongly) evaluated first.
void infer_shape_of_a_fold_checks_count_admissibility_before_initial() {
  const auto module = parse_and_typecheck(
      "fn g(): i32 = i32bits(0x00000001);\n"
      "fn f(): i32 = fold<2>(i32bits(0x00000009); acc, idx; g(); acc);\n"
      "export f;\n");
  const auto outcome = infer_shape(*module.functions[1].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ005"));
}

// Documents a disclosed, known limitation (spec-auditor PLAUSIBLE
// finding, this slice): a build/fold whose own `count` expression needs
// C-Len-E/C-Len-U (here, `len(xs)` for an already-shaped `xs`) cannot
// resolve via check_count_admissibility's own internal infer_count call,
// which has no shape_context parameter -- even though shape_context
// already has xs's own entry here, and even though this function's own
// *separate* infer_count call for the builder's own (exact,upper) pair
// DOES receive shape_context. check_count_admissibility runs first and
// rejects with SIZ003 before that second call is ever reached. Fixing
// this needs extending check_count_admissibility's own already-shipped
// signature (src/source/size/count), out of this slice's own scope.
void infer_shape_of_a_build_whose_count_is_len_of_an_already_shaped_variable_is_not_yet_supported() {
  const auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): arr<i32, 8> = build<8>(len(xs); i; i32bits(0x00000000));\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  auto array_shape = std::make_shared<boundfin::source::size::shape::Shape>();
  array_shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = make_literal(5);
  data.upper = make_literal(5);
  data.capacity = 8;
  data.element = boundfin::source::size::shape::scalar_shape();
  array_shape->data = data;

  ShapeContext shape_context;
  shape_context[*param.binding] = array_shape;

  const auto outcome = infer_shape(body(module), shape_context);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// AM-036: a call expression has no implemented dispatch rule yet --
// SIZ013, not SIZ003 (which would wrongly claim no rule exists at all).
void infer_shape_of_a_call_rejects_with_SIZ013() {
  const auto module = parse_and_typecheck(
      "fn g(): i32 = i32bits(0x00000001);\nfn f(): i32 = g();\nexport f;\n");
  const auto outcome = infer_shape(*module.functions[1].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ013"));
}

// A fold nested inside a build's own body threads index_context
// correctly: the inner fold's own count can reference the outer build's
// index binder, confirming extended_index_context genuinely propagates
// into nested recursion, not just one level deep.
void infer_shape_of_a_build_containing_a_nested_fold_referencing_its_index() {
  const auto module = parse_and_typecheck(
      "fn f(): arr<i32, 4> = build<4>(i32bits(0x00000003); i; fold<3>(i; acc, j; i32bits(0x00000000); acc));\nexport f;\n");
  const auto outcome = infer_shape(body(module));
  BOUNDFIN_CHECK(outcome.ok);
  const auto &result = std::get<ArrayShape>((*outcome.result)->data);
  BOUNDFIN_CHECK(result.element->kind == ShapeKind::Scalar);
}

void boundfin_infer_shape() {
  infer_shape_of_a_literal_is_scalar();
  infer_shape_of_a_binary_primitive_is_scalar();
  infer_shape_of_a_variable_resolves_via_the_context();
  infer_shape_of_an_index_expression_recurses_into_the_array_operand();
  infer_shape_of_a_product_and_its_projection();
  infer_shape_of_an_array_literal();
  infer_shape_of_a_conditional();
  infer_shape_of_a_let_binding();
  infer_shape_of_a_fold_with_a_scalar_accumulator();
  infer_shape_of_a_build();
  infer_shape_of_a_build_with_an_over_capacity_count_propagates_SIZ005();
  infer_shape_of_a_fold_checks_count_admissibility_before_initial();
  infer_shape_of_a_build_whose_count_is_len_of_an_already_shaped_variable_is_not_yet_supported();
  infer_shape_of_a_call_rejects_with_SIZ013();
  infer_shape_of_a_build_containing_a_nested_fold_referencing_its_index();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_infer_shape)
