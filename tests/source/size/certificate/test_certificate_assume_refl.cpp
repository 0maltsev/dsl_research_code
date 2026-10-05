#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <vector>

namespace {

using namespace boundfin::source::size::certificate;

// ASSUME: "phi in Delta => Delta |-cert phi" (main.pdf App. A.4).

void Assume_succeeds_when_claimed_constraint_is_in_delta() {
  const Constraint phi{ConstraintKind::Le, make_symbol("n_x"), make_symbol("N")};
  const std::vector<Constraint> delta{phi};
  const auto outcome = check_certificate(cert_assume(phi), delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(constraints_equal(*outcome.proved, phi));
}

// Assume also works for an Eq-shaped member of Delta: the boxed rule is
// generic over phi, not restricted to <=.
void Assume_succeeds_for_an_equality_member_of_delta() {
  const Constraint phi{ConstraintKind::Eq, make_symbol("n_x"), make_literal(7)};
  const std::vector<Constraint> delta{phi};
  const auto outcome = check_certificate(cert_assume(phi), delta);
  BOUNDFIN_CHECK(outcome.ok);
}

void Assume_fails_when_claimed_constraint_is_not_in_delta() {
  const Constraint phi{ConstraintKind::Le, make_symbol("n_x"), make_symbol("N")};
  const std::vector<Constraint> delta{}; // empty
  const auto outcome = check_certificate(cert_assume(phi), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

void Assume_fails_when_delta_has_a_similar_but_different_constraint() {
  const Constraint claimed{ConstraintKind::Le, make_symbol("n_x"), make_symbol("N")};
  const Constraint present{ConstraintKind::Le, make_symbol("n_x"), make_symbol("M")}; // different rhs
  const std::vector<Constraint> delta{present};
  const auto outcome = check_certificate(cert_assume(claimed), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

// REFL: "=> Delta |- u <= u" (main.pdf App. A.4): always succeeds for any
// term, regardless of Delta.
void Refl_succeeds_for_any_term_with_an_empty_delta() {
  const auto outcome = check_certificate(cert_refl(make_add(make_symbol("a"), make_literal(3))), {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.proved->kind == ConstraintKind::Le);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *outcome.proved->rhs));
}

void boundfin_certificate_assume_refl() {
  Assume_succeeds_when_claimed_constraint_is_in_delta();
  Assume_succeeds_for_an_equality_member_of_delta();
  Assume_fails_when_claimed_constraint_is_not_in_delta();
  Assume_fails_when_delta_has_a_similar_but_different_constraint();
  Refl_succeeds_for_any_term_with_an_empty_delta();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_assume_refl)
