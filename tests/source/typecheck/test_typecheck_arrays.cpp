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

// T-Array: "m <= N and every e_i:tau" -> "literal: arr<tau, N>". N is the
// literal's own declared capacity; AM-018 keeps "m <= N" out of this phase.
void TArray_result_type_is_arr_of_element_type_and_declared_capacity() {
  auto module = parse_and_resolve(
      "fn f(): arr<i32, 3> = array<3>[i32bits(0x00000001), i32bits(0x00000002)];\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  const auto &type = *module.functions[0].body->inferred_type;
  BOUNDFIN_CHECK(type->kind == TypeKind::Array);
  const auto &array_type = std::get<ArrayType>(type->data);
  BOUNDFIN_CHECK(array_type.element->kind == TypeKind::I32);
  BOUNDFIN_CHECK_EQ(array_type.capacity, static_cast<std::uint32_t>(3));
}

void TYP008_heterogeneous_array_literal_is_rejected() {
  auto module = parse_and_resolve(
      "fn f(): arr<i32, 2> = array<2>[i32bits(0x00000001), i64bits(0x0000000000000002)];\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP008"));
}

// AM-020: an empty array literal has no element to synthesize an element
// type from; T-Array yields no derivation, so typecheck rejects it (TYP008,
// chosen over minting a new code). grammar.ebnf and the Phase 2.2 parser
// both accept "array<N>[]" syntactically -- this is a Phase 3.2 rejection,
// not a parse-time one.
void TYP008_empty_array_literal_is_rejected() {
  auto module = parse_and_resolve("fn f(): arr<i32, 4> = array<4>[];\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP008"));
}

// T-Len/T-Index: "array: arr<tau, N>; index: i32" -> "length: i32; index result: tau".
void TLen_result_type_is_i32() {
  auto module = parse_and_resolve(
      "fn f(): i32 = len(array<2>[i32bits(0x00000001), i32bits(0x00000002)]);\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I32);
}

void TYP009_len_rejects_non_array_operand() {
  auto module = parse_and_resolve("fn f(): i32 = len(i32bits(0x00000001));\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP009"));
}

void TIndex_result_type_is_the_element_type() {
  auto module = parse_and_resolve(
      "fn f(): i32 = array<2>[i32bits(0x00000001), i32bits(0x00000002)][i32bits(0x00000000)];\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK((*module.functions[0].body->inferred_type)->kind == TypeKind::I32);
}

void TYP009_index_rejects_non_i32_index_operand() {
  auto module = parse_and_resolve(
      "fn f(): i32 = array<2>[i32bits(0x00000001), i32bits(0x00000002)][true];\nexport f;\n");
  const auto result = typecheck_module(module);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("TYP009"));
}

void boundfin_typecheck_arrays() {
  TArray_result_type_is_arr_of_element_type_and_declared_capacity();
  TYP008_heterogeneous_array_literal_is_rejected();
  TYP008_empty_array_literal_is_rejected();
  TLen_result_type_is_i32();
  TYP009_len_rejects_non_array_operand();
  TIndex_result_type_is_the_element_type();
  TYP009_index_rejects_non_i32_index_operand();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_typecheck_arrays)
