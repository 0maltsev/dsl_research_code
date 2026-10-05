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

constexpr std::uint64_t kI32Max = 2147483647; // 2^31-1

// C-Add: "operands (lambda_i,u_i;nu_i); certificate NW_+" ->
// (lambda1+lambda2,u1+u2;nu) if both exact, else (star,u1+u2;nu).
// AM-023: the certificate obligation is exactly "u1+u2 <=cert 2^31-1"
// (0<=u1+u2 is free/structural, since upper-bound terms are built only
// from naturals); no separate obligation for the exact case, which
// follows from the already-maintained lambda<=u invariant. Every u
// reachable at this point is closed (built only from C-Const/C-If's
// Literal/Max), so the obligation is certified via direct closed
// evaluation (ClosedEval); a future symbolic u (C-ABI/C-Idx) will need a
// non-ClosedEval certificate, not yet needed here.
CountOutcome infer_add(const BinaryPrimitiveExpr &binary, SourceSpan span) {
  const auto lhs_outcome = infer_count_impl(*binary.lhs);
  if (!lhs_outcome.ok) {
    return lhs_outcome;
  }
  const auto rhs_outcome = infer_count_impl(*binary.rhs);
  if (!rhs_outcome.ok) {
    return rhs_outcome;
  }
  const auto upper = make_add(lhs_outcome.result->upper, rhs_outcome.result->upper);
  const auto obligation = cert_closed_eval(Constraint{ConstraintKind::Le, upper, make_literal(kI32Max)});
  const auto check = check_certificate(obligation, {});
  if (!check.ok) {
    return fail("SIZ006", span, "count add lacks the required no-wrap certificate: " + *check.failure_reason);
  }
  std::optional<TermPtr> exact;
  if (lhs_outcome.result->exact && rhs_outcome.result->exact) {
    exact = make_add(*lhs_outcome.result->exact, *rhs_outcome.result->exact);
  }
  return ok_with(CountResult{exact, upper});
}

// C-Sub: "operands (lambda_i,u_i;nu_i); certificate NW_-" ->
// (lambda1-lambda2,u1;nu) if both exact, else (star,u1;nu).
// AM-025 (correcting AM-023's original C-Sub reading, found unsound: a
// counterexample ((10-9)-2, a deterministic underflow) was silently
// accepted because certifying u2<=u1 does not bound the real minuend
// value once u1 is a loose upper bound, e.g. after a nested C-Sub keeps
// u1 wide while lambda1 is tight). u is only ever an *upper* bound on
// the judgment's real nu; it is not usable as a stand-in for nu1 in a
// no-underflow check unless it is tight, i.e. unless lambda1 is present
// (lambda1 == nu1 exactly, per Sec. 4.2's own "nu = s" definition). The
// corrected obligation is "u2 <=cert lambda1", REQUIRING lambda1 to be
// present: if the minuend itself is inexact, there is no sound lower
// bound on nu1 available from (lambda,u) tracking alone, so C-Sub is
// rejected (SIZ006) rather than falling back to an inexact result.
// AM-024: the exact case is reachable only when both operands are
// *closed* (the term grammar t::=n|xi|t+t|kt has no subtraction
// production, so a symbolic lambda1-lambda2 cannot be represented the
// way C-Add's lambda1+lambda2 can via t+t); compute the natural-number
// difference directly after confirming lambda2<=lambda1, else fall back
// to star even though lambda1 is present (lambda2 may still be absent
// or non-closed).
CountOutcome infer_sub(const BinaryPrimitiveExpr &binary, SourceSpan span) {
  const auto lhs_outcome = infer_count_impl(*binary.lhs);
  if (!lhs_outcome.ok) {
    return lhs_outcome;
  }
  const auto rhs_outcome = infer_count_impl(*binary.rhs);
  if (!rhs_outcome.ok) {
    return rhs_outcome;
  }
  const auto upper = lhs_outcome.result->upper; // C-Sub's upper bound is simply u1
  if (!lhs_outcome.result->exact) {
    return fail("SIZ006", span,
                "count sub lacks the required no-wrap certificate: minuend has no exact lower bound to certify against");
  }
  const auto &lambda1 = *lhs_outcome.result->exact;
  const auto obligation = cert_closed_eval(Constraint{ConstraintKind::Le, rhs_outcome.result->upper, lambda1});
  const auto check = check_certificate(obligation, {});
  if (!check.ok) {
    return fail("SIZ006", span, "count sub lacks the required no-wrap certificate: " + *check.failure_reason);
  }
  std::optional<TermPtr> exact;
  if (rhs_outcome.result->exact) {
    const auto lhs_value = evaluate_closed(*lambda1);
    const auto rhs_value = evaluate_closed(**rhs_outcome.result->exact);
    if (lhs_value && rhs_value && *rhs_value <= *lhs_value) {
      exact = make_literal(*lhs_value - *rhs_value);
    }
  }
  return ok_with(CountResult{exact, upper});
}

CountOutcome infer_count_impl(const Expr &expr) {
  switch (expr.kind) {
  case ExprKind::Literal:
    return infer_const(std::get<LiteralExpr>(expr.data), expr.span);
  case ExprKind::If:
    return infer_if(std::get<IfExpr>(expr.data));
  case ExprKind::BinaryPrimitive: {
    const auto &binary = std::get<BinaryPrimitiveExpr>(expr.data);
    if (binary.op == BinaryPrimitiveOp::Add) {
      return infer_add(binary, expr.span);
    }
    if (binary.op == BinaryPrimitiveOp::Sub) {
      return infer_sub(binary, expr.span);
    }
    return fail("SIZ003", expr.span, "expression has no accepted count-refinement rule");
  }
  case ExprKind::Let:
  case ExprKind::Fold:
  case ExprKind::Build:
  case ExprKind::UnaryPrimitive:
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
