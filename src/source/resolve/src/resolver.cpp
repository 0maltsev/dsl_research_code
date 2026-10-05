#include "boundfin/source/resolve/resolver.hpp"

#include <unordered_map>
#include <vector>

namespace boundfin::source::resolve {

namespace {

struct ResolveFailure {
  ResolveDiagnostic diagnostic;
};

[[noreturn]] void fail(std::string code, SourceSpan span, std::string message) {
  throw ResolveFailure{ResolveDiagnostic{std::move(code), span, std::move(message)}};
}

struct ScopeEntry {
  std::string name;
  ast::BindingId id;
};

// Returns true and sets `out` to the innermost (lexically shadowing)
// binding named `name`, if any.
bool lookup(const std::vector<ScopeEntry> &scope, const std::string &name, ast::BindingId &out) {
  for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
    if (it->name == name) {
      out = it->id;
      return true;
    }
  }
  return false;
}

class Resolver {
public:
  explicit Resolver(ast::Module &module) : module_(module) {}

  void run() {
    build_function_table();
    for (std::size_t rank = 0; rank < module_.functions.size(); ++rank) {
      resolve_function(module_.functions[rank], rank);
    }
    resolve_export();
  }

private:
  ast::Module &module_;
  std::unordered_map<std::string, std::size_t> functions_; // name -> declaration rank
  ast::BindingId next_binding_id_ = 0;

  ast::BindingId fresh_binding_id() { return next_binding_id_++; }

  void build_function_table() {
    for (std::size_t rank = 0; rank < module_.functions.size(); ++rank) {
      const auto &decl = module_.functions[rank];
      if (functions_.contains(decl.name)) {
        fail("NAM001", decl.name_span, "duplicate function declaration \"" + decl.name + "\"");
      }
      functions_.emplace(decl.name, rank);
    }
  }

  void resolve_function(ast::FunctionDecl &decl, std::size_t rank) {
    std::vector<ScopeEntry> scope;
    for (auto &param : decl.parameters) {
      for (const auto &entry : scope) {
        if (entry.name == param.name) {
          fail("NAM002", param.name_span, "duplicate parameter name \"" + param.name + "\"");
        }
      }
      const ast::BindingId id = fresh_binding_id();
      param.binding = id;
      scope.push_back(ScopeEntry{param.name, id});
    }
    resolve_expr(*decl.body, scope, rank);
  }

  void resolve_expr(ast::Expr &expr, std::vector<ScopeEntry> &scope, std::size_t caller_rank) {
    using namespace boundfin::source::ast;
    switch (expr.kind) {
    case ExprKind::Let: {
      auto &let_expr = std::get<LetExpr>(expr.data);
      resolve_expr(*let_expr.bound, scope, caller_rank);
      const ast::BindingId id = fresh_binding_id();
      let_expr.binding = id;
      scope.push_back(ScopeEntry{let_expr.name, id});
      resolve_expr(*let_expr.body, scope, caller_rank);
      scope.pop_back();
      return;
    }
    case ExprKind::If: {
      auto &if_expr = std::get<IfExpr>(expr.data);
      resolve_expr(*if_expr.condition, scope, caller_rank);
      resolve_expr(*if_expr.then_branch, scope, caller_rank);
      resolve_expr(*if_expr.else_branch, scope, caller_rank);
      return;
    }
    case ExprKind::Fold: {
      auto &fold_expr = std::get<FoldExpr>(expr.data);
      resolve_expr(*fold_expr.count, scope, caller_rank);
      resolve_expr(*fold_expr.initial, scope, caller_rank);
      // grammar.ebnf static constraint 4 / diagnostics-and-status.md NAM002:
      // the accumulator/index pair is one binding list.
      if (fold_expr.accumulator_name == fold_expr.index_name) {
        fail("NAM002", fold_expr.index_name_span,
             "duplicate binder name \"" + fold_expr.index_name + "\" in one fold binding list");
      }
      const ast::BindingId acc_id = fresh_binding_id();
      const ast::BindingId idx_id = fresh_binding_id();
      fold_expr.accumulator_binding = acc_id;
      fold_expr.index_binding = idx_id;
      scope.push_back(ScopeEntry{fold_expr.accumulator_name, acc_id});
      scope.push_back(ScopeEntry{fold_expr.index_name, idx_id});
      resolve_expr(*fold_expr.body, scope, caller_rank);
      scope.pop_back();
      scope.pop_back();
      return;
    }
    case ExprKind::Build: {
      auto &build_expr = std::get<BuildExpr>(expr.data);
      resolve_expr(*build_expr.count, scope, caller_rank);
      const ast::BindingId id = fresh_binding_id();
      build_expr.index_binding = id;
      scope.push_back(ScopeEntry{build_expr.index_name, id});
      resolve_expr(*build_expr.body, scope, caller_rank);
      scope.pop_back();
      return;
    }
    case ExprKind::UnaryPrimitive:
      resolve_expr(*std::get<UnaryPrimitiveExpr>(expr.data).operand, scope, caller_rank);
      return;
    case ExprKind::BinaryPrimitive: {
      auto &binary_expr = std::get<BinaryPrimitiveExpr>(expr.data);
      resolve_expr(*binary_expr.lhs, scope, caller_rank);
      resolve_expr(*binary_expr.rhs, scope, caller_rank);
      return;
    }
    case ExprKind::Var: {
      auto &var_expr = std::get<VarExpr>(expr.data);
      ast::BindingId id = 0;
      if (lookup(scope, var_expr.name, id)) {
        var_expr.resolved_binding = id;
        return;
      }
      if (functions_.contains(var_expr.name)) {
        fail("NAM007", var_expr.name_span,
             "\"" + var_expr.name + "\" names a function; call it with \"(...)\", it cannot be used as a value");
      }
      fail("NAM003", var_expr.name_span, "unbound variable \"" + var_expr.name + "\"");
    }
    case ExprKind::Call: {
      auto &call_expr = std::get<CallExpr>(expr.data);
      const auto function_it = functions_.find(call_expr.callee);
      if (function_it == functions_.end()) {
        ast::BindingId unused_id = 0;
        if (lookup(scope, call_expr.callee, unused_id)) {
          fail("NAM007", call_expr.callee_span,
               "\"" + call_expr.callee + "\" names a variable, not a function; remove \"(...)\"");
        }
        fail("NAM004", call_expr.callee_span, "unknown function \"" + call_expr.callee + "\"");
      }
      const std::size_t callee_rank = function_it->second;
      // paper Sec. 4.1: the body of g may call only f with f declared
      // strictly before g, so the call graph is acyclic; "There is no ...
      // recursion". A function calling itself is the one case this frozen
      // grammar can reach of diagnostics-and-status.md's EFF004
      // ("Recursion or cyclic call graph"); any other violation (a forward
      // reference to a different, later-declared function) is NAM006.
      if (callee_rank == caller_rank) {
        fail("EFF004", call_expr.callee_span, "recursion: \"" + call_expr.callee + "\" calls itself");
      }
      if (callee_rank > caller_rank) {
        fail("NAM006", call_expr.callee_span,
             "call target \"" + call_expr.callee + "\" is not declared strictly before its caller");
      }
      call_expr.resolved_callee_rank = callee_rank;
      for (auto &argument : call_expr.arguments) {
        resolve_expr(*argument, scope, caller_rank);
      }
      return;
    }
    case ExprKind::ArrayLiteral: {
      for (auto &element : std::get<ArrayLiteralExpr>(expr.data).elements) {
        resolve_expr(*element, scope, caller_rank);
      }
      return;
    }
    case ExprKind::Product: {
      for (auto &component : std::get<ProductExpr>(expr.data).components) {
        resolve_expr(*component, scope, caller_rank);
      }
      return;
    }
    case ExprKind::Len:
      resolve_expr(*std::get<LenExpr>(expr.data).array, scope, caller_rank);
      return;
    case ExprKind::Proj:
      resolve_expr(*std::get<ProjExpr>(expr.data).operand, scope, caller_rank);
      return;
    case ExprKind::Index: {
      auto &index_expr = std::get<IndexExpr>(expr.data);
      resolve_expr(*index_expr.array, scope, caller_rank);
      resolve_expr(*index_expr.index, scope, caller_rank);
      return;
    }
    case ExprKind::Literal:
      return;
    }
  }

  void resolve_export() {
    auto &export_decl = module_.export_decl;
    const auto function_it = functions_.find(export_decl.name);
    if (function_it == functions_.end()) {
      fail("NAM005", export_decl.name_span, "export names no declaration: \"" + export_decl.name + "\"");
    }
    export_decl.resolved_target_rank = function_it->second;
  }
};

} // namespace

ResolveResult resolve_module(ast::Module &module) {
  try {
    Resolver resolver(module);
    resolver.run();
    return ResolveResult{true, std::nullopt};
  } catch (const ResolveFailure &failure) {
    return ResolveResult{false, failure.diagnostic};
  }
}

} // namespace boundfin::source::resolve
