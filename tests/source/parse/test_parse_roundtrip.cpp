#include "boundfin/source/ast/print.hpp"
#include "boundfin/source/ast/serialize.hpp"
#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <array>
#include <sstream>
#include <string>

namespace {

using boundfin::source::ast::print;
using boundfin::source::ast::serialize;
using boundfin::source::parse::parse_module;

// "generated parse/print/parse cases" (PLAN.md Phase 2 "Tests"): parse
// `source`, print the AST back to concrete syntax, reparse the printed
// text, and require the two ASTs to be structurally identical (compared via
// span-free deterministic serialization, since the printed text is freshly
// rendered and its byte offsets never equal the original source's).
void assert_roundtrips(const std::string &label, const std::string &source) {
  const auto first = parse_module(source);
  if (!first.ok) {
    throw boundfin::testing::CheckFailure(label + ": source did not parse: " + first.diagnostic->code + " " +
                                           first.diagnostic->message);
  }
  const std::string printed = print(*first.module);
  const auto second = parse_module(printed);
  if (!second.ok) {
    throw boundfin::testing::CheckFailure(label + ": printed text did not reparse: " + second.diagnostic->code +
                                           " " + second.diagnostic->message + "\nprinted:\n" + printed);
  }
  const auto json1 = serialize(*first.module, /*include_spans=*/false);
  const auto json2 = serialize(*second.module, /*include_spans=*/false);
  if (json1 != json2) {
    throw boundfin::testing::CheckFailure(label + ": AST changed across the round trip\nprinted:\n" + printed +
                                           "\nfirst:  " + json1.dump() + "\nsecond: " + json2.dump());
  }
}

void every_production_round_trips() {
  const std::array<std::pair<std::string, std::string>, 17> cases = {{
      {"scalar types and identity", "fn id(x: i32): i32 = x;\nexport id;\n"},
      {"every scalar parameter type",
       "fn f(a: bool, b: i32, c: i64, d: f64): bool = a;\nexport f;\n"},
      {"array and product types",
       "fn f(xs: arr<i32, 4>, p: prod<i32, i64, f64>): bool = true;\nexport f;\n"},
      {"nullary function and call", "fn f(): i32 = g();\nfn g(): i32 = i32bits(0x00000001);\nexport f;\n"},
      {"multi-arg call", "fn f(a: i32, b: i32): i32 = g(a, b);\nfn g(x: i32, y: i32): i32 = x;\nexport f;\n"},
      {"every binary primitive",
       "fn f(a: i32, b: i32): i32 = a + b - a * b / b % a;\nexport f;\n"},
      {"logic and comparison",
       "fn f(a: i32, b: i32): bool = a == b && !(a != b) || a <= b;\nexport f;\n"},
      {"unary negation", "fn f(x: i32): i32 = -x;\nexport f;\n"},
      {"let expression", "fn f(x: i32): i32 = let y = x + i32bits(0x00000001) in y;\nexport f;\n"},
      {"if expression", "fn f(a: i32, b: i32): i32 = if a < b then a else b;\nexport f;\n"},
      {"fold expression",
       "fn f(n: i32): i32 = fold<16>(n; acc, idx; i32bits(0x00000000); acc + idx);\nexport f;\n"},
      {"build expression", "fn f(n: i32): arr<i32, 4> = build<4>(n; idx; idx * n);\nexport f;\n"},
      {"array literal",
       "fn f(): arr<i32, 3> = array<3>[i32bits(0x00000001), i32bits(0x00000002), i32bits(0x00000003)];\nexport f;\n"},
      {"product expression and every bit width",
       "fn f(): prod<i32, i64, f64> = "
       "(i32bits(0xDEADBEEF), i64bits(0x0123456789ABCDEF), f64bits(0x7FF8000000000000));\nexport f;\n"},
      {"len/proj/index", "fn f(xs: arr<i32, 4>, p: prod<i32, i32>): i32 = proj<1>(p) + xs[i32bits(0x00000000)] + len(xs);\nexport f;\n"},
      {"abs", "fn f(x: f64): f64 = abs(x);\nexport f;\n"},
      {"an operator operand requiring parens around let/if",
       "fn f(x: i32): i32 = x + (let y = x in y) + (if x < x then x else x);\nexport f;\n"},
  }};
  for (const auto &[label, source] : cases) {
    assert_roundtrips(label, source);
  }
}

// A single larger module exercising many productions together, closer to
// realistic source than the per-production cases above.
void a_realistic_module_round_trips() {
  const std::string source = "fn clamp(x: i32, lo: i32, hi: i32): i32 =\n"
                              "  if x < lo then lo else if x > hi then hi else x;\n"
                              "\n"
                              "fn sum_to(n: i32): i32 =\n"
                              "  fold<8>(n; acc, idx; i32bits(0x00000000); acc + idx);\n"
                              "\n"
                              "fn make(n: i32): arr<i32, 4> =\n"
                              "  build<4>(n; idx; idx * n);\n"
                              "\n"
                              "fn first(p: prod<i32, i32>): i32 =\n"
                              "  proj<1>(p);\n"
                              "\n"
                              "fn check(a: i32, b: i32): bool =\n"
                              "  a == b && !(a != b) || a <= b;\n"
                              "\n"
                              "fn magnitude(x: f64): f64 =\n"
                              "  abs(x);\n"
                              "\n"
                              "fn at(xs: arr<i32, 4>, i: i32): i32 =\n"
                              "  xs[i];\n"
                              "\n"
                              "export clamp;\n";
  assert_roundtrips("realistic module", source);
}

void boundfin_parse_roundtrip() {
  every_production_round_trips();
  a_realistic_module_round_trips();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_roundtrip)
