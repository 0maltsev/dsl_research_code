#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <vector>

namespace {

using namespace boundfin::source::size::certificate;

// EqSandwich: "Equality is checked as two inequalities" (main.pdf App.
// A.4) -- Delta|-t1<=t2 and Delta|-t2<=t1 together prove Delta|-t1=t2.

void EqSandwich_proves_equality_from_both_directions() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint b_le_a{ConstraintKind::Le, make_symbol("b"), make_symbol("a")};
  const std::vector<Constraint> delta{a_le_b, b_le_a};
  const auto outcome = check_certificate(cert_eq_sandwich(cert_assume(a_le_b), cert_assume(b_le_a)), delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(outcome.proved->kind == ConstraintKind::Eq);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_symbol("a")));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_symbol("b")));
}

// Malformed tree: the two premises are not over the same pair of terms.
void EqSandwich_rejects_premises_over_different_terms() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint c_le_a{ConstraintKind::Le, make_symbol("c"), make_symbol("a")}; // "c" != "b"
  const std::vector<Constraint> delta{a_le_b, c_le_a};
  const auto outcome = check_certificate(cert_eq_sandwich(cert_assume(a_le_b), cert_assume(c_le_a)), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

void EqSandwich_rejects_an_invalid_premise() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint b_le_a{ConstraintKind::Le, make_symbol("b"), make_symbol("a")};
  const std::vector<Constraint> delta{a_le_b}; // b_le_a is not assumed
  const auto outcome = check_certificate(cert_eq_sandwich(cert_assume(a_le_b), cert_assume(b_le_a)), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

void boundfin_certificate_eq_sandwich() {
  EqSandwich_proves_equality_from_both_directions();
  EqSandwich_rejects_premises_over_different_terms();
  EqSandwich_rejects_an_invalid_premise();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_eq_sandwich)
