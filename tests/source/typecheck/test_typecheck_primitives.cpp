#include "boundfin/source/parse/parser.hpp"
#include "boundfin/source/resolve/resolver.hpp"
#include "boundfin/source/typecheck/typecheck.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;
using boundfin::source::resolve::resolve_module;
using boundfin::source::typecheck::typecheck_module;

Module parse_and_resolve(const std::string &source) {
  auto parsed = parse_module(source);
  BOUNDFIN_CHECK(parsed.ok);
  auto module = std::move(*parsed.module);
  const auto resolved = resolve_module(module);
  BOUNDFIN_CHECK(resolved.ok);
  return module;
}

MonomorphicPrimitive unary_primitive_of(Module &module) {
  const auto &unary = std::get<UnaryPrimitiveExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(unary.resolved_primitive.has_value());
  return *unary.resolved_primitive;
}

MonomorphicPrimitive binary_primitive_of(Module &module) {
  const auto &binary = std::get<BinaryPrimitiveExpr>(module.functions[0].body->data);
  BOUNDFIN_CHECK(binary.resolved_primitive.has_value());
  return *binary.resolved_primitive;
}

MonomorphicPrimitive unary_primitive_for(const std::string &source) {
  auto module = parse_and_resolve(source);
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  return unary_primitive_of(module);
}

MonomorphicPrimitive binary_primitive_for(const std::string &source) {
  auto module = parse_and_resolve(source);
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  return binary_primitive_of(module);
}

const std::string kI32A = "i32bits(0x00000001)";
const std::string kI32B = "i32bits(0x00000002)";
const std::string kI64A = "i64bits(0x0000000000000001)";
const std::string kI64B = "i64bits(0x0000000000000002)";
const std::string kF64A = "f64bits(0x3ff0000000000000)"; // 1.0
const std::string kF64B = "f64bits(0x4000000000000000)"; // 2.0

// --- Unary: Table 5 "bool.not", "neg_w", "fneg"/"fabs" --------------------
// Every unary primitive has exactly one (op, operand-kind) combination in
// Table 5 except Neg (i32/i64/f64); all are covered below.

void TPrim_not_monomorphizes_to_BoolNot() {
  BOUNDFIN_CHECK(unary_primitive_for("fn f(): bool = !true;\nexport f;\n") == MonomorphicPrimitive::BoolNot);
}

void TYP001_not_rejects_non_bool_operand() {
  auto module = parse_and_resolve("fn f(): bool = !i32bits(0x00000001);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP001"));
}

void TPrim_neg_monomorphizes_per_width() {
  BOUNDFIN_CHECK(unary_primitive_for("fn f(): i32 = -" + kI32A + ";\nexport f;\n") == MonomorphicPrimitive::Neg32);
  BOUNDFIN_CHECK(unary_primitive_for("fn f(): i64 = -" + kI64A + ";\nexport f;\n") == MonomorphicPrimitive::Neg64);
  BOUNDFIN_CHECK(unary_primitive_for("fn f(): f64 = -" + kF64A + ";\nexport f;\n") == MonomorphicPrimitive::FNeg);
}

// Table 5 lists only `fabs: f64 -> f64`; there is no integer abs.
void TPrim_abs_monomorphizes_to_FAbs_and_rejects_integers() {
  BOUNDFIN_CHECK(unary_primitive_for("fn f(): f64 = abs(" + kF64A + ");\nexport f;\n") == MonomorphicPrimitive::FAbs);

  auto bad_module = parse_and_resolve("fn f(): i32 = abs(" + kI32A + ");\nexport f;\n");
  const auto bad_result = typecheck_module(bad_module);
  BOUNDFIN_CHECK(!bad_result.ok);
  BOUNDFIN_CHECK_EQ(bad_result.diagnostic->code, std::string("TYP001"));
}

// --- Binary arithmetic: Table 5 "add/sub/mul/divs_w", "fadd/fsub/fmul/fdiv" -
// Each of Add/Sub/Mul/Div has exactly 3 combinations (i32, i64, f64); all
// 12 are asserted below.

void TPrim_add_monomorphizes_per_width() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i32 = " + kI32A + " + " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Add32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i64 = " + kI64A + " + " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Add64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): f64 = " + kF64A + " + " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FAdd);
}

void TPrim_sub_monomorphizes_per_width() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i32 = " + kI32A + " - " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Sub32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i64 = " + kI64A + " - " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Sub64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): f64 = " + kF64A + " - " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FSub);
}

void TPrim_mul_monomorphizes_per_width() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i32 = " + kI32A + " * " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Mul32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i64 = " + kI64A + " * " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Mul64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): f64 = " + kF64A + " * " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FMul);
}

void TPrim_div_monomorphizes_per_width() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i32 = " + kI32A + " / " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::DivS32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i64 = " + kI64A + " / " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::DivS64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): f64 = " + kF64A + " / " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FDiv);
}

void TYP012_mismatched_numeric_widths_are_rejected() {
  auto module = parse_and_resolve("fn f(): i64 = " + kI32A + " + " + kI64B + ";\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP012"));
}

void TYP001_arithmetic_rejects_non_numeric_operands() {
  auto module = parse_and_resolve("fn f(): bool = true + true;\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP001"));
}

// --- Rem: Table 5 "rems_w" only -- no floating remainder (TYP012) ---------
// Exactly 2 combinations (i32, i64); both asserted below.

void TPrim_rem_monomorphizes_per_integer_width() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i32 = i32bits(0x00000007) % " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::RemS32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): i64 = i64bits(0x0000000000000007) % " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::RemS64);
}

void TYP012_floating_remainder_is_rejected() {
  auto module = parse_and_resolve("fn f(): f64 = " + kF64A + " % " + kF64B + ";\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP012"));
}

// --- Eq/Ne: Table 5 defines these for bool, i32, i64, and f64 -------------
// Each of Eq/Ne has exactly 4 combinations; all 8 are asserted below.

void TPrim_eq_monomorphizes_per_scalar_kind() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = true == false;\nexport f;\n") == MonomorphicPrimitive::BoolEq);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI32A + " == " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Eq32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI64A + " == " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Eq64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kF64A + " == " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FEq);
}

void TPrim_ne_monomorphizes_per_scalar_kind() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = true != false;\nexport f;\n") == MonomorphicPrimitive::BoolNe);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI32A + " != " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Ne32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI64A + " != " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::Ne64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kF64A + " != " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FNe);
}

void TYP012_eq_rejects_mismatched_types() {
  auto module = parse_and_resolve("fn f(): bool = true == " + kI32A + ";\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP012"));
}

// --- Lt/Le/Gt/Ge: Table 5 defines these for i32/i64/f64 only, not bool ----
// Each has exactly 3 combinations; all 12 are asserted below.

void TPrim_lt_monomorphizes_per_numeric_kind() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI32A + " < " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::LtS32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI64A + " < " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::LtS64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kF64A + " < " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FLt);
}

void TPrim_le_monomorphizes_per_numeric_kind() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI32A + " <= " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::LeS32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI64A + " <= " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::LeS64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kF64A + " <= " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FLe);
}

void TPrim_gt_monomorphizes_per_numeric_kind() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI32A + " > " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::GtS32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI64A + " > " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::GtS64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kF64A + " > " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FGt);
}

void TPrim_ge_monomorphizes_per_numeric_kind() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI32A + " >= " + kI32B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::GeS32);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kI64A + " >= " + kI64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::GeS64);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = " + kF64A + " >= " + kF64B + ";\nexport f;\n") ==
                 MonomorphicPrimitive::FGe);
}

void TYP012_boolean_ordering_is_rejected() {
  auto module = parse_and_resolve("fn f(): bool = (true == true) < (false == false);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP012"));
}

// --- And/Or: Table 5 "bool.and/or", strict (both operands evaluated) -----
// Each has exactly 1 combination (bool only); both asserted below.

void TPrim_and_or_monomorphize_to_bool_primitives() {
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = true && false;\nexport f;\n") == MonomorphicPrimitive::BoolAnd);
  BOUNDFIN_CHECK(binary_primitive_for("fn f(): bool = true || false;\nexport f;\n") == MonomorphicPrimitive::BoolOr);
}

void TYP001_and_rejects_non_bool_operands() {
  auto module = parse_and_resolve("fn f(): bool = " + kI32A + " && i32bits(0x00000000);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP001"));
}

void boundfin_typecheck_primitives() {
  TPrim_not_monomorphizes_to_BoolNot();
  TYP001_not_rejects_non_bool_operand();
  TPrim_neg_monomorphizes_per_width();
  TPrim_abs_monomorphizes_to_FAbs_and_rejects_integers();
  TPrim_add_monomorphizes_per_width();
  TPrim_sub_monomorphizes_per_width();
  TPrim_mul_monomorphizes_per_width();
  TPrim_div_monomorphizes_per_width();
  TYP012_mismatched_numeric_widths_are_rejected();
  TYP001_arithmetic_rejects_non_numeric_operands();
  TPrim_rem_monomorphizes_per_integer_width();
  TYP012_floating_remainder_is_rejected();
  TPrim_eq_monomorphizes_per_scalar_kind();
  TPrim_ne_monomorphizes_per_scalar_kind();
  TYP012_eq_rejects_mismatched_types();
  TPrim_lt_monomorphizes_per_numeric_kind();
  TPrim_le_monomorphizes_per_numeric_kind();
  TPrim_gt_monomorphizes_per_numeric_kind();
  TPrim_ge_monomorphizes_per_numeric_kind();
  TYP012_boolean_ordering_is_rejected();
  TPrim_and_or_monomorphize_to_bool_primitives();
  TYP001_and_rejects_non_bool_operands();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_primitives)
