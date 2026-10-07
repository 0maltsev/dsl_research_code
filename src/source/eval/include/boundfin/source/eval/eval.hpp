#pragma once

#include "boundfin/source/ast/ast.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace boundfin::source::eval {

// Phase 4: the independent reference evaluator (main.pdf Sec. 5, "Operational
// Semantics and Source Liveness"; Appendix A.6, "Evaluation, errors, and
// auxiliary relations"; `docs/spec-freeze/cost-trace-semantics.md` Sec. 1-5).
// This module's own scope (`docs/architecture.md`): "Independent rooted
// source semantics and exact dynamic trace" -- must never call BIR/Wasm/
// native code (AGENTS.md's "Independent implementations" rule: the BIR
// evaluator and native oracle, built later, must share no semantic code with
// this one, so correlated defects cannot invalidate G2's differential
// testing).
//
// This slice implements only the core Value/Store/reachability
// infrastructure every evaluation rule needs, plus the two rules with no
// further dependency at all: E-Var and E-Const (main.pdf p.39, Appendix
// A.6). Every other construct needs this same infrastructure plus its own
// additional machinery (sequencing, store mutation, cost/trace recording),
// deferred to later slices -- the same "narrowest testable rule first"
// pattern every earlier phase in this project has already followed (e.g.
// Phase 3.4's own first slice: 3 of Table 6's 10 rules, narrowly scoped to
// the rules with no ABI/array-shape/call-summary dependency).

// --- Values (main.pdf p.13, Sec. 5.1: "An array value is an object
// identifier alpha in an invocation-local store"; the exact grammar
// notation `v::=bool(z1)|i32(z32)|i64(z64)|f64(z64)|(v1,...,vk)|alpha`
// itself is Appendix A.2, p.36, "Values, stores, results, and
// observations" -- Sec. 5.1's own main-text sentence is the informal
// paraphrase of that same grammar, not a separate restatement of it) ----

using ObjectId = std::uint64_t;

enum class ValueKind { Bool, I32, I64, F64, Product, Array };

struct Value;

// One wrapper struct per `ValueKind` alternative (mirroring `certificate::
// Term`'s and `shape::Shape`'s own established per-kind-wrapper convention)
// rather than, e.g., two bare `std::uint64_t` variant alternatives for I64
// and F64 bits, which `std::get<std::uint64_t>` could not disambiguate.
struct BoolWord {
  bool value = false;
};
struct I32Word {
  std::uint32_t bits = 0;
};
struct I64Word {
  std::uint64_t bits = 0;
};
// A raw bit pattern, not a parsed double -- `numeric-semantics.md` governs
// its interpretation (canonicalization after every primitive, NaN handling,
// etc.), not this value type, mirroring `ast::LiteralExpr`'s own identical
// "raw bit pattern" convention for `F64Bits`.
struct F64Word {
  std::uint64_t bits = 0;
};
struct ProductValue {
  std::vector<Value> components;
};
// An array value is JUST the object identifier (main.pdf's own value
// grammar, quoted above) -- the array's own contents (element_type,
// capacity, length, origin, elements) live in the `Store`, keyed by `id`,
// not in the value itself.
struct ArrayValue {
  ObjectId id = 0;
};

struct Value {
  ValueKind kind = ValueKind::Bool;
  std::variant<BoolWord, I32Word, I64Word, F64Word, ProductValue, ArrayValue> data;
};

[[nodiscard]] Value make_bool(bool value);
[[nodiscard]] Value make_i32(std::uint32_t bits);
[[nodiscard]] Value make_i64(std::uint64_t bits);
[[nodiscard]] Value make_f64(std::uint64_t bits);
[[nodiscard]] Value make_product(std::vector<Value> components);
[[nodiscard]] Value make_array(ObjectId id);

// --- Store and objects (main.pdf p.13, Sec. 5.1: "sigma(alpha)=<tau,N,l,
// omega,v0,...,v_{l-1}>, 0<=l<=N" for a sealed object; `cost-trace-
// semantics.md` Sec. 1 additionally names the administrative `prefix(...)`
// object a literal/builder construction uses before it is sealed) --------

enum class Origin { Abi, Local };

// A fully-formed, immutable source array (main.pdf's own `sigma(alpha)`
// shape above): `length` is the array's own runtime length `l` (`0<=l<=
// capacity`), distinct from `capacity` itself (the declared `N`).
struct SealedObject {
  ast::TypePtr element_type;
  std::uint32_t capacity = 0;
  std::uint32_t length = 0;
  Origin origin = Origin::Local;
  std::vector<Value> elements;
};

// An administrative, not-yet-sealed construction object a literal/builder
// uses internally (`cost-trace-semantics.md` Sec. 1: "A prefix exists only
// inside literal/builder administration") -- `target_length` is the
// literal's own `m` or the builder's own certified `n`; `initialized_count`
// is the number of positions written so far (`j` in main.pdf's own E-Lit/
// E-Build family of rules).
struct PrefixObject {
  ast::TypePtr element_type;
  std::uint32_t capacity = 0;
  std::uint32_t target_length = 0;
  std::uint32_t initialized_count = 0;
  Origin origin = Origin::Local;
  std::vector<Value> initialized_elements;
};

enum class StoreObjectKind { Sealed, Prefix };

struct StoreObject {
  StoreObjectKind kind = StoreObjectKind::Sealed;
  std::variant<SealedObject, PrefixObject> data;
};

// An invocation-local store (main.pdf p.13: "stored arrays are immutable
// and identifiers are not source values that can be compared or converted
// to integers"). Keyed by `ObjectId`, not owned/shared pointers -- object
// identity is purely nominal (a fresh counter value, main.pdf's own
// Theorem 5.1 "Determinism modulo rooted-store isomorphism" explicitly
// states results are unique only up to a fresh-identifier bijection, so
// any deterministic, collision-free minting scheme is sound; this module
// does not yet mint identifiers itself -- that is a later slice's job,
// once a rule exists that actually creates an object, e.g. E-Lit/E-Build).
using Store = std::unordered_map<ObjectId, StoreObject>;

// `Reach_sigma(V)` (main.pdf p.13): "the least set containing every array
// identifier occurring in V and every identifier recursively occurring in
// the elements of a reached object." `roots` is a finite value set (in
// this codebase, a `std::vector`, since `Value` is not trivially hashable
// and main.pdf's own "finite root collection" wording does not require a
// literal `std::unordered_set` representation) -- a scalar value in
// `roots` contributes nothing; a product value recurses into its own
// components; an array value contributes its own `id` UNCONDITIONALLY
// (the definition's own "every array identifier occurring in V" clause
// does not require that identifier to already be bound in `store`) and
// recurses FURTHER into that object's own elements only when `store`
// actually has an entry for it (sealed or prefix, whichever `kind` it
// currently records) -- there is simply nothing more to find for an id
// `store` does not have. This means the returned set can contain an id
// absent from `store` (a "dangling" root, structurally impossible for any
// value genuinely produced by evaluating a real expression against a
// consistent store, but this function's own contract does not assume
// that); `restrict_store`, below, naturally drops such an id from its own
// output regardless, since there is nothing in `store` to copy for it.
[[nodiscard]] std::unordered_set<ObjectId> reachable_objects(const Store &store, const std::vector<Value> &roots);

// `sigma restricted-to V` (main.pdf p.13: "sigma1 == sigma2 at roots V,"
// and Sec. 5.2's own repeated "on successful return the store is
// restricted to K union {v}; on error it is restricted to K" -- "this is
// semantic quotienting, not an observable collection event or a runtime-
// garbage-collector claim"). Computes `reachable_objects(store, roots)`
// and keeps only those entries -- objects outside the closure are
// deliberately dropped from the returned `Store`, matching main.pdf's own
// "unreachable objects are deliberately absent from this equivalence."
[[nodiscard]] Store restrict_store(const Store &store, const std::vector<Value> &roots);

// --- Environment (main.pdf p.13, Sec. 5.2: "An environment rho maps
// variables to values") --------------------------------------------------

using Environment = std::unordered_map<ast::BindingId, Value>;

// --- Results and the rooted judgment's own per-step outcome -------------

// `r ::= ok(v) | err(epsilon)` for `epsilon` in `{Bounds,DivZero,
// DivOverflow}` (main.pdf p.13, Sec. 5.2) -- `InvalidABI` is deliberately
// NOT a member of this enum: it is "produced only before entry" (same
// sentence), the pre-evaluation ABI-validity gate already established in
// Phase 3's own well-formedness judgment (main.pdf p.12, Sec. 4.4:
// "Invocation rejection yields InvalidABI before the entry body begins"),
// a wholly separate mechanism from this module's own in-evaluation
// declared-error outcomes.
enum class ErrorCode { Bounds, DivZero, DivOverflow };

struct EvalOutcome {
  bool ok = false;
  std::optional<Value> value;     // present iff ok
  std::optional<ErrorCode> error; // present iff !ok
};

[[nodiscard]] EvalOutcome eval_ok(Value value);
[[nodiscard]] EvalOutcome eval_err(ErrorCode error);

// The full rooted judgment `Sigma;K|-<e,rho,sigma>Downarrow<r,sigma'>|C`
// returns three things: the paper's own `(r,sigma')` pair (`EvalOutcome`
// plus the restricted `Store`), and the exact dynamic cost `C` (main.pdf
// Sec. 6; `cost-trace-semantics.md` Sec. 2-4's own `Trace_K`/event
// vocabulary) -- deliberately NOT yet represented here: `E-Var`/`E-Const`
// both contribute cost `0_1` (Table 10, main.pdf p.44: the "variable,
// constant, propagation" row is the all-zero event weight), so this
// slice's own two rules need no actual event/trace machinery to be
// faithfully implemented; building the full `Trace_K` datatype (Start/
// Step/Stop, every event constructor) is deferred to the slice that first
// needs a NON-zero-cost event (e.g. `E-Prim`), the same "don't build
// machinery before a rule actually needs it" pattern this project's own
// `src/source/size/count`/`shape` modules already followed repeatedly
// (e.g. `CountResult`'s own `nu` component, still unrepresented, `AM-026`).
//
// `StepResult` additionally distinguishes a genuine source-level outcome
// (`EvalOutcome`, present when `ok=true`) from an INTERNAL precondition
// violation (`internal_error`, present when `ok=false`) -- a separate
// level from `EvalOutcome`'s own `ok`/`err(epsilon)` pair, so that
// `EvalOutcome` itself stays a precise, unpolluted mirror of main.pdf's
// own 2-case `r` grammar. A reference-evaluator precondition violation
// (e.g. a variable reference with no entry in `environment`) is
// structurally impossible for any program that has gone through Phase
// 3.1's resolver and Phase 3.2's typecheck -- every resolved variable
// reference names an in-scope binding -- mirroring the same "caller
// precondition violation, not a program property" trust-boundary
// reasoning `shape_of_var`'s own `INT001` case already established for
// the identical situation in the STATIC analysis modules, adapted here
// since this module does not (and should not) use SIZ/NAM/TYP-style
// diagnostic codes at all -- it is not a source-rejection stage.
struct StepResult {
  bool ok = false;                           // false only for an internal precondition violation
  std::optional<EvalOutcome> outcome;        // present iff ok
  std::optional<Store> store;                // sigma', present iff ok
  std::optional<std::string> internal_error; // present iff !ok
};

[[nodiscard]] StepResult step_ok(EvalOutcome outcome, Store store);
[[nodiscard]] StepResult step_internal_error(std::string message);

// E-Var (main.pdf p.39, Appendix A.6): "rho(x)=v" -> "Sigma;K|-<x,rho,
// sigma>Downarrow<ok(v),sigma restricted-to(K union {v})>|0_1." No failure
// premise is shown for this rule at all -- a variable reference is
// unconditionally well-formed for any resolved, typechecked program (an
// unresolved or out-of-scope reference is already rejected by Phase 3.1's
// own `NAM002`-`NAM007` family before evaluation is ever reached), so
// `step_internal_error` here is a defensive trust-boundary branch, not a
// reachable program outcome -- mirroring `shape_of_var`'s identical
// reasoning for the identical situation.
//
// Disclosed, not yet acted on (a spec-auditor review of this slice): this
// function's own parameter list (`environment` included) is NOT uniform
// with `eval_const`'s (below, no `environment` at all, since E-Const's
// own premise never reads `rho`) -- individually correct for each rule
// (faithful to each rule's own exact premises), but it means a future
// unifying recursive dispatcher (this module's own eventual analogue of
// `src/source/size/infer`'s single-entry `infer_shape`) cannot call every
// per-rule function through one uniform signature without either
// per-rule special-casing or a shared `EvalContext`-style struct bundling
// `rho`/`sigma`/`K` together regardless of whether a given rule reads all
// of them. Worth anticipating when that dispatcher slice is scoped, not
// a defect in this one.
[[nodiscard]] StepResult eval_var(const ast::VarExpr &var_expr, const Environment &environment, const Store &store,
                                   const std::vector<Value> &roots);

// E-Const (main.pdf p.39, Appendix A.6): "v=bits(c)" -> "Sigma;K|-<c,rho,
// sigma>Downarrow<ok(v),sigma restricted-to K>|0_1." A literal `c` is
// always scalar (the exact grammar notation `c::=true|false|i32bits(z32)|
// i64bits(z64)|f64bits(z64)` is Appendix A.1, p.36, "Lexical and abstract
// syntax" -- main.pdf's own Sec. 4.1 main text is the informal
// restatement of this same grammar, not a separate source for it), no
// aggregate case -- an array LITERAL `[e0,...]_N` is a distinct, separate
// expression form, not this
// `c` nonterminal), so the restricted root set is exactly `K`, never
// `K union {v}` (a scalar `v` would contribute nothing to that union's own
// reachable closure regardless, but this function follows the rule's own
// literal text, not an equivalent reformulation).
[[nodiscard]] StepResult eval_const(const ast::LiteralExpr &literal, const Store &store,
                                     const std::vector<Value> &roots);

} // namespace boundfin::source::eval
