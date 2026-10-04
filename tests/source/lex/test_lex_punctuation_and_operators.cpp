#include "boundfin/source/lex/lexer.hpp"
#include "boundfin_test.hpp"

#include <array>
#include <string>

namespace {

using boundfin::source::lex::TokenKind;
using boundfin::source::lex::tokenize;

void every_punctuation_and_operator_terminal_lexes_to_its_own_kind() {
  const std::array<std::pair<std::string, TokenKind>, 22> cases = {{
      {"(", TokenKind::LParen},       {")", TokenKind::RParen},   {"[", TokenKind::LBracket},
      {"]", TokenKind::RBracket},     {"<", TokenKind::Less},     {">", TokenKind::Greater},
      {",", TokenKind::Comma},        {":", TokenKind::Colon},    {";", TokenKind::Semicolon},
      {"=", TokenKind::Equal},        {"==", TokenKind::EqualEqual}, {"!=", TokenKind::BangEqual},
      {"<=", TokenKind::LessEqual},   {">=", TokenKind::GreaterEqual}, {"&&", TokenKind::AmpAmp},
      {"||", TokenKind::PipePipe},    {"+", TokenKind::Plus},     {"-", TokenKind::Minus},
      {"*", TokenKind::Star},         {"/", TokenKind::Slash},    {"%", TokenKind::Percent},
      {"!", TokenKind::Bang},
  }};

  for (const auto &[text, kind] : cases) {
    const auto result = tokenize(text);
    BOUNDFIN_CHECK(result.ok);
    BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{2});
    BOUNDFIN_CHECK(result.tokens[0].kind == kind);
    BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, text.size());
  }
}

void two_char_operators_take_priority_over_their_one_char_prefix() {
  // "<=" is one LessEqual token, never Less followed by Equal.
  const auto result = tokenize("<=x");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{3}); // <=, x, EOF
  BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::LessEqual);
  BOUNDFIN_CHECK_EQ(result.tokens[0].span.start, std::size_t{0});
  BOUNDFIN_CHECK_EQ(result.tokens[0].span.end, std::size_t{2});
}

void whitespace_separates_punctuation_without_being_tokenized() {
  const auto result = tokenize("( )");
  BOUNDFIN_CHECK(result.ok);
  BOUNDFIN_CHECK_EQ(result.tokens.size(), std::size_t{3}); // (, ), EOF
  BOUNDFIN_CHECK(result.tokens[0].kind == TokenKind::LParen);
  BOUNDFIN_CHECK(result.tokens[1].kind == TokenKind::RParen);
  BOUNDFIN_CHECK_EQ(result.tokens[1].span.start, std::size_t{2});
}

void boundfin_lex_punctuation_and_operators() {
  every_punctuation_and_operator_terminal_lexes_to_its_own_kind();
  two_char_operators_take_priority_over_their_one_char_prefix();
  whitespace_separates_punctuation_without_being_tokenized();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_lex_punctuation_and_operators)
