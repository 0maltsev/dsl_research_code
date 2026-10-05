#include "boundfin/source/size/certificate/certificate.hpp"
#include "boundfin_test.hpp"

namespace {

using namespace boundfin::source::size::certificate;

void literal_terms_equal_iff_same_value() {
  BOUNDFIN_CHECK(terms_equal(*make_literal(5), *make_literal(5)));
  BOUNDFIN_CHECK(!terms_equal(*make_literal(5), *make_literal(6)));
}

void symbol_terms_equal_iff_same_name() {
  BOUNDFIN_CHECK(terms_equal(*make_symbol("n_x"), *make_symbol("n_x")));
  BOUNDFIN_CHECK(!terms_equal(*make_symbol("n_x"), *make_symbol("n_y")));
}

void different_kinds_are_never_equal() {
  BOUNDFIN_CHECK(!terms_equal(*make_literal(0), *make_symbol("n_x")));
}

void add_terms_equal_require_both_operands_equal() {
  BOUNDFIN_CHECK(terms_equal(*make_add(make_literal(1), make_literal(2)), *make_add(make_literal(1), make_literal(2))));
  BOUNDFIN_CHECK(!terms_equal(*make_add(make_literal(1), make_literal(2)), *make_add(make_literal(2), make_literal(1))));
}

void scale_terms_equal_require_same_factor_and_term() {
  BOUNDFIN_CHECK(terms_equal(*make_scale(3, make_symbol("x")), *make_scale(3, make_symbol("x"))));
  BOUNDFIN_CHECK(!terms_equal(*make_scale(3, make_symbol("x")), *make_scale(4, make_symbol("x"))));
  BOUNDFIN_CHECK(!terms_equal(*make_scale(3, make_symbol("x")), *make_scale(3, make_symbol("y"))));
}

void max_terms_equal_require_both_operands_equal_in_order() {
  BOUNDFIN_CHECK(terms_equal(*make_max(make_symbol("a"), make_symbol("b")), *make_max(make_symbol("a"), make_symbol("b"))));
  BOUNDFIN_CHECK(
      !terms_equal(*make_max(make_symbol("a"), make_symbol("b")), *make_max(make_symbol("b"), make_symbol("a"))));
}

void nested_terms_compare_structurally() {
  const auto lhs = make_add(make_scale(2, make_symbol("x")), make_max(make_literal(1), make_symbol("y")));
  const auto rhs = make_add(make_scale(2, make_symbol("x")), make_max(make_literal(1), make_symbol("y")));
  BOUNDFIN_CHECK(terms_equal(*lhs, *rhs));
  const auto different = make_add(make_scale(2, make_symbol("x")), make_max(make_literal(1), make_symbol("z")));
  BOUNDFIN_CHECK(!terms_equal(*lhs, *different));
}

void constraints_equal_require_same_kind_and_terms() {
  const Constraint le1{ConstraintKind::Le, make_literal(1), make_literal(2)};
  const Constraint le2{ConstraintKind::Le, make_literal(1), make_literal(2)};
  const Constraint eq{ConstraintKind::Eq, make_literal(1), make_literal(2)};
  const Constraint le_different{ConstraintKind::Le, make_literal(1), make_literal(3)};
  BOUNDFIN_CHECK(constraints_equal(le1, le2));
  BOUNDFIN_CHECK(!constraints_equal(le1, eq));
  BOUNDFIN_CHECK(!constraints_equal(le1, le_different));
}

void boundfin_certificate_terms() {
  literal_terms_equal_iff_same_value();
  symbol_terms_equal_iff_same_name();
  different_kinds_are_never_equal();
  add_terms_equal_require_both_operands_equal();
  scale_terms_equal_require_same_factor_and_term();
  max_terms_equal_require_both_operands_equal_in_order();
  nested_terms_compare_structurally();
  constraints_equal_require_same_kind_and_terms();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_certificate_terms)
