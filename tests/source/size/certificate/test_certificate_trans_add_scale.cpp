#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

#include <vector>

namespace {

using namespace boundfin::source::size::certificate;

// TRANS: "Delta|-u1<=u2  Delta|-u2<=u3 => Delta|-u1<=u3".

void Trans_chains_two_inequalities() {
  const Constraint u1_le_u2{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint u2_le_u3{ConstraintKind::Le, make_symbol("b"), make_symbol("c")};
  const std::vector<Constraint> delta{u1_le_u2, u2_le_u3};
  const auto outcome = check_certificate(cert_trans(cert_assume(u1_le_u2), cert_assume(u2_le_u3)), delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_symbol("a")));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_symbol("c")));
}

// Malformed tree: the middle term doesn't match between the two premises.
void Trans_rejects_a_mismatched_middle_term() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint x_le_c{ConstraintKind::Le, make_symbol("x"), make_symbol("c")}; // "x" != "b"
  const std::vector<Constraint> delta{a_le_b, x_le_c};
  const auto outcome = check_certificate(cert_trans(cert_assume(a_le_b), cert_assume(x_le_c)), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

void Trans_rejects_an_invalid_premise() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint not_in_delta{ConstraintKind::Le, make_symbol("b"), make_symbol("c")};
  const std::vector<Constraint> delta{a_le_b}; // not_in_delta is missing
  const auto outcome = check_certificate(cert_trans(cert_assume(a_le_b), cert_assume(not_in_delta)), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

// ADD: "a<=b  c<=d => a+c<=b+d".

void Add_combines_two_inequalities_componentwise() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint c_le_d{ConstraintKind::Le, make_symbol("c"), make_symbol("d")};
  const std::vector<Constraint> delta{a_le_b, c_le_d};
  const auto outcome = check_certificate(cert_add(cert_assume(a_le_b), cert_assume(c_le_d)), delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_add(make_symbol("a"), make_symbol("c"))));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_add(make_symbol("b"), make_symbol("d"))));
}

void Add_rejects_a_premise_that_is_not_a_valid_le_fact() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const Constraint not_assumed{ConstraintKind::Le, make_symbol("c"), make_symbol("d")};
  const std::vector<Constraint> delta{a_le_b};
  const auto outcome = check_certificate(cert_add(cert_assume(a_le_b), cert_assume(not_assumed)), delta);
  BOUNDFIN_CHECK(!outcome.ok);
}

// SCALE (k in N): "a<=b => ka<=kb".

void Scale_multiplies_both_sides_by_the_same_factor() {
  const Constraint a_le_b{ConstraintKind::Le, make_symbol("a"), make_symbol("b")};
  const std::vector<Constraint> delta{a_le_b};
  const auto outcome = check_certificate(cert_scale(5, cert_assume(a_le_b)), delta);
  BOUNDFIN_CHECK(outcome.ok);
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->lhs, *make_scale(5, make_symbol("a"))));
  BOUNDFIN_CHECK(terms_equal(*outcome.proved->rhs, *make_scale(5, make_symbol("b"))));
}

void Scale_rejects_an_invalid_premise() {
  const auto outcome = check_certificate(cert_scale(5, cert_assume(Constraint{ConstraintKind::Le, make_symbol("a"),
                                                                                make_symbol("b")})),
                                          {}); // empty delta: the assumption is unjustified
  BOUNDFIN_CHECK(!outcome.ok);
}

void boundfin_certificate_trans_add_scale() {
  Trans_chains_two_inequalities();
  Trans_rejects_a_mismatched_middle_term();
  Trans_rejects_an_invalid_premise();
  Add_combines_two_inequalities_componentwise();
  Add_rejects_a_premise_that_is_not_a_valid_le_fact();
  Scale_multiplies_both_sides_by_the_same_factor();
  Scale_rejects_an_invalid_premise();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_trans_add_scale)
