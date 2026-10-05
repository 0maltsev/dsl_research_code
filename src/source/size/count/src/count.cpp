#include "boundfin/source/size/count/count.hpp"

#include <cstdint>

namespace boundfin::source::size::count {

namespace {

using namespace boundfin::source::ast;
using namespace boundfin::source::size::certificate;

CountOutcome fail(std::string code, SourceSpan span, std::string message) {
  return CountOutcome{false, std::nullopt, CountDiagnostic{std::move(code), span, std::move(message)}};
}

CountOutcome ok_with(CountResult result) { return CountOutcome{true, std::move(result), std::nullopt}; }

// C-Const: "literal signed value n in [0, 2^31-1]" -> (n,n;nu) with nu=n.
// numeric-semantics.md / main.pdf Sec. 3 "i32 ... signed interpretation is
// two's-complement only where a rule says signed": the literal's low 32
// bits are reinterpreted as a signed i32 (well-defined two's-complement
// conversion as of C++20), and the rule applies only when that value is
// nonnegative.
CountOutcome infer_const(const LiteralExpr &literal, SourceSpan span) {
  if (literal.kind != LiteralKind::I32Bits) {
    return fail("SIZ003", span, "literal is not i32: no accepted count-refinement rule");
  }
  const auto signed_value = static_cast<std::int32_t>(static_cast<std::uint32_t>(literal.value));
  if (signed_value < 0) {
    return fail("SIZ003", span, "negative i32 literal: no accepted count-refinement rule");
  }
  const auto term = make_literal(static_cast<std::uint64_t>(signed_value));
  return ok_with(CountResult{term, term});
}

CountOutcome infer_count_impl(const Expr &expr);

// C-If-E: "guard Boolean; both branches (s,u_t;nu_t) and (s,u_f;nu_f)" ->
// (s,max(u_t,u_f);nu) with nu=s.
// C-If-U: "branch exact terms differ or either is star" ->
// (star,max(u_t,u_f);nu) with 0<=nu<=max(u_t,u_f).
CountOutcome infer_if(const IfExpr &if_expr) {
  const auto then_outcome = infer_count_impl(*if_expr.then_branch);
  if (!then_outcome.ok) {
    return then_outcome;
  }
  const auto else_outcome = infer_count_impl(*if_expr.else_branch);
  if (!else_outcome.ok) {
    return else_outcome;
  }
  const auto upper = make_max(then_outcome.result->upper, else_outcome.result->upper);
  if (then_outcome.result->exact && else_outcome.result->exact &&
      terms_equal(**then_outcome.result->exact, **else_outcome.result->exact)) {
    return ok_with(CountResult{*then_outcome.result->exact, upper}); // C-If-E
  }
  return ok_with(CountResult{std::nullopt, upper}); // C-If-U
}

CountOutcome infer_count_impl(const Expr &expr) {
  switch (expr.kind) {
  case ExprKind::Literal:
    return infer_const(std::get<LiteralExpr>(expr.data), expr.span);
  case ExprKind::If:
    return infer_if(std::get<IfExpr>(expr.data));
  case ExprKind::Let:
  case ExprKind::Fold:
  case ExprKind::Build:
  case ExprKind::UnaryPrimitive:
  case ExprKind::BinaryPrimitive:
  case ExprKind::Var:
  case ExprKind::Call:
  case ExprKind::ArrayLiteral:
  case ExprKind::Product:
  case ExprKind::Len:
  case ExprKind::Proj:
  case ExprKind::Index:
    return fail("SIZ003", expr.span, "expression has no accepted count-refinement rule");
  }
  return fail("SIZ003", expr.span, "internal: unreachable expression kind");
}

} // namespace

CountOutcome infer_count(const ast::Expr &expr) { return infer_count_impl(expr); }

} // namespace boundfin::source::size::count
