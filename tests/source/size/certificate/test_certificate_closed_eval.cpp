#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <limits>

namespace {

using namespace boundfin::source::size::certificate;

// ClosedEval: "A closed arithmetic leaf is accepted exactly when direct
// natural-number evaluation makes it true" (main.pdf App. A.4).

void ClosedEval_accepts_a_true_closed_le_fact() {
  const auto term = make_add(make_literal(2), make_scale(3, make_literal(1))); // 2 + 3*1 = 5
  const Constraint claimed{ConstraintKind::Le, term, make_literal(5)};         // 5 <= 5
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(outcome.ok);
}

void ClosedEval_rejects_a_false_closed_le_fact() {
  const Constraint claimed{ConstraintKind::Le, make_literal(6), make_literal(5)}; // 6 <= 5 is false
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

void ClosedEval_accepts_a_true_closed_eq_fact() {
  const auto term = make_max(make_literal(3), make_literal(7)); // max(3,7) = 7
  const Constraint claimed{ConstraintKind::Eq, term, make_literal(7)};
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(outcome.ok);
}

void ClosedEval_rejects_a_false_closed_eq_fact() {
  const Constraint claimed{ConstraintKind::Eq, make_literal(3), make_literal(4)};
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

// A term containing a free symbol is not "closed"; ClosedEval must reject
// it regardless of whether the relation happens to be plausible.
void ClosedEval_rejects_a_term_containing_a_free_symbol() {
  const Constraint claimed{ConstraintKind::Le, make_symbol("n_x"), make_literal(100)};
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

// Overflow during evaluation is rejected, not wrapped: this is
// mathematical-integer evaluation, not modular machine arithmetic.
void ClosedEval_rejects_an_overflowing_evaluation() {
  const auto huge = make_literal(std::numeric_limits<std::uint64_t>::max());
  const Constraint claimed{ConstraintKind::Le, make_add(huge, make_literal(1)), make_literal(0)};
  const auto outcome = check_certificate(cert_closed_eval(claimed), {});
  BOUNDFIN_CHECK(!outcome.ok);
}

void boundfin_certificate_closed_eval() {
  ClosedEval_accepts_a_true_closed_le_fact();
  ClosedEval_rejects_a_false_closed_le_fact();
  ClosedEval_accepts_a_true_closed_eq_fact();
  ClosedEval_rejects_a_false_closed_eq_fact();
  ClosedEval_rejects_a_term_containing_a_free_symbol();
  ClosedEval_rejects_an_overflowing_evaluation();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_closed_eval)
