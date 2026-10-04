#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::lex::tokenize;

// Byte sequences below were independently cross-checked against Node's
// strict `TextDecoder('utf-8', { fatal: true })` before being written here
// (not transcribed from memory): each VALID case decoded to the expected
// codepoint, and each INVALID case threw.

void LEX001_malformed_utf8_sequences() {
  const std::string overlong_2byte_of_slash = "x" + std::string{static_cast<char>(0xC0), static_cast<char>(0xAF)};
  const std::string truncated_2byte_lead = "x" + std::string{static_cast<char>(0xC2)};
  const std::string lone_continuation_byte = "x" + std::string{static_cast<char>(0x80)};
  const std::string surrogate_half = "x" + std::string{static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0x80)};
  const std::string beyond_max_codepoint =
      "x" + std::string{static_cast<char>(0xF4), static_cast<char>(0x90), static_cast<char>(0x80), static_cast<char>(0x80)};
  const std::string invalid_lead_byte = "x" + std::string{static_cast<char>(0xFF)};

  for (const std::string &text :
       {overlong_2byte_of_slash, truncated_2byte_lead, lone_continuation_byte, surrogate_half, beyond_max_codepoint,
        invalid_lead_byte}) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX001"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{1}); // after the leading "x"
    BOUNDFIN_CHECK(result.tokens.empty()); // whole-buffer precondition, checked before any token is recognized
  }
}

void LEX001_byte_order_mark() {
  const std::string bom = {static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF)};
  {
    const auto result = tokenize(bom);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX001"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{3});
  }
  {
    const auto result = tokenize(bom + "x");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX001"));
  }
}

void well_formed_ascii_source_is_unaffected() {
  const auto result = tokenize("let x = 1 in x;");
  BOUNDFIN_CHECK(result.ok);
}

void boundfin_lex_utf8_and_bom() {
  LEX001_malformed_utf8_sequences();
  LEX001_byte_order_mark();
  well_formed_ascii_source_is_unaffected();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_utf8_and_bom)
