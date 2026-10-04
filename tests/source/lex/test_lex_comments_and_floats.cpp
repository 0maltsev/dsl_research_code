#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::lex::tokenize;

void LEX007_decimal_floating_literal_attempts() {
  // digits '.' digits
  {
    const auto result = tokenize("3.14");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{4});
  }
  // digits '.' with nothing after
  {
    const auto result = tokenize("3.");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{2});
  }
  // '.' digits, nothing before
  {
    const auto result = tokenize(".5");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{2});
  }
  // an admissible capacity followed by a float continuation still triggers
  // LEX007 rather than being accepted as two tokens.
  {
    const auto result = tokenize("arr<i32, 4.0>");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
  }
}

void LEX007_source_comment_attempts() {
  {
    const auto result = tokenize("// comment");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{2});
  }
  {
    const auto result = tokenize("/* comment */");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{2});
  }
}

void bare_dot_with_no_adjacent_digit_is_outside_the_permitted_set() {
  // '.' alone is not part of any token; with no digit on either side it is
  // not a floating-literal attempt either, so it is LEX002, not LEX007.
  const auto result = tokenize("x . y");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
}

void plain_division_is_unaffected() {
  const auto result = tokenize("x / y");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{4}); // x, /, y, EOF
}

void boundfin_lex_comments_and_floats() {
  LEX007_decimal_floating_literal_attempts();
  LEX007_source_comment_attempts();
  bare_dot_with_no_adjacent_digit_is_outside_the_permitted_set();
  plain_division_is_unaffected();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_comments_and_floats)
