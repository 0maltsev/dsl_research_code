#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/count/count.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using namespace boundfin::source::size::certificate;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::count::infer_count;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::Shape;
using boundfin::source::size::shape::ShapeContext;
using boundfin::source::size::shape::ShapeKind;
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

ShapePtr array_shape(std::optional<std::uint64_t> exact_value, std::uint64_t upper_value, std::uint32_t capacity) {
  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = exact_value ? std::optional{make_literal(*exact_value)} : std::nullopt;
  data.upper = make_literal(upper_value);
  data.capacity = capacity;
  data.element = scalar_shape();
  shape->data = data;
  return shape;
}

// C-Len-E: "e has array(s,u,N;kappa-bar)" -> "(s,u;nu)" with nu=s. AM-034:
// exercised standalone via a caller-supplied ShapeContext, scoped to a
// VarExpr operand only, not yet wired to live ast::Expr traversal.
void CLenE_variable_with_an_exact_shape_resolves_via_the_context() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  ShapeContext shape_context;
  shape_context[*param.binding] = array_shape(5, 5, 8);

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_literal(5)));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_literal(5)));
}

// C-Len-U: "e has array(star,u,N;kappa-bar)" -> "(star,u;nu)" with
// 0<=nu<=u.
void CLenU_variable_with_an_inexact_shape_resolves_via_the_context() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  ShapeContext shape_context;
  shape_context[*param.binding] = array_shape(std::nullopt, 8, 8);

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(!outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_literal(8)));
}

// C-Len-E composes with an already-implemented rule (C-Add): "len(xs) +
// len(xs)" combines through infer_add exactly like any other operand,
// mirroring test_count_idx.cpp's identical CIdx-composes-with-CAdd case.
void CLenE_composes_with_CAdd() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs) + len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  ShapeContext shape_context;
  shape_context[*param.binding] = array_shape(3, 3, 8);

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.result->exact.has_value());
  BOUNDFIN_CHECK(terms_equal(**outcome.result->exact, *make_add(make_literal(3), make_literal(3))));
  BOUNDFIN_CHECK(terms_equal(*outcome.result->upper, *make_add(make_literal(3), make_literal(3))));
}

// AM-034: this slice is scoped to a VarExpr operand only -- len of a
// non-variable operand (here, an array literal) has no accepted
// count-refinement rule yet, deferred to a later live-wiring slice.
void SIZ003_len_of_a_non_variable_operand_is_rejected() {
  auto module = parse_and_typecheck(
      "fn f(): i32 = len(array<2>[i32bits(0x00000001), i32bits(0x00000002)]);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// Without a supplied shape_context (the default, empty one), a variable
// operand has no accepted count-refinement rule either: the array's own
// shape is established only by the context a caller supplies, never
// re-derived from the AST itself in this slice (no infer_shape dispatcher
// exists yet).
void SIZ003_len_of_a_variable_without_a_recorded_shape_is_rejected() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto outcome = infer_count(*module.functions[0].body); // no shape_context argument: defaults to empty
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("SIZ003"));
}

// INT001: a shape_context entry of the wrong kind (here, Scalar instead
// of Array) for the len operand's own binding is a caller precondition
// violation -- Phase 3.2's TYP009 already rejects a non-array len operand
// before shape/count derivation ever runs, mirroring shape_of_len's own
// identical trust-boundary reasoning.
void INT001_len_operand_shape_of_the_wrong_kind_is_rejected() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  ShapeContext shape_context;
  shape_context[*param.binding] = scalar_shape();

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a present-but-null exact term inside an otherwise-well-formed
// array shape is a caller precondition violation, distinct from the
// legitimately-inexact "absent" case above -- mirrors shape_of_builder's
// own AM-032-driven distinction.
void INT001_len_operand_shape_with_a_present_but_null_exact_term_is_rejected() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = std::optional<TermPtr>{nullptr};
  data.upper = make_literal(8);
  data.capacity = 8;
  data.element = scalar_shape();
  shape->data = data;

  ShapeContext shape_context;
  shape_context[*param.binding] = shape;

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// INT001: a null upper term is likewise a caller precondition violation,
// isolated from the present-but-null-exact case above by keeping `exact`
// present and valid here.
void INT001_len_operand_shape_with_a_null_upper_term_is_rejected() {
  auto module = parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = len(xs);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  auto shape = std::make_shared<Shape>();
  shape->kind = ShapeKind::Array;
  ArrayShape data;
  data.exact = make_literal(8);
  data.upper = nullptr;
  data.capacity = 8;
  data.element = scalar_shape();
  shape->data = data;

  ShapeContext shape_context;
  shape_context[*param.binding] = shape;

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// shape_context threads through infer_if (not just infer_add): a len(xs)
// reference in one branch resolves against the same caller-supplied
// context as a top-level reference would.
void CLenE_shape_context_threads_through_infer_if() {
  auto module =
      parse_and_typecheck("fn f(xs: arr<i32, 8>): i32 = if true then len(xs) else i32bits(0x00000000);\nexport f;\n");
  const auto &param = module.functions[0].parameters[0];
  BOUNDFIN_CHECK(param.binding.has_value());

  ShapeContext shape_context;
  shape_context[*param.binding] = array_shape(4, 4, 8);

  const auto outcome = infer_count(*module.functions[0].body, {}, shape_context);
  BOUNDFIN_CHECK(outcome.ok);
}

void boundfin_count_len() {
  CLenE_variable_with_an_exact_shape_resolves_via_the_context();
  CLenU_variable_with_an_inexact_shape_resolves_via_the_context();
  CLenE_composes_with_CAdd();
  CLenE_shape_context_threads_through_infer_if();
  SIZ003_len_of_a_non_variable_operand_is_rejected();
  SIZ003_len_of_a_variable_without_a_recorded_shape_is_rejected();
  INT001_len_operand_shape_of_the_wrong_kind_is_rejected();
  INT001_len_operand_shape_with_a_present_but_null_exact_term_is_rejected();
  INT001_len_operand_shape_with_a_null_upper_term_is_rejected();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_len)
