#pragma once

#include "boundfin/source/lex/token.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace boundfin::source::ast {

using boundfin::source::lex::SourceSpan;

// A stable identity for one name-introducing binding occurrence (a
// parameter, a let-bound name, a fold accumulator/index, or a build index),
// assigned by src/source/resolve (Phase 3.1). grammar.ebnf's static
// constraint 4 requires these names to be "resolved and alpha-renamed":
// two bindings that happen to share a surface spelling (lexical shadowing)
// get different BindingIds, and every use (VarExpr/CallExpr) records which
// specific binding/declaration it resolves to. Unset (std::nullopt) until
// resolution runs; the parser never sets these.
using BindingId = std::uint64_t;

// --- Types (grammar.ebnf `type`) -------------------------------------------

enum class TypeKind { Bool, I32, I64, F64, Array, Product };

struct Type;
using TypePtr = std::unique_ptr<Type>;

struct ArrayType {
  TypePtr element;
  std::uint32_t capacity = 0;
};

// Arity at least two (AM-002: "product types and values have arity at least
// two; grouping parentheses are not products").
struct ProductType {
  std::vector<TypePtr> components;
};

struct Type {
  TypeKind kind = TypeKind::Bool;
  SourceSpan span;
  // Empty for scalar kinds (Bool/I32/I64/F64).
  std::variant<std::monostate, ArrayType, ProductType> data;
};

// --- Expressions (grammar.ebnf `expression`) --------------------------------
//
// Surface operators are already elaborated to UnaryPrimitiveExpr/
// BinaryPrimitiveExpr here (grammar.ebnf "Mandatory elaboration before
// typing/size/cost analysis"; AM-001: elaboration is one-for-one and never
// optimizes or reorders). The elaborated primitive is type-generic (e.g.
// `Add`, not yet `add32`/`add64`/`fadd`): resolving to the exact monomorphic
// primitive needs operand types, which this untyped AST does not have --
// that is Phase 3's job (Table 7 typing rules).

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct LetExpr {
  std::string name;
  SourceSpan name_span;
  ExprPtr bound;
  ExprPtr body;
  std::optional<BindingId> binding = std::nullopt; // set by src/source/resolve
};

struct IfExpr {
  ExprPtr condition;
  ExprPtr then_branch;
  ExprPtr else_branch;
};

// grammar.ebnf: `fold<capacity>(count; accumulator_name, index_name; initial; body)`.
// cost-trace-semantics.md "Fold": "The count and initial accumulator
// evaluate left-to-right... its index maps to zero" -- confirms the second
// binder is a loop index (0-based), not an array element: this is a
// count-bounded fold, not an array fold.
struct FoldExpr {
  std::uint32_t capacity = 0;
  ExprPtr count;
  std::string accumulator_name;
  SourceSpan accumulator_name_span;
  std::string index_name;
  SourceSpan index_name_span;
  ExprPtr initial;
  ExprPtr body;
  std::optional<BindingId> accumulator_binding = std::nullopt; // set by src/source/resolve
  std::optional<BindingId> index_binding = std::nullopt;       // set by src/source/resolve
};

// grammar.ebnf: `build<capacity>(count; index_name; body)`.
struct BuildExpr {
  std::uint32_t capacity = 0;
  ExprPtr count;
  std::string index_name;
  SourceSpan index_name_span;
  ExprPtr body;
  std::optional<BindingId> index_binding = std::nullopt; // set by src/source/resolve
};

enum class UnaryPrimitiveOp { Not, Neg, Abs };
struct UnaryPrimitiveExpr {
  UnaryPrimitiveOp op = UnaryPrimitiveOp::Not;
  ExprPtr operand;
};

enum class BinaryPrimitiveOp { And, Or, Add, Sub, Mul, Div, Rem, Eq, Ne, Lt, Le, Gt, Ge };
struct BinaryPrimitiveExpr {
  BinaryPrimitiveOp op = BinaryPrimitiveOp::Add;
  ExprPtr lhs;
  ExprPtr rhs;
};

// A bare identifier with no call parens (grammar.ebnf `name-expression`
// without the optional "(" argument-list ")").
struct VarExpr {
  std::string name;
  SourceSpan name_span;
  std::optional<BindingId> resolved_binding = std::nullopt; // set by src/source/resolve
};

// An identifier with call parens present, zero or more arguments (AM-002:
// "Nullary functions remain permitted").
struct CallExpr {
  std::string callee;
  SourceSpan callee_span;
  std::vector<ExprPtr> arguments;
  // The callee's 0-based declaration rank (source order): set by
  // src/source/resolve once it has confirmed the call target is declared
  // strictly before the caller (paper Sec. 4.1's "f precedes_M g" rule;
  // "f (x1:t1,...): t = e" is declaration f, "precedes_M" is strict
  // source order) holds for this call.
  std::optional<std::size_t> resolved_callee_rank = std::nullopt;
};

struct ArrayLiteralExpr {
  std::uint32_t capacity = 0;
  std::vector<ExprPtr> elements;
};

// `parenthesized-expression` with two or more comma-separated components
// (AM-002: arity at least two). A single parenthesized expression with no
// comma is transparent grouping and produces no AST node of its own.
struct ProductExpr {
  std::vector<ExprPtr> components;
};

struct LenExpr {
  ExprPtr array;
};

// `index` is one-based (grammar.ebnf `positive-decimal`; AM-002: "`proj<j>(e)`
// uses one-based `j`").
struct ProjExpr {
  std::uint32_t index = 1;
  ExprPtr operand;
};

// Postfix `array[index]`; `postfix-expression` repeats this left-associatively.
struct IndexExpr {
  ExprPtr array;
  ExprPtr index;
};

enum class LiteralKind { Bool, I32Bits, I64Bits, F64Bits };
struct LiteralExpr {
  LiteralKind kind = LiteralKind::Bool;
  // Bool: 0 or 1. I32Bits: low 32 bits significant. I64Bits/F64Bits: all 64
  // bits significant (F64Bits is a raw bit pattern, not a parsed double --
  // numeric-semantics.md governs its interpretation, not this AST).
  std::uint64_t value = 0;
};

enum class ExprKind {
  Let,
  If,
  Fold,
  Build,
  UnaryPrimitive,
  BinaryPrimitive,
  Var,
  Call,
  ArrayLiteral,
  Product,
  Len,
  Proj,
  Index,
  Literal,
};

struct Expr {
  ExprKind kind;
  SourceSpan span;
  std::variant<LetExpr, IfExpr, FoldExpr, BuildExpr, UnaryPrimitiveExpr, BinaryPrimitiveExpr, VarExpr, CallExpr,
               ArrayLiteralExpr, ProductExpr, LenExpr, ProjExpr, IndexExpr, LiteralExpr>
      data;
};

// --- Declarations and module (grammar.ebnf `module`) ------------------------

struct Parameter {
  std::string name;
  SourceSpan name_span;
  TypePtr type;
  SourceSpan span;
  std::optional<BindingId> binding = std::nullopt; // set by src/source/resolve
};

struct FunctionDecl {
  std::string name;
  SourceSpan name_span;
  std::vector<Parameter> parameters;
  TypePtr result_type;
  ExprPtr body;
  SourceSpan span;
};

struct ExportDecl {
  std::string name;
  SourceSpan name_span;
  SourceSpan span;
  // The exported function's 0-based declaration rank: set by
  // src/source/resolve once it has confirmed the export names a declared
  // function (NAM005 otherwise).
  std::optional<std::size_t> resolved_target_rank = std::nullopt;
};

struct Module {
  std::vector<FunctionDecl> functions;
  ExportDecl export_decl;
  SourceSpan span;
};

} // namespace boundfin::source::ast
