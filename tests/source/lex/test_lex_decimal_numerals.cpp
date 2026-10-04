#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <cstdint>
#include <string>

namespace {

using boundfin::source::lex::TokenKind;
using boundfin::source::lex::tokenize;

void LEX006_decimal_numeral_known_answers() {
  // Valid: "0" alone, and any nonzero-leading run of digits.
  {
    const auto result = tokenize("0");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::DecimalNumeral);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0});
  }
  {
    const auto result = tokenize("2147483647"); // 2^31 - 1, the admissible maximum
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::DecimalNumeral);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{2147483647});
  }
  {
    const auto result = tokenize("42");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{42});
  }
}

void LEX006_forbidden_leading_zero() {
  for (const std::string text : {"00", "01", "007", "0123"}) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK(result.diagnostic.has_value());
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX006"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, text.size());
  }
}

void LEX006_lexical_overflow_beyond_admissible_range() {
  // 2^31 = 2147483648, one past the admissible maximum.
  {
    const auto result = tokenize("2147483648");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX006"));
  }
  // A numeral far too large for any fixed-width accumulator must still be
  // rejected deterministically, without undefined behavior.
  {
    const auto result = tokenize("999999999999999999999999999999");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX006"));
  }
}

void decimal_numeral_maximal_munch_stops_at_non_digit() {
  const auto result = tokenize("42x");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{3}); // 42, x, EOF
  BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::DecimalNumeral);
  BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, std::size_t{2});
  BOUNDFIN_CHECK(result.tokens[1].kind == TokenKind::Identifier);
}

void boundfin_lex_decimal_numerals() {
  LEX006_decimal_numeral_known_answers();
  LEX006_forbidden_leading_zero();
  LEX006_lexical_overflow_beyond_admissible_range();
  decimal_numeral_maximal_munch_stops_at_non_digit();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_decimal_numerals)
