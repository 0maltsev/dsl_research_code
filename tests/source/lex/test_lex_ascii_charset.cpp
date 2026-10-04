#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::lex::tokenize;

void LEX002_ascii_characters_outside_any_token() {
  // None of these ASCII characters appear in any grammar.ebnf terminal.
  for (const char c : {'@', '#', '$', '^', '~', '`', '\\', '?', '\'', '"'}) {
    const std::string text(1, c);
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{1});
  }
}

void LEX002_control_characters_that_are_not_permitted_whitespace() {
  // Only space, tab, LF, and CR are whitespace (grammar.ebnf
  // `whitespace-character`); every other ASCII control character is LEX002.
  for (const char c : {'\0', '\a', '\b', '\v', '\f'}) {
    const std::string text(1, c);
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
  }
}

void LEX002_delete_character() {
  const std::string text(1, static_cast<char>(0x7F));
  const auto result = tokenize(text);
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
}

void permitted_whitespace_characters_are_accepted() {
  const auto result = tokenize(" \t\n\r;");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2}); // ;, EOF
}

// Well-formed, non-shortest-form-violating UTF-8 that simply isn't ASCII is
// LEX002 (not LEX001, which is about byte-sequence malformedness and the
// BOM specifically). Byte sequences cross-checked against Node's strict
// `TextDecoder` before being written here.
void LEX002_well_formed_non_ascii_utf8() {
  const std::string e_acute = {static_cast<char>(0xC3), static_cast<char>(0xA9)}; // U+00E9, 2 bytes
  const std::string cjk_zhong = {static_cast<char>(0xE4), static_cast<char>(0xB8), static_cast<char>(0xAD)}; // U+4E2D, 3 bytes
  const std::string emoji = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                              static_cast<char>(0x80)}; // U+1F600, 4 bytes

  {
    const auto result = tokenize("x" + e_acute);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{1});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{3}); // spans the full 2-byte codepoint
  }
  {
    const auto result = tokenize("x" + cjk_zhong);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{4}); // spans the full 3-byte codepoint
  }
  {
    const auto result = tokenize("x" + emoji);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{5}); // spans the full 4-byte codepoint
  }
}

void boundfin_lex_ascii_charset() {
  LEX002_ascii_characters_outside_any_token();
  LEX002_control_characters_that_are_not_permitted_whitespace();
  LEX002_delete_character();
  permitted_whitespace_characters_are_accepted();
  LEX002_well_formed_non_ascii_utf8();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_ascii_charset)
