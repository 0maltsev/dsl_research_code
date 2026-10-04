#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <cstdint>
#include <string>

namespace {

using boundfin::source::lex::TokenKind;
using boundfin::source::lex::tokenize;

void valid_hex_numeral_widths() {
  // i32-width: exactly 8 hex digits. Mixed case is permitted for the digits
  // themselves (hex-digit = decimal-digit | a-f | A-F); only the "0x"
  // prefix is case-exact.
  {
    const auto result = tokenize("0xDEADBEEF");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2});
    BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::HexNumeral);
    BOUNDFIN_CHECK_EQ(result.tokens[0].hex_digit_width, 8);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0xDEADBEEFULL});
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, std::size_t{10});
  }
  {
    const auto result = tokenize("0x00000000");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0});
    BOUNDFIN_CHECK_EQ(result.tokens[0].hex_digit_width, 8);
  }
  // i64/f64-width: exactly 16 hex digits.
  {
    const auto result = tokenize("0x0123456789abcdef");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens[0].hex_digit_width, 16);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0x0123456789abcdefULL});
  }
  {
    const auto result = tokenize("0x7FF8000000000000"); // canonical NaN bit pattern, numeric-semantics.md
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens[0].hex_digit_width, 16);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0x7FF8000000000000ULL});
  }
}

void LEX004_wrong_width() {
  for (const std::string text : {"0x1", "0x1234567", "0x123456789", "0x0123456789abcde", "0x0123456789abcdef0"}) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX004"));
  }
}

void LEX004_no_hex_digits_at_all() {
  const auto result = tokenize("0x");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX004"));
  BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
  BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{2});
}

void LEX004_non_hex_character_truncates_the_run() {
  // "G" is not a hex digit: the run stops after 2 digits, which is neither
  // 8 nor 16, so this is still a width violation.
  const auto result = tokenize("0x12G4");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX004"));
  BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{4}); // "0x12"
}

void zero_followed_by_a_non_x_letter_is_two_ordinary_tokens() {
  // "0x" (lowercase) is the only grammar.ebnf terminal starting with "0";
  // there is no textual basis for treating any other letter after "0" as a
  // malformed bit-literal attempt (a prior version of this lexer did, and a
  // spec-auditor review correctly flagged it as invented behavior with no
  // grammar support). Uppercase "0X" and any other letter are therefore
  // ordinary DecimalNumeral("0") followed by their own Identifier token,
  // exactly like "42x" lexes as DecimalNumeral(42) then Identifier(x).
  {
    const auto result = tokenize("0X1234ABCD");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{3}); // 0, X1234ABCD, EOF
    BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::DecimalNumeral);
    BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0});
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, std::size_t{1});
    BOUNDFIN_CHECK(result.tokens[1].kind == TokenKind::Identifier);
    BOUNDFIN_CHECK_EQ(result.tokens[1].span.start, std::size_t{1});
    BOUNDFIN_CHECK_EQ(result.tokens[1].span.end, std::size_t{10});
  }
  {
    const auto result = tokenize("0z1234");
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{3}); // 0, z1234, EOF
    BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::DecimalNumeral);
    BOUNDFIN_CHECK(result.tokens[1].kind == TokenKind::Identifier);
  }
}

void zero_not_followed_by_a_letter_is_an_ordinary_decimal_numeral() {
  // "0" followed by a digit is a leading-zero LEX006 case, not LEX004 (no
  // letter is involved); already covered in test_lex_decimal_numerals.cpp.
  // "0" followed by punctuation/whitespace/EOF is simply the numeral zero.
  const auto result = tokenize("0;");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::DecimalNumeral);
  BOUNDFIN_CHECK_EQ(result.tokens[0].numeral_value, std::uint64_t{0});
  BOUNDFIN_CHECK(result.tokens[1].kind == TokenKind::Semicolon);
}

void boundfin_lex_bit_literals() {
  valid_hex_numeral_widths();
  LEX004_wrong_width();
  LEX004_no_hex_digits_at_all();
  LEX004_non_hex_character_truncates_the_run();
  zero_followed_by_a_non_x_letter_is_two_ordinary_tokens();
  zero_not_followed_by_a_letter_is_an_ordinary_decimal_numeral();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_bit_literals)
