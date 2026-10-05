#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

namespace {

using namespace boundfin::source::size::certificate;

// MAXL: "=> a<=max(a,b)"; MAXR: "=> b<=max(a,b)". Both hold unconditionally.

void MaxL_proves_a_le_max_a_b() {
  const auto outcome = check_certificate(cert_max_l(make_symbol("a"), make_symbol("b")), {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_symbol("a")));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_max(make_symbol("a"), make_symbol("b"))));
}

void MaxR_proves_b_le_max_a_b() {
  const auto outcome = check_certificate(cert_max_r(make_symbol("a"), make_symbol("b")), {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_symbol("b")));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_max(make_symbol("a"), make_symbol("b"))));
}

// MAXU: "a<=c  b<=c => max(a,b)<=c".

void MaxU_combines_two_upper_bounds_sharing_c() {
  const Constraint a_le_c{ConstraintKind::Le, make_symbol("a"), make_symbol("c")};
  const Constraint b_le_c{ConstraintKind::Le, make_symbol("b"), make_symbol("c")};
  const std::vector<Constraint> delta{a_le_c, b_le_c};
  const auto outcome = check_certificate(cert_max_u(cert_assume(a_le_c), cert_assume(b_le_c)), delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_max(make_symbol("a"), make_symbol("b"))));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_symbol("c")));
}

// Malformed tree: the two premises bound different upper terms.
void MaxU_rejects_premises_with_different_upper_bounds() {
  const Constraint a_le_c{ConstraintKind::Le, make_symbol("a"), make_symbol("c")};
  const Constraint b_le_d{ConstraintKind::Le, make_symbol("b"), make_symbol("d")}; // "d" != "c"
  const std::vector<Constraint> delta{a_le_c, b_le_d};
  const auto outcome = check_certificate(cert_max_u(cert_assume(a_le_c), cert_assume(b_le_d)), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

// Nested max: max(max(a,b),c) is reachable by combining MAXL/MAXU, and
// terms_equal must treat it structurally, not just at the top level.
void Max_can_nest_via_combined_rules() {
  const auto a = make_symbol("a");
  const auto b = make_symbol("b");
  const auto c = make_symbol("c");
  // a <= max(a,b), via MAXL.
  const auto a_le_max_ab = cert_max_l(a, b);
  // max(a,b) <= max(max(a,b),c), via MAXL again (treating max(a,b) as the
  // left operand of an outer max).
  const auto max_ab_le_outer = cert_max_l(make_max(a, b), c);
  const auto outcome = check_certificate(cert_trans(a_le_max_ab, max_ab_le_outer), {});
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *a));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_max(make_max(a, b), c)));
}

void boundfin_certificate_max() {
  MaxL_proves_a_le_max_a_b();
  MaxR_proves_b_le_max_a_b();
  MaxU_combines_two_upper_bounds_sharing_c();
  MaxU_rejects_premises_with_different_upper_bounds();
  Max_can_nest_via_combined_rules();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_max)
