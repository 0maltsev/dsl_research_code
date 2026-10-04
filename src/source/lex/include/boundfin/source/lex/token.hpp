#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace boundfin::source::lex {

// Half-open byte-offset range [start, end) into the original source buffer.
// Every token and diagnostic in this module is located by one of these;
// together with the source bytes they make every token's text exactly
// reconstructible ("lossless spans" -- PLAN.md Phase 2 "Deliverables").
struct SourceSpan {
  std::size_t start = 0;
  std::size_t end = 0;

  [[nodiscard]] std::size_t length() const { return end - start; }
};

[[nodiscard]] inline bool operator==(const SourceSpan &lhs, const SourceSpan &rhs) {
  return lhs.start == rhs.start && lhs.end == rhs.end;
}

// One terminal of grammar.ebnf's concrete syntax. Every quoted terminal in
// the grammar has exactly one TokenKind; surface elaboration (grammar.ebnf,
// "Mandatory elaboration before typing/size/cost analysis") happens later,
// in the parser/resolver, never here.
enum class TokenKind {
  // Reserved words (grammar.ebnf lexical constraint 4), recognized by
  // identifier-shaped maximal munch, then keyword lookup.
  KwFn,
  KwExport,
  KwBool,
  KwI32,
  KwI64,
  KwF64,
  KwArr,
  KwProd,
  KwLet,
  KwIn,
  KwIf,
  KwThen,
  KwElse,
  KwFold,
  KwBuild,
  KwArray,
  KwLen,
  KwProj,
  KwAbs,
  KwTrue,
  KwFalse,
  KwI32Bits,
  KwI64Bits,
  KwF64Bits,

  Identifier,

  // A maximal run of decimal digits with no forbidden leading zero and a
  // value in [0, 2^31-1] (SPEC_FREEZE.md "Frozen language boundary":
  // "Capacities are decimal natural literals in [0, 2^31-1]" -- the only
  // semantic use of a bare decimal numeral anywhere in grammar.ebnf is as a
  // `capacity` or `positive-decimal`, both reducing to this one lexical
  // shape). `numeral_value` holds the decoded value.
  DecimalNumeral,

  // "0x" followed by exactly 8 or exactly 16 hex digits (grammar.ebnf
  // `hex8`/`hex16`): the only two lexically valid widths, since "0x" never
  // appears outside an i32/i64/f64 bit literal. Matching a width-8 numeral
  // to `i32bits` versus a width-16 one to `i64bits`/`f64bits` is a parser
  // concern (SYN-level), not this lexer's. `numeral_value` and
  // `hex_digit_width` hold the decoded value and width (8 or 16).
  HexNumeral,

  // Punctuation and operators (grammar.ebnf terminals). Context-free: "<"
  // and ">" mean the same token whether used as a relational operator or as
  // a generic-style delimiter in `arr<...>`/`fold<...>`/`proj<...>`; the
  // parser disambiguates by production, not this lexer.
  LParen,
  RParen,
  LBracket,
  RBracket,
  Less,
  Greater,
  Comma,
  Colon,
  Semicolon,
  Equal,
  EqualEqual,
  BangEqual,
  LessEqual,
  GreaterEqual,
  AmpAmp,
  PipePipe,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Bang,

  EndOfFile,
};

[[nodiscard]] std::string to_string(TokenKind kind);

struct Token {
  TokenKind kind = TokenKind::EndOfFile;
  SourceSpan span;

  // Meaningful only for DecimalNumeral and HexNumeral; 0 otherwise.
  std::uint64_t numeral_value = 0;

  // Meaningful only for HexNumeral (8 or 16); 0 otherwise.
  int hex_digit_width = 0;
};

} // namespace boundfin::source::lex
