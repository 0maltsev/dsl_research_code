#include "boundfin/source/ast/ast.hpp"
#include "boundfin/source/parse/parser.hpp"
#include "boundfin_test.hpp"

#include <cstdint>
#include <string>

namespace {

using namespace boundfin::source::ast;
using boundfin::source::parse::parse_module;

const Expr &function_body(const Module &module, std::size_t index = 0) { return *module.functions[index].body; }

void boolean_literals() {
  {
    const auto result = parse_module("fn f(): bool = true;\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &literal = std::get<LiteralExpr>(function_body(*result.module).data);
    BOUNDFIN_CHECK(literal.kind == LiteralKind::Bool);
    BOUNDFIN_CHECK_EQ(literal.value, std::uint64_t{1});
  }
  {
    const auto result = parse_module("fn f(): bool = false;\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &literal = std::get<LiteralExpr>(function_body(*result.module).data);
    BOUNDFIN_CHECK_EQ(literal.value, std::uint64_t{0});
  }
}

void bit_literals_every_width() {
  const auto result = parse_module("fn f(): prod<i32, i64, f64> = "
                                    "(i32bits(0xDEADBEEF), i64bits(0x0123456789ABCDEF), f64bits(0x7FF8000000000000));\n"
                                    "export f;\n");
  BOUNDFIN_CHECK(result.ok);
  const auto &components = std::get<ProductExpr>(function_body(*result.module).data).components;
  BOUNDFIN_CHECK_EQ(components.size(), std::size_t{3});

  const auto &i32_literal = std::get<LiteralExpr>(components[0]->data);
  BOUNDFIN_CHECK(i32_literal.kind == LiteralKind::I32Bits);
  BOUNDFIN_CHECK_EQ(i32_literal.value, std::uint64_t{0xDEADBEEFULL});

  const auto &i64_literal = std::get<LiteralExpr>(components[1]->data);
  BOUNDFIN_CHECK(i64_literal.kind == LiteralKind::I64Bits);
  BOUNDFIN_CHECK_EQ(i64_literal.value, std::uint64_t{0x0123456789ABCDEFULL});

  const auto &f64_literal = std::get<LiteralExpr>(components[2]->data);
  BOUNDFIN_CHECK(f64_literal.kind == LiteralKind::F64Bits);
  BOUNDFIN_CHECK_EQ(f64_literal.value, std::uint64_t{0x7FF8000000000000ULL}); // numeric-semantics.md canonical NaN
}

void wrong_bit_literal_width_is_a_syntax_error() {
  // The lexer accepts an 8-digit HexNumeral on its own (AM-017 context);
  // the parser is what knows i64bits requires 16, so this is a SYN
  // mismatch, not a lexical one.
  const auto result = parse_module("fn f(): i64 = i64bits(0xDEADBEEF);\nexport f;\n");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN001"));
}

void bare_hex_numeral_without_bit_literal_wrapper_is_rejected() {
  const auto result = parse_module("fn f(): i32 = 0xDEADBEEF;\nexport f;\n");
  BOUNDFIN_CHECK(!result.ok);
  BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("SYN001"));
}

void var_and_call_including_nullary() {
  {
    const auto result = parse_module("fn f(x: i32): i32 = x;\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &var = std::get<VarExpr>(function_body(*result.module).data);
    BOUNDFIN_CHECK_EQ(var.name, std::string("x"));
  }
  {
    const auto result = parse_module("fn f(): i32 = g();\nfn g(): i32 = i32bits(0x00000001);\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &call = std::get<CallExpr>(function_body(*result.module).data);
    BOUNDFIN_CHECK_EQ(call.callee, std::string("g"));
    BOUNDFIN_CHECK(call.arguments.empty());
  }
  {
    const auto result = parse_module("fn f(a: i32, b: i32): i32 = g(a, b);\n"
                                      "fn g(x: i32, y: i32): i32 = x;\nexport f;\n");
    BOUNDFIN_CHECK(result.ok);
    const auto &call = std::get<CallExpr>(function_body(*result.module).data);
    BOUNDFIN_CHECK_EQ(call.arguments.size(), std::size_t{2});
  }
}

void LEX005_reserved_word_where_identifier_required() {
  // Parameter name.
  {
    const auto result = parse_module("fn f(let: i32): i32 = i32bits(0x00000001);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX005"));
  }
  // Function name.
  {
    const auto result = parse_module("fn if(): i32 = i32bits(0x00000001);\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX005"));
  }
  // Let-bound name.
  {
    const auto result = parse_module("fn f(): i32 = let fn = i32bits(0x00000001) in fn;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX005"));
  }
  // Export target.
  {
    const auto result = parse_module("fn f(): i32 = i32bits(0x00000001);\nexport let;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX005"));
  }
  // A keyword used where a primary expression (a variable reference) is
  // required.
  {
    const auto result = parse_module("fn f(): i32 = true + true;\nexport f;\n");
    // "true"/"false" ARE valid primary-expression literals, not this case;
    // use a keyword with no expression meaning instead.
    BOUNDFIN_CHECK(result.ok);
  }
  {
    const auto result = parse_module("fn f(): i32 = fn;\nexport f;\n");
    BOUNDFIN_CHECK(!result.ok);
    BOUNDFIN_CHECK_EQ(result.diagnostic->code, std::string("LEX005"));
  }
}

void boundfin_parse_literals_and_names() {
  boolean_literals();
  bit_literals_every_width();
  wrong_bit_literal_width_is_a_syntax_error();
  bare_hex_numeral_without_bit_literal_wrapper_is_rejected();
  var_and_call_including_nullary();
  LEX005_reserved_word_where_identifier_required();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_parse_literals_and_names)
