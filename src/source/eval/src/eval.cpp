#include "boundfin/source/eval/eval.hpp"

#include <deque>

namespace boundfin::source::eval {

Value make_bool(bool value) {
  Value result;
  result.kind = ValueKind::Bool;
  result.data = BoolWord{value};
  return result;
}

Value make_i32(std::uint32_t bits) {
  Value result;
  result.kind = ValueKind::I32;
  result.data = I32Word{bits};
  return result;
}

Value make_i64(std::uint64_t bits) {
  Value result;
  result.kind = ValueKind::I64;
  result.data = I64Word{bits};
  return result;
}

Value make_f64(std::uint64_t bits) {
  Value result;
  result.kind = ValueKind::F64;
  result.data = F64Word{bits};
  return result;
}

Value make_product(std::vector<Value> components) {
  Value result;
  result.kind = ValueKind::Product;
  result.data = ProductValue{std::move(components)};
  return result;
}

Value make_array(ObjectId id) {
  Value result;
  result.kind = ValueKind::Array;
  result.data = ArrayValue{id};
  return result;
}

namespace {

// Pushes every object identifier directly named by `value` (an ArrayValue
// leaf) or recursively contained in it (a ProductValue's own components)
// onto `frontier`, for the caller's own BFS traversal. A scalar value
// contributes nothing.
void collect_roots(const Value &value, std::deque<ObjectId> &frontier) {
  switch (value.kind) {
  case ValueKind::Bool:
  case ValueKind::I32:
  case ValueKind::I64:
  case ValueKind::F64:
    return;
  case ValueKind::Product:
    for (const auto &component : std::get<ProductValue>(value.data).components) {
      collect_roots(component, frontier);
    }
    return;
  case ValueKind::Array:
    frontier.push_back(std::get<ArrayValue>(value.data).id);
    return;
  }
}

} // namespace

std::unordered_set<ObjectId> reachable_objects(const Store &store, const std::vector<Value> &roots) {
  std::unordered_set<ObjectId> visited;
  std::deque<ObjectId> frontier;
  for (const auto &root : roots) {
    collect_roots(root, frontier);
  }
  while (!frontier.empty()) {
    const auto id = frontier.front();
    frontier.pop_front();
    if (!visited.insert(id).second) {
      continue; // already visited -- defensive cycle guard, not expected to trigger
    }
    const auto it = store.find(id);
    if (it == store.end()) {
      // A root naming an object the store no longer has (already
      // restricted away, or never materialized) is silently skipped --
      // this function only computes a closure over what IS present, it
      // does not validate that every named root actually exists.
      continue;
    }
    switch (it->second.kind) {
    case StoreObjectKind::Sealed:
      for (const auto &element : std::get<SealedObject>(it->second.data).elements) {
        collect_roots(element, frontier);
      }
      break;
    case StoreObjectKind::Prefix:
      for (const auto &element : std::get<PrefixObject>(it->second.data).initialized_elements) {
        collect_roots(element, frontier);
      }
      break;
    }
  }
  return visited;
}

Store restrict_store(const Store &store, const std::vector<Value> &roots) {
  const auto reachable = reachable_objects(store, roots);
  Store restricted;
  for (const auto &id : reachable) {
    const auto it = store.find(id);
    if (it != store.end()) {
      restricted.emplace(id, it->second);
    }
  }
  return restricted;
}

EvalOutcome eval_ok(Value value) { return EvalOutcome{true, std::move(value), std::nullopt}; }

EvalOutcome eval_err(ErrorCode error) { return EvalOutcome{false, std::nullopt, error}; }

StepResult step_ok(EvalOutcome outcome, Store store) {
  return StepResult{true, std::move(outcome), std::move(store), std::nullopt};
}

StepResult step_internal_error(std::string message) {
  return StepResult{false, std::nullopt, std::nullopt, std::move(message)};
}

StepResult eval_var(const ast::VarExpr &var_expr, const Environment &environment, const Store &store,
                     const std::vector<Value> &roots) {
  if (!var_expr.resolved_binding) {
    // Unreachable for a resolver-processed module; mirrors every sibling
    // static-analysis module's identical defensive branch, adapted to
    // this module's own plain-string internal-error convention (no
    // SIZ/NAM/TYP-style diagnostic code -- this is not a source-rejection
    // stage).
    return step_internal_error("internal: variable reference has no resolved binding");
  }
  const auto it = environment.find(*var_expr.resolved_binding);
  if (it == environment.end()) {
    // Structurally unreachable for any program that passed Phase 3.1's
    // resolver and Phase 3.2's typecheck: every resolved variable
    // reference names an in-scope binding, which the caller is expected
    // to have already seeded into `environment` before evaluation
    // reaches this node -- mirrors `shape_of_var`'s identical
    // trust-boundary reasoning in the static analysis modules.
    return step_internal_error("internal: variable reference has no entry in the evaluation environment");
  }
  const auto &value = it->second;
  std::vector<Value> extended_roots = roots;
  extended_roots.push_back(value);
  return step_ok(eval_ok(value), restrict_store(store, extended_roots));
}

StepResult eval_const(const ast::LiteralExpr &literal, const Store &store, const std::vector<Value> &roots) {
  switch (literal.kind) {
  case ast::LiteralKind::Bool:
    return step_ok(eval_ok(make_bool(literal.value != 0)), restrict_store(store, roots));
  case ast::LiteralKind::I32Bits:
    return step_ok(eval_ok(make_i32(static_cast<std::uint32_t>(literal.value))), restrict_store(store, roots));
  case ast::LiteralKind::I64Bits:
    return step_ok(eval_ok(make_i64(literal.value)), restrict_store(store, roots));
  case ast::LiteralKind::F64Bits:
    return step_ok(eval_ok(make_f64(literal.value)), restrict_store(store, roots));
  }
  // Unreachable (exhaustive switch over every LiteralKind); an internal
  // precondition violation, not a declared source error.
  return step_internal_error("internal: unreachable literal kind");
}

} // namespace boundfin::source::eval
