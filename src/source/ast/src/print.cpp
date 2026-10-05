#include "boundfin/source/ast/print.hpp"

#include <array>
#include <sstream>

namespace boundfin::source::ast {

namespace {

std::string print_type(const Type &type);
std::string print_expr(const Expr &expr);

// Wraps an expression in explicit parentheses when it sits in a grammar
// position narrower than full `expression` (an operator operand or the
// indexed-array part of `postfix-expression`): grammar.ebnf's
// `primary-expression`/`unary-expression`/... productions do not include
// let/if/fold/build, so printing one there without parens would not
// reparse. Always parenthesizing (rather than only when structurally
// required) keeps this printer simple and trivially correct; see print.hpp.
std::string print_operand(const Expr &expr) { return "(" + print_expr(expr) + ")"; }

std::string hex_digits(std::uint64_t value, int width) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out(static_cast<std::size_t>(width), '0');
  for (int i = width - 1; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = kHex[value & 0xFU];
    value >>= 4;
  }
  return out;
}

std::string print_type(const Type &type) {
  switch (type.kind) {
  case TypeKind::Bool:
    return "bool";
  case TypeKind::I32:
    return "i32";
  case TypeKind::I64:
    return "i64";
  case TypeKind::F64:
    return "f64";
  case TypeKind::Array: {
    const auto &array_type = std::get<ArrayType>(type.data);
    return "arr<" + print_type(*array_type.element) + ", " + std::to_string(array_type.capacity) + ">";
  }
  case TypeKind::Product: {
    const auto &product_type = std::get<ProductType>(type.data);
    std::string out = "prod<";
    for (std::size_t i = 0; i < product_type.components.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += print_type(*product_type.components[i]);
    }
    out += ">";
    return out;
  }
  }
  return "";
}

const char *binary_op_token(BinaryPrimitiveOp op) {
  switch (op) {
  case BinaryPrimitiveOp::And:
    return "&&";
  case BinaryPrimitiveOp::Or:
    return "||";
  case BinaryPrimitiveOp::Add:
    return "+";
  case BinaryPrimitiveOp::Sub:
    return "-";
  case BinaryPrimitiveOp::Mul:
    return "*";
  case BinaryPrimitiveOp::Div:
    return "/";
  case BinaryPrimitiveOp::Rem:
    return "%";
  case BinaryPrimitiveOp::Eq:
    return "==";
  case BinaryPrimitiveOp::Ne:
    return "!=";
  case BinaryPrimitiveOp::Lt:
    return "<";
  case BinaryPrimitiveOp::Le:
    return "<=";
  case BinaryPrimitiveOp::Gt:
    return ">";
  case BinaryPrimitiveOp::Ge:
    return ">=";
  }
  return "";
}

std::string print_expr(const Expr &expr) {
  switch (expr.kind) {
  case ExprKind::Let: {
    const auto &let_expr = std::get<LetExpr>(expr.data);
    return "let " + let_expr.name + " = " + print_expr(*let_expr.bound) + " in " + print_expr(*let_expr.body);
  }
  case ExprKind::If: {
    const auto &if_expr = std::get<IfExpr>(expr.data);
    return "if " + print_expr(*if_expr.condition) + " then " + print_expr(*if_expr.then_branch) + " else " +
           print_expr(*if_expr.else_branch);
  }
  case ExprKind::Fold: {
    const auto &fold_expr = std::get<FoldExpr>(expr.data);
    return "fold<" + std::to_string(fold_expr.capacity) + ">(" + print_expr(*fold_expr.count) + "; " +
           fold_expr.accumulator_name + ", " + fold_expr.index_name + "; " + print_expr(*fold_expr.initial) + "; " +
           print_expr(*fold_expr.body) + ")";
  }
  case ExprKind::Build: {
    const auto &build_expr = std::get<BuildExpr>(expr.data);
    return "build<" + std::to_string(build_expr.capacity) + ">(" + print_expr(*build_expr.count) + "; " +
           build_expr.index_name + "; " + print_expr(*build_expr.body) + ")";
  }
  case ExprKind::UnaryPrimitive: {
    const auto &unary_expr = std::get<UnaryPrimitiveExpr>(expr.data);
    switch (unary_expr.op) {
    case UnaryPrimitiveOp::Not:
      return "!" + print_operand(*unary_expr.operand);
    case UnaryPrimitiveOp::Neg:
      return "-" + print_operand(*unary_expr.operand);
    case UnaryPrimitiveOp::Abs:
      return "abs(" + print_expr(*unary_expr.operand) + ")";
    }
    return "";
  }
  case ExprKind::BinaryPrimitive: {
    const auto &binary_expr = std::get<BinaryPrimitiveExpr>(expr.data);
    return print_operand(*binary_expr.lhs) + " " + binary_op_token(binary_expr.op) + " " +
           print_operand(*binary_expr.rhs);
  }
  case ExprKind::Var: {
    const auto &var_expr = std::get<VarExpr>(expr.data);
    return var_expr.name;
  }
  case ExprKind::Call: {
    const auto &call_expr = std::get<CallExpr>(expr.data);
    std::string out = call_expr.callee + "(";
    for (std::size_t i = 0; i < call_expr.arguments.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += print_expr(*call_expr.arguments[i]);
    }
    out += ")";
    return out;
  }
  case ExprKind::ArrayLiteral: {
    const auto &array_literal = std::get<ArrayLiteralExpr>(expr.data);
    std::string out = "array<" + std::to_string(array_literal.capacity) + ">[";
    for (std::size_t i = 0; i < array_literal.elements.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += print_expr(*array_literal.elements[i]);
    }
    out += "]";
    return out;
  }
  case ExprKind::Product: {
    const auto &product_expr = std::get<ProductExpr>(expr.data);
    std::string out = "(";
    for (std::size_t i = 0; i < product_expr.components.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += print_expr(*product_expr.components[i]);
    }
    out += ")";
    return out;
  }
  case ExprKind::Len: {
    const auto &len_expr = std::get<LenExpr>(expr.data);
    return "len(" + print_expr(*len_expr.array) + ")";
  }
  case ExprKind::Proj: {
    const auto &proj_expr = std::get<ProjExpr>(expr.data);
    return "proj<" + std::to_string(proj_expr.index) + ">(" + print_expr(*proj_expr.operand) + ")";
  }
  case ExprKind::Index: {
    const auto &index_expr = std::get<IndexExpr>(expr.data);
    return print_operand(*index_expr.array) + "[" + print_expr(*index_expr.index) + "]";
  }
  case ExprKind::Literal: {
    const auto &literal_expr = std::get<LiteralExpr>(expr.data);
    switch (literal_expr.kind) {
    case LiteralKind::Bool:
      return literal_expr.value != 0 ? "true" : "false";
    case LiteralKind::I32Bits:
      return "i32bits(0x" + hex_digits(literal_expr.value, 8) + ")";
    case LiteralKind::I64Bits:
      return "i64bits(0x" + hex_digits(literal_expr.value, 16) + ")";
    case LiteralKind::F64Bits:
      return "f64bits(0x" + hex_digits(literal_expr.value, 16) + ")";
    }
    return "";
  }
  }
  return "";
}

} // namespace

std::string print(const Module &module) {
  std::ostringstream out;
  for (const auto &function : module.functions) {
    out << "fn " << function.name << "(";
    for (std::size_t i = 0; i < function.parameters.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << function.parameters[i].name << ": " << print_type(*function.parameters[i].type);
    }
    out << "): " << print_type(*function.result_type) << " = " << print_expr(*function.body) << ";\n";
  }
  out << "export " << module.export_decl.name << ";\n";
  return out.str();
}

} // namespace boundfin::source::ast
