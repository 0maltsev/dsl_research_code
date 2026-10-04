#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::lex::tokenize;

void LEX003_lone_ampersand() {
  for (const std::string text : {"&", "&x", "& &"}) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX003"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{1});
  }
}

void LEX003_lone_pipe() {
  for (const std::string text : {"|", "|x", "| |"}) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX003"));
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.start, std::size_t{0});
    BOUNDFIN_CHECK_EQ(result.diagnostic->span.end, std::size_t{1});
  }
}

void tokens_recognized_before_the_failure_are_retained() {
  // First-error behavior: everything strictly before the failing character
  // is still reported in `tokens`, for partial-recovery tooling.
  const auto result = tokenize("x + &");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX003"));
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2}); // x, +
}

void boundfin_lex_malformed_tokens() {
  LEX003_lone_ampersand();
  LEX003_lone_pipe();
  tokens_recognized_before_the_failure_are_retained();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_malformed_tokens)
