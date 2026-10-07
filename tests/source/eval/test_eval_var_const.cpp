#include "boundfin/source/eval/eval.hpp"
#include "boundfin_test.hpp"

namespace {

using namespace boundfin::source::ast;
using namespace boundfin::source::eval;

// E-Var and E-Const (main.pdf p.39, Appendix A.6).

VarExpr resolved_var(BindingId binding) {
  VarExpr var_expr;
  var_expr.name = "x";
  var_expr.resolved_binding = binding;
  return var_expr;
}

LiteralExpr literal(LiteralKind kind, std::uint64_t value) {
  LiteralExpr literal_expr;
  literal_expr.kind = kind;
  literal_expr.value = value;
  return literal_expr;
}

StoreObject sealed_store_object() {
  SealedObject object;
  object.capacity = 4;
  object.length = 0;
  object.origin = Origin::Local;
  return StoreObject{StoreObjectKind::Sealed, object};
}

void eval_var_resolves_a_scalar_from_the_environment() {
  const Environment environment{{1, make_i32(0x2A)}};
  const auto result = eval_var(resolved_var(1), environment, {}, {});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.outcome->ok);
  BOUNDFIN_CHECK(result.outcome->value->kind == ValueKind::I32);
  BOUNDFIN_CHECK_EQ(std::get<I32Word>(result.outcome->value->data).bits, static_cast<std::uint32_t>(0x2A));
}

// The key store-restriction behavior (main.pdf p.39: "sigma restricted-to
// (K union {v})"): object 1 is the looked-up array value's own referent
// (kept); object 2 is unreachable from both K and the looked-up value
// (dropped).
void eval_var_restricts_the_store_to_the_looked_up_value_plus_roots() {
  Store store;
  store.emplace(1, sealed_store_object());
  store.emplace(2, sealed_store_object());
  const Environment environment{{1, make_array(1)}};
  const auto result = eval_var(resolved_var(1), environment, store, {});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.store->size(), static_cast<std::size_t>(1));
  BOUNDFIN_CHECK(result.store->count(1) == 1);
  BOUNDFIN_CHECK(result.store->count(2) == 0);
}

// An object reachable only via the pre-existing root set K (not the
// looked-up value itself) is still retained -- K must survive regardless
// of which value this particular step returns.
void eval_var_retains_objects_reachable_only_via_the_existing_roots() {
  Store store;
  store.emplace(1, sealed_store_object()); // reachable via K below
  const Environment environment{{1, make_i32(5)}}; // looked-up value is scalar
  const auto result = eval_var(resolved_var(1), environment, store, {make_array(1)});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.store->count(1) == 1);
}

void eval_var_rejects_with_internal_error_for_an_unresolved_binding() {
  VarExpr var_expr;
  var_expr.name = "x";
  var_expr.resolved_binding = std::nullopt;
  const auto result = eval_var(var_expr, {}, {}, {});
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK(result.internal_error.has_value());
}

void eval_var_rejects_with_internal_error_when_binding_absent_from_environment() {
  const Environment environment; // deliberately empty
  const auto result = eval_var(resolved_var(1), environment, {}, {});
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK(result.internal_error.has_value());
}

void eval_const_produces_bool_true_and_false() {
  const auto true_result = eval_const(literal(LiteralKind::Bool, 1), {}, {});
  BOUNDFIN_CHECK(true_result.ok);
  BOUNDFIN_CHECK(true_result.outcome->value->kind == ValueKind::Bool);
  BOUNDFIN_CHECK_EQ(std::get<BoolWord>(true_result.outcome->value->data).value, true);

  const auto false_result = eval_const(literal(LiteralKind::Bool, 0), {}, {});
  BOUNDFIN_CHECK_EQ(std::get<BoolWord>(false_result.outcome->value->data).value, false);
}

void eval_const_produces_the_correct_i32_bits() {
  const auto result = eval_const(literal(LiteralKind::I32Bits, 0x7FFFFFFF), {}, {});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.outcome->value->kind == ValueKind::I32);
  BOUNDFIN_CHECK_EQ(std::get<I32Word>(result.outcome->value->data).bits, static_cast<std::uint32_t>(0x7FFFFFFF));
}

void eval_const_produces_the_correct_i64_bits() {
  const auto result = eval_const(literal(LiteralKind::I64Bits, 0xFFFFFFFFFFFFFFFFULL), {}, {});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.outcome->value->kind == ValueKind::I64);
  BOUNDFIN_CHECK_EQ(std::get<I64Word>(result.outcome->value->data).bits, 0xFFFFFFFFFFFFFFFFULL);
}

// numeric-semantics.md: "Literal and ABI input words are not
// canonicalized merely by being read" -- a raw NaN bit pattern passes
// through E-Const completely unchanged, not replaced with the
// canonical qNaN.
void eval_const_f64_bits_are_not_canonicalized() {
  constexpr std::uint64_t signaling_nan = 0x7ff0000000000001ULL;
  const auto result = eval_const(literal(LiteralKind::F64Bits, signaling_nan), {}, {});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.outcome->value->kind == ValueKind::F64);
  BOUNDFIN_CHECK_EQ(std::get<F64Word>(result.outcome->value->data).bits, signaling_nan);
}

// main.pdf p.39: "sigma restricted-to K" (not K union {v}) -- a constant
// is always scalar, so the restriction depends only on K, confirmed here
// by an object reachable solely through K surviving regardless of the
// constant's own (scalar) value.
void eval_const_restricts_the_store_to_just_the_roots() {
  Store store;
  store.emplace(1, sealed_store_object()); // reachable via K
  store.emplace(2, sealed_store_object()); // not reachable at all -- dropped
  const auto result = eval_const(literal(LiteralKind::I32Bits, 1), store, {make_array(1)});
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.store->size(), static_cast<std::size_t>(1));
  BOUNDFIN_CHECK(result.store->count(1) == 1);
  BOUNDFIN_CHECK(result.store->count(2) == 0);
}

void boundfin_eval_var_const() {
  eval_var_resolves_a_scalar_from_the_environment();
  eval_var_restricts_the_store_to_the_looked_up_value_plus_roots();
  eval_var_retains_objects_reachable_only_via_the_existing_roots();
  eval_var_rejects_with_internal_error_for_an_unresolved_binding();
  eval_var_rejects_with_internal_error_when_binding_absent_from_environment();
  eval_const_produces_bool_true_and_false();
  eval_const_produces_the_correct_i32_bits();
  eval_const_produces_the_correct_i64_bits();
  eval_const_f64_bits_are_not_canonicalized();
  eval_const_restricts_the_store_to_just_the_roots();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_eval_var_const)
