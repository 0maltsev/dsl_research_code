#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <vector>

namespace {

using namespace boundfin::source::size::certificate;

// A malformed (null) TermPtr/Constraint field must be rejected cleanly by
// check_certificate (CheckOutcome.ok == false), never crash. None of the
// public cert_*()/make_*() builders validate their arguments -- the
// checker itself is the trust boundary (main.pdf Sec. 3.3, p.8:
// "Certificate generation may use an untrusted solver, but module
// acceptance depends on checking a finite derivation").

void Assume_rejects_a_claimed_constraint_with_a_null_term() {
  const Constraint claimed{ConstraintKind::Le, TermPtr{}, make_literal(5)};
  const std::vector<Constraint> delta{claimed}; // even if "present" verbatim in Delta
  const auto outcome = check_certificate(cert_assume(claimed), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

void Refl_rejects_a_null_term() {
  const auto outcome = check_certificate(cert_refl(TermPtr{}), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

void MaxL_rejects_a_null_operand() {
  BOUNDFIN_CHECK(!check_certificate(cert_max_l(TermPtr{}, make_symbol("b")), {}).ok);
  BOUNDFIN_CHECK(!check_certificate(cert_max_l(make_symbol("a"), TermPtr{}), {}).ok);
}

void MaxR_rejects_a_null_operand() {
  BOUNDFIN_CHECK(!check_certificate(cert_max_r(TermPtr{}, make_symbol("b")), {}).ok);
  BOUNDFIN_CHECK(!check_certificate(cert_max_r(make_symbol("a"), TermPtr{}), {}).ok);
}

void ClosedEval_rejects_a_claimed_constraint_with_a_null_term() {
  const Constraint claimed{ConstraintKind::Le, TermPtr{}, make_literal(5)};
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

// A null child *nested inside* an otherwise non-null term (not just a
// top-level null TermPtr) must also be caught, by both the term-equality
// path (Assume/terms_equal) and the evaluation path (ClosedEval).
void ClosedEval_rejects_a_nested_null_child() {
  const auto broken_sum = make_add(TermPtr{}, make_literal(1)); // Add(null, 1)
  const Constraint claimed{ConstraintKind::Le, broken_sum, make_literal(5)};
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

void Assume_rejects_when_delta_member_has_a_nested_null_child() {
  const auto broken_sum = make_add(TermPtr{}, make_literal(1));
  const Constraint claimed{ConstraintKind::Le, broken_sum, make_literal(5)};
  const std::vector<Constraint> delta{claimed};
  const auto outcome = check_certificate(cert_assume(claimed), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

// terms_equal/constraints_equal themselves must not crash when handed a
// term/constraint with a null top-level or nested field.
void terms_equal_is_null_safe() {
  const Term null_add_lhs{TermKind::Add, AddTerm{TermPtr{}, make_literal(1)}};
  const Term literal_two{TermKind::Literal, LiteralTerm{2}};
  BOUNDFIN_CHECK(!terms_equal(null_add_lhs, literal_two));
  BOUNDFIN_CHECK(!terms_equal(null_add_lhs, null_add_lhs)); // not even equal to itself
}

void constraints_equal_is_null_safe() {
  const Constraint a{ConstraintKind::Le, TermPtr{}, make_literal(5)};
  const Constraint b{ConstraintKind::Le, make_literal(1), make_literal(5)};
  BOUNDFIN_CHECK(!constraints_equal(a, b));
  BOUNDFIN_CHECK(!constraints_equal(a, a));
}

void boundfin_certificate_null_safety() {
  Assume_rejects_a_claimed_constraint_with_a_null_term();
  Refl_rejects_a_null_term();
  MaxL_rejects_a_null_operand();
  MaxR_rejects_a_null_operand();
  ClosedEval_rejects_a_claimed_constraint_with_a_null_term();
  ClosedEval_rejects_a_nested_null_child();
  Assume_rejects_when_delta_member_has_a_nested_null_child();
  terms_equal_is_null_safe();
  constraints_equal_is_null_safe();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_null_safety)
