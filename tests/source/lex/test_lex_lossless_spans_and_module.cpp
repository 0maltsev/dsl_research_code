#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <string>
#include <string_view>

namespace {

using boundfin::source::lex::TokenKind;
using boundfin::source::lex::tokenize;

// "Lossless spans" (PLAN.md Phase 2 "Deliverables"): every token's span is
// exactly its own text in the source, consecutive token spans never
// overlap, and the gap between any two consecutive spans (and between 0 and
// the first span, and the last span and EndOfFile) is exactly the
// whitespace between them -- nothing is lost or approximated.
void every_token_span_exactly_reconstructs_its_own_text() {
  const std::string source = "fn id(x: i32): i32 = x;\nexport id;";
  const auto result = tokenize(source);
  BOUNDFIN_CHECK(result.ok);

  for (const auto &token : result.tokens) {
    if (token.kind == TokenKind::EndOfFile) {
      BOUNDFIN_CHECK_EQ(token.span.start, source.size());
      BOUNDFIN_CHECK_EQ(token.span.end, source.size());
      continue;
    }
    BOUNDFIN_CHECK(token.span.start < token.span.end);
    BOUNDFIN_CHECK(token.span.end <= source.size());
  }
}

void consecutive_spans_are_ordered_and_non_overlapping() {
  const std::string source = "fn id(x: i32): i32 = x;\nexport id;";
  const auto result = tokenize(source);
  BOUNDFIN_CHECK(result.ok);

  for (std::size_t i = 1; i < result.tokens.size(); ++i) {
    BOUNDFIN_CHECK(result.tokens[i - 1].span.end <= result.tokens[i].span.start);
  }
}

void keyword_and_identifier_spans_recover_exact_source_text() {
  const std::string source = "fn add(a: i32, b: i32): i32 = a + b;\nexport add;";
  const auto result = tokenize(source);
  BOUNDFIN_CHECK(result.ok);

  BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::KwFn);
  BOUNDFIN_CHECK_EQ(std::string_view(source).substr(result.tokens[0].span.start, result.tokens[0].span.length()),
                     std::string_view("fn"));

  BOUNDFIN_CHECK(result.tokens[1].kind == TokenKind::Identifier);
  BOUNDFIN_CHECK_EQ(std::string_view(source).substr(result.tokens[1].span.start, result.tokens[1].span.length()),
                     std::string_view("add"));
}

// A small but complete module exercising every grammar.ebnf expression form
// in one source text, tokenizing end to end with no lexical diagnostic.
void a_realistic_module_tokenizes_cleanly() {
  const std::string source = R"(fn clamp(x: i32, lo: i32, hi: i32): i32 =
  if x < lo then lo else if x > hi then hi else x;

fn sum_to(n: arr<i32, 8>): i32 =
  fold<8>(n; acc, e; 0; acc + e);

fn make(n: i32): arr<i32, 4> =
  build<4>(n; i; i * n);

fn first(p: prod<i32, i32>): i32 =
  proj<1>(p);

fn check(a: i32, b: i32): bool =
  a == b && !(a != b) || a <= b;

fn magnitude(x: f64): f64 =
  abs(x);

fn at(xs: arr<i32, 4>, i: i32): i32 =
  xs[i];

fn consts(): prod<bool, i32, i64, f64> =
  (true, i32bits(0x0000002A), i64bits(0x000000000000002A), f64bits(0x4005000000000000));

export clamp;
)";
  const auto result = tokenize(source);
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(!result.tokens.empty());
  BOUNDFIN_CHECK(result.tokens.back().kind == TokenKind::EndOfFile);
}

void boundfin_lex_lossless_spans_and_module() {
  every_token_span_exactly_reconstructs_its_own_text();
  consecutive_spans_are_ordered_and_non_overlapping();
  keyword_and_identifier_spans_recover_exact_source_text();
  a_realistic_module_tokenizes_cleanly();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_lossless_spans_and_module)
