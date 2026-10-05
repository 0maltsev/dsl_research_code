#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <string>

namespace {

using boundfin::source::parse::parse_module;

// parse_module() tokenizes internally; a lexical failure anywhere in the
// source must surface as that exact LEX code, not be swallowed or
// relabeled as a generic SYN error.
void lexical_diagnostics_propagate_through_parse_module() {
  {
    // LEX006: forbidden leading zero, inside a capacity position.
    const auto result = parse_module("fn f(xs: arr<i32, 04>): i32 = i32bits(0x00000001);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX006"));
  }
  {
    // LEX004: wrong hex width, mid-expression.
    const auto result = parse_module("fn f(): i32 = i32bits(0x1);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX004"));
  }
  {
    // LEX007: a decimal float attempt inside an otherwise well-formed call.
    const auto result = parse_module("fn f(): i32 = g(3.14);\nfn g(x: i32): i32 = x;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX007"));
  }
  {
    // LEX002: a non-ASCII character (U+00E9) used as part of a function name.
    const std::string source =
        "fn " + std::string({static_cast<char>(0xC3), static_cast<char>(0xA9)}) + "(): i32 = i32bits(0x00000001);\nexport f;\n";
    const auto result = parse_module(source);
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX002"));
  }
}

void boundfin_parse_lex_error_propagation() { lexical_diagnostics_propagate_through_parse_module(); }

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_lex_error_propagation)
