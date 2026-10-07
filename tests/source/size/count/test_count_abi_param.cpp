#include "boundfin/source/size/count/count.hpp"
#include "boundfin/source/size/shape/shape.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::ast::ArrayType;
using boundfin::source::ast::BindingId;
using boundfin::source::ast::SourceSpan;
using boundfin::source::ast::Type;
using boundfin::source::ast::TypeKind;
using boundfin::source::ast::TypePtr;
using boundfin::source::size::certificate::make_literal;
using boundfin::source::size::certificate::make_symbol;
using boundfin::source::size::certificate::terms_equal;
using boundfin::source::size::count::count_of_abi_param;
using boundfin::source::size::shape::ArrayShape;
using boundfin::source::size::shape::shape_of_abi_array_param;

// Table 6's C-ABI (main.pdf p.38): "ABI length symbol n_x, declaration
// capacity N" -> "(n_x,N;n_x) and 0<=n_x<=N". AM-034/AM-035: no source
// expression triggers this rule; it seeds Sigma/K_f/Q_f construction's
// own context for a function's formal ABI array parameters. n_x is
// minted deterministically as "abi#" + to_string(param_binding),
// identical to shape::shape_of_abi_array_param's own convention for the
// same parameter.
void CABI_mints_an_exact_count_with_the_declared_capacity_as_upper() {
  const auto result = count_of_abi_param(10, BindingId{42});
  BOUNDFIN_CHECK(result.exact.has_value());
  const auto expected_symbol = make_symbol("abi#42");
  BOUNDFIN_CHECK(terms_equal(**result.exact, *expected_symbol));
  BOUNDFIN_CHECK(terms_equal(*result.upper, *make_literal(10)));
}

// Two distinct ABI array parameters mint two distinct, structurally
// unequal symbols.
void CABI_mints_distinct_symbols_for_distinct_bindings() {
  const auto first = count_of_abi_param(10, BindingId{1});
  const auto second = count_of_abi_param(10, BindingId{2});
  BOUNDFIN_CHECK(!terms_equal(**first.exact, **second.exact));
}

// AM-035's own crux design point: both modules mint the IDENTICAL n_x
// for the same ABI array parameter, so Sigma/K_f/Q_f construction (a
// future slice) can rely on structural equality across the two
// independently-computed judgments without sharing a context object.
// Confirmed here by calling BOTH functions directly (src/source/size/
// count already depends on src/source/size/shape since the sixth
// slice's C-Len-E/C-Len-U), not merely by each test file independently
// hard-coding the same expected string.
void CABI_symbol_matches_the_shape_modules_identical_convention() {
  const auto count_result = count_of_abi_param(16, BindingId{7});

  auto element_type = std::make_shared<Type>();
  element_type->kind = TypeKind::I32;
  auto array_type_ptr = std::make_shared<Type>();
  array_type_ptr->kind = TypeKind::Array;
  array_type_ptr->data = ArrayType{element_type, 16};
  const auto shape_outcome = shape_of_abi_array_param(array_type_ptr, BindingId{7}, SourceSpan{});
  BOUNDFIN_CHECK(shape_outcome.ok);
  const auto &shape_result = std::get<ArrayShape>((*shape_outcome.result)->data);

  BOUNDFIN_CHECK(terms_equal(**count_result.exact, **shape_result.exact));
}

void boundfin_count_abi_param() {
  CABI_mints_an_exact_count_with_the_declared_capacity_as_upper();
  CABI_mints_distinct_symbols_for_distinct_bindings();
  CABI_symbol_matches_the_shape_modules_identical_convention();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_count_abi_param)
