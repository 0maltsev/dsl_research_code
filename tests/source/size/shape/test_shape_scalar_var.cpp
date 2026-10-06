#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::size::shape::ShapeContext;
using boundfin::source::size::shape::ShapeKind;
using boundfin::source::size::shape::scalar_shape;
using boundfin::source::size::shape::shape_of_var;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_typecheck(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  BOUNDFIN_CHECK(resolve_module(module).ok);
  BOUNDFIN_CHECK(typecheck_module(module).ok);
  return module;
}

// Table 8's "scalar/variable" row, first half (main.pdf p.11): "scalar
// literal/primitive" -> "scalar" -- every Table 5 primitive returns a
// scalar type, so this holds unconditionally, with no premise to check.
void scalar_shape_is_always_the_scalar_kind() {
  const auto shape = scalar_shape();
  BOUNDFIN_CHECK(shape != nullptr);
  BOUNDFIN_CHECK(shape->kind == ShapeKind::Scalar);
}

// scalar_shape() returns the same shared instance every call -- Scalar
// carries no data, so there is nothing to distinguish between calls, and
// callers that compare by pointer identity (a plausible future
// optimization, e.g. a join operation short-circuiting on
// `a.get() == b.get()`) get a stable answer.
void scalar_shape_is_a_stable_shared_instance() {
  BOUNDFIN_CHECK(scalar_shape().get() == scalar_shape().get());
}

// Table 8's "scalar/variable" row, second half: "Gamma_sz(x)=kappa" -- a
// variable's shape is whatever the caller-supplied ShapeContext already
// has recorded for its resolved binding.
void shape_of_var_returns_the_contexts_recorded_shape() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = n;\nexport f;\n");
  const auto &var_expr = std::get<VarExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(var_expr.resolved_binding.has_value());

  ShapeContext context;
  context[*var_expr.resolved_binding] = scalar_shape();

  const auto outcome = shape_of_var(var_expr, module.functions[0].body->span, context);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK((*outcome.result)->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK((*outcome.result).get() == scalar_shape().get());
}

// INT001: a resolved, in-scope variable with no recorded shape in
// `context` is a caller precondition violation (shape.hpp's documented
// trust boundary), not a true fact about this program -- unlike Table
// 6's IndexContext, where "not an index binder" is a legitimate,
// permanent SIZ003 rejection, every Table 8 row gives a resolved
// variable a shape, so a missing entry is always a defect in whatever
// populated `context`, never user source rejection
// (diagnostics-and-status.md's INTxxx catalogue).
void shape_of_var_rejects_with_INT001_when_the_binding_is_absent_from_context() {
  auto module = parse_and_typecheck("fn f(n: i32): i32 = n;\nexport f;\n");
  const auto &var_expr = std::get<VarExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(var_expr.resolved_binding.has_value());

  const ShapeContext empty_context; // deliberately missing the binding
  const auto outcome = shape_of_var(var_expr, module.functions[0].body->span, empty_context);
  BOUNDFIN_CHECK(!outcome.ok);
  BOUNDFIN_CHECK_EQ(outcome.diagnostic->code, std::string("INT001"));
}

// Two distinct variables resolve to two distinct shapes when `context`
// records different shapes for their distinct BindingIds -- confirms
// the lookup is genuinely per-binding, not e.g. always returning the
// first/only entry.
void shape_of_var_distinguishes_two_distinct_bindings_in_one_context() {
  auto module = parse_and_typecheck("fn f(n: i32, m: i32): i32 = n + m;\nexport f;\n");
  const auto &binary = std::get<BinaryPrimitiveExpr>(module.functions[0].body->data);
  const auto &n_ref = std::get<VarExpr>(binary.lhs->data);
  const auto &m_ref = std::get<VarExpr>(binary.rhs->data);
  BOUNDFIN_CHECK(n_ref.resolved_binding.has_value());
  BOUNDFIN_CHECK(m_ref.resolved_binding.has_value());
  BOUNDFIN_CHECK(*n_ref.resolved_binding != *m_ref.resolved_binding);

  auto array_like = std::make_shared<boundfin::source::size::shape::Shape>();
  array_like->kind = ShapeKind::Array;
  boundfin::source::size::shape::ArrayShape array_data;
  array_data.exact = std::nullopt;
  array_data.upper = boundfin::source::size::certificate::make_literal(4);
  array_data.capacity = 4;
  array_data.element = scalar_shape();
  array_like->data = array_data;

  ShapeContext context;
  context[*n_ref.resolved_binding] = scalar_shape();
  context[*m_ref.resolved_binding] = array_like;

  const auto n_outcome = shape_of_var(n_ref, binary.lhs->span, context);
  const auto m_outcome = shape_of_var(m_ref, binary.rhs->span, context);
  BOUNDFIN_CHECK(n_outcome.ok);
  BOUNDFIN_CHECK(m_outcome.ok);
  BOUNDFIN_CHECK((*n_outcome.result)->kind == ShapeKind::Scalar);
  BOUNDFIN_CHECK((*m_outcome.result)->kind == ShapeKind::Array);
}

void boundfin_shape_scalar_var() {
  scalar_shape_is_always_the_scalar_kind();
  scalar_shape_is_a_stable_shared_instance();
  shape_of_var_returns_the_contexts_recorded_shape();
  shape_of_var_rejects_with_INT001_when_the_binding_is_absent_from_context();
  shape_of_var_distinguishes_two_distinct_bindings_in_one_context();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_shape_scalar_var)
