# Specification amendments and clarifications

The paper snapshot is immutable. This ledger records every implementation resolution that is not fully determined by it. “Propagate” means the change should be applied to the separate paper repository before a results-bearing manuscript or preregistration is frozen.

## AM-001 — Concrete source grammar and operator surface

- Source location: `main.pdf`, Appendix A.1 and Section 4.1; `EXPERIMENT_IMPLEMENTATION_SPEC.md` Section 2.1.
- Ambiguity: only abstract mathematical syntax is provided; declaration terminators, concrete bounded syntax, product syntax, typed bit tokens, and operator precedence are not mechanically parsable. Appendix A also says conveniences outside its lexical categories have no core meaning.
- Adopted resolution: `grammar.ebnf` defines a complete UTF-8 concrete syntax. Fixed infix/unary operators are one-for-one pre-core elaborations to primitive calls; postfix indexing is left associative; equality and comparison operators are non-associative. Elaboration occurs before semantic analysis and never optimizes or reorders.
- Scientific effect: no new primitive or event; source text becomes reproducibly parseable. Parser/elaboration conformance becomes part of PO-NORM evidence.
- Propagate to paper repository: **Yes**; add a concrete-syntax appendix or explicitly cite the artifact grammar.

## AM-002 — Product arity and projection numbering

- Source location: `main.pdf` Sections 4.1, A.1, and Table 7.
- Ambiguity: finite product arity has no lower bound, and concrete projection numbering is not fixed.
- Adopted resolution: product types and values have arity at least two; grouping parentheses are not products. `proj<j>(e)` uses one-based `j` in `[1,k]`, matching the paper's typing rule. Nullary functions remain permitted.
- Scientific effect: removes unit/unary-product corner cases without changing any planned kernel.
- Propagate to paper repository: **Yes**.

## AM-003 — Definition and scope of `Q_f`

- Source location: `main.pdf`, Table 6 rule C-Call versus Section 4.2 T-Call and Section 4.3 `K_f`; `Q_f` is otherwise undefined.
- Ambiguity: the count-call rule refers to `Q_f`, but the function environment stores only signature, result-size transformer `K_f`, and cost transformer `Phi_f`.
- Adopted resolution: a checked function environment entry is `Sigma(f)=(signature, K_f, Q_f?, Phi_f, rank)`. `Q_f` is present only when the declared result is `i32` and the body has an accepted count-refinement derivation. It is derived under exactly the same formal parameters, formal array-length symbols, constraints, and declaration rank as `K_f`; it names the returned word and stores `(lambda_f,u_f;nu_f)`. Call substitution for `Q_f`, `K_f`, and `Phi_f` is simultaneous, capture avoiding, and uses the same actual shape/footprint vector. If `Q_f` is absent, the call is legal as an ordinary expression but prohibited in the count fragment.
- Scientific effect: preserves intended compositional count calls without trusting arbitrary `i32` functions. Soundness relies on TH-CERT plus the existing declaration-rank and PO-SIZE-SUBST/PO-COST-CALL arguments; it adds no new theorem claim.
- Propagate to paper repository: **Yes, required before proof work**.

## AM-004 — Dynamic trace datatype and exact `peak_K`

- Source location: `main.pdf` Sections 5 and 6.1 and Appendix A.7.
- Ambiguity: the prose defines trace-aware composition and a maximum over semantic configurations but not a mathematical trace datatype, exact boundary samples, or a total composition operator.
- Adopted resolution: `cost-trace-semantics.md` defines traces as an initial snapshot followed by typed event/snapshot steps and a terminal outcome, compatibility-checked concatenation, active roots at every boundary, activation markers, and exact `peak_K` over all snapshots. Sequential constructs, selected branches, explicit calls, partial aggregate states, object death, and failures have explicit trace clauses.
- Scientific effect: eliminates peak ambiguity and prevents premise-peak undercount. It strengthens the object to which PO-COST-SOUND applies but does not claim the static equations are sound.
- Propagate to paper repository: **Yes, required before proving PO-COST-ERR/CALL/SOUND**.

## AM-005 — Total retained-footprint environment `Lambda`

- Source location: `main.pdf` Sections 6.2–6.3 and Appendix A.7.
- Ambiguity: `Lambda` is described as mapping aggregate variables to footprint expressions, but scalar entries, ABI parameters, local products/arrays, aliases, call formals, and results are not a total definition.
- Adopted resolution: `cost-trace-semantics.md` defines `Lambda` on every typed variable. Scalars map to zero. Export ABI aggregates map to zero local bytes. Earlier-function aggregate formals receive footprint variables instantiated from actual result-footprint bounds. Let/fold/body binders receive the bound expression/accumulator transformer. Products sum component candidates, arrays include their capacity region plus upper-bounded logical element graphs, and duplicate references are conservatively counted. Every expression form has total `L` and `P` equations or is rejected when its retained-root transformer is unavailable.
- Scientific effect: makes analyzer behavior deterministic and conservatively alias safe; exactness may decrease through overcount. The call/alias soundness obligations remain open.
- Propagate to paper repository: **Yes**.

## AM-006 — Complete scalar and aggregate ABI

- Source location: `main.pdf` Sections 7.5, A.7, and A.8; the paper fixes only broad header, width, alignment, and export shape.
- Ambiguity: Boolean encoding, `A(tau)`, product offsets/padding, nested aggregates, root records, output ownership, exact export names, aliasing, padding, and host/module validation are incomplete.
- Adopted resolution: `abi-layout.md` freezes widths/alignments, canonical product flattening, array objects and capacity trees, absolute little-endian offsets, input/output record headers, padding, nested arrays/products, disjointness, caller-owned deep-copy results, export names/signature, status, trap policy, and host obligations.
- Scientific effect: fixes ABI work included in RQ3/RQ5 and makes layout/address evidence reproducible. It can affect measured overhead and must be identical across execution forms.
- Propagate to paper repository: **Yes, required before performance preregistration**.

## AM-007 — Boolean and padding validity

- Source location: `main.pdf` Sections 3.2, 7.5, and A.8.
- Ambiguity: the width of Boolean is given indirectly, but valid ABI words and padding contents are not fixed.
- Adopted resolution: Boolean is a 32-bit little-endian word and only `0` and `1` are valid. Every input reserved field, alignment byte, unused root byte, unused array-capacity byte, and ignored preallocated nested object byte is zero. The output region is zeroed before invocation and all output padding remains zero.
- Scientific effect: removes multiple encodings and uninitialized-byte/hash leakage; adds symmetric ABI validation/zeroing work.
- Propagate to paper repository: **Yes**.

## AM-008 — Floating output and NaN observation

- Source location: `main.pdf` Sections 3.2, 7.6, and 8.1.
- Ambiguity: source primitives canonicalize NaNs while scalar Wasm observations quotient them; it is not explicit whether output serialization itself canonicalizes.
- Adopted resolution: the reference evaluator and C++ oracle canonicalize after every floating primitive. Scalar Wasm may serialize any Wasm-permitted NaN word. Output serialization does not add a canonicalization step; correctness applies the NaN quotient. Non-NaNs, including signed zero, are exact bits.
- Scientific effect: preserves the paper's target observation relation and avoids inserting unmodelled target work. Raw output hashes involving NaNs must also record the normalized observation hash.
- Propagate to paper repository: **Recommended clarification**.

## AM-009 — Canonical BIR capability vocabulary

- Source location: `main.pdf` Section 7.2 uses long names while Appendix B.1 uses `inR`, `fresh`, `imm`, `scratch`, `outW`; `EXPERIMENT_IMPLEMENTATION_SPEC.md` Section 3.2 names four kinds.
- Ambiguity: vocabulary and whether sealed immutable storage is a fifth capability kind conflict.
- Adopted resolution: the only kinds are `inputRead`, `freshInit`, `scratchInit`, and `outputWrite`. Builder prefix/sealed state and initialized coverage are state fields. `seal` changes `freshInit` from building to sealed-read state without creating an `imm` kind. `reserve` consumes a suitable scratch/output subrange and creates `freshInit`.
- Scientific effect: no semantic change intended; the complete transition system is now testable and serialized records use one vocabulary.
- Propagate to paper repository: **Yes**.

## AM-010 — Executable BIR contract

- Source location: `main.pdf`, Appendix B and Sections 7.2–7.4.
- Ambiguity: the mathematical appendix omits a concrete serialization, several per-instruction state updates, output-coverage details, and interpreter completeness obligations.
- Adopted resolution: `bir-spec.md` freezes a versioned textual abstract syntax, SSA/block/function/module well-formedness, capability-linear state, memory operations, call/return frames, small-step transitions, terminal states, ordered faults, and interpreter obligations. No catch-all transition is permitted.
- Scientific effect: two implementations can be compared step for step; proof obligations remain open.
- Propagate to paper repository: **Cite artifact specification; copy material semantic fixes if proof depends on them**.

## AM-011 — Stable diagnostics and runtime identities

- Source location: `main.pdf` Sections 4.4 and 8.1; `EXPERIMENT_IMPLEMENTATION_SPEC.md` Sections 2.6 and 3.5.
- Ambiguity: categories are named but stable machine-readable identifiers are not assigned.
- Adopted resolution: `diagnostics-and-status.md` assigns versioned codes for lexical, syntax, name, type, size, effect, cost, BIR, ABI, Wasm, internal, and runtime failures, plus fixed source status integers and BIR fault names.
- Scientific effect: negative conformance and invalid-run accounting become comparable; no language behavior changes.
- Propagate to paper repository: **No**, provided the artifact document is cited; status integers should be propagated to the implementation hand-off.

## AM-012 — Generated-record schemas

- Source location: `EXPERIMENT_IMPLEMENTATION_SPEC.md` Sections 1.3, 3.1, 5.4, 6.6, and 8.
- Ambiguity: required record fields are listed informally without types, nullability, enum domains, or versioned schemas.
- Adopted resolution: the four Draft 2020-12 schemas in `schemas/` are normative at schema version `1.0.0`. Every record contains explicit configuration/toolchain/dataset/seed/checksum/failure identity; inapplicable values are required and `null`, never silently absent.
- Scientific effect: makes evidence validation and missingness explicit; no result is created by schema validation.
- Propagate to paper repository: **Cite the artifact schemas**.

## AM-013 — Fixed Wasm exports and source status mapping

- Source location: `main.pdf` Section 7.5 says one contract export and memory only if required; names and integers are absent.
- Ambiguity: host integration and aggregate I/O require stable exports, but neither names nor codes are fixed.
- Adopted resolution: export non-growing memory as `memory` and the sole function as `boundfin_entry(i32,i32)->i32`. Status values are success `0`, Bounds `1`, DivZero `2`, DivOverflow `3`, InvalidABI `4`; target/runtime failures are never encoded as these statuses.
- Scientific effect: fixes the native-call boundary and status work included in measurements.
- Propagate to paper repository: **Yes, required before preregistration**.

## AM-014 — Static array region versus nested aggregate capacity tree

- Source location: `main.pdf` Appendix A.7 defines `region(tau,N)` but does not define a complete nested ABI graph layout.
- Ambiguity: arrays containing arrays/products require offsets and bounded descendant storage; `region` alone covers only the immediate object.
- Adopted resolution: `region(tau,N)` remains the immediate source-object charge used by the source cost model. `TreeBytes(tau)` in `abi-layout.md` is a separate target ABI capacity-tree extent that recursively reserves descendant array objects in deterministic preorder. Source `h`/`p`, ABI input/output bytes, BIR frames, and total linear memory remain separate metrics.
- Scientific effect: prevents target ABI capacity from being mislabeled as source scratch or cumulative reservation.
- Propagate to paper repository: **Yes, as an ABI clarification**.

## AM-015 — BIR primitive error edge and call return shape

- Source location: `main.pdf` Appendix B.1 syntax and Table 12.
- Ambiguity: the shorthand primitive error label and call continuations do not explicitly state that every declared arithmetic error is distinct or how void/scalar/product/aggregate results bind.
- Adopted resolution: each error-capable primitive carries a total map from its feasible declared source errors to labels; other primitives carry an empty map. Direct calls have one success continuation whose parameters exactly represent the declared result and one label per declared error. Nullary functions are allowed; source functions always return one typed value.
- Scientific effect: preserves first-error identity and prevents generic error merging.
- Propagate to paper repository: **Recommended**.

## AM-016 — Experiment planning estimates versus frozen sample sizes

- Source location: `main.pdf` Sections 9.6–9.7 and `EXPERIMENT_IMPLEMENTATION_SPEC.md` Section 6.
- Ambiguity: cells are fixed but final replication, run count, invocation count, storage, and p99.9 family depend on an unrun pilot.
- Adopted resolution: `experiment-matrix.md` gives formulas and explicitly labelled capacity scenarios, not chosen sample sizes or results. Final counts and p99.9 anchors remain author decisions selected under the paper's pilot protocol.
- Scientific effect: supports resource planning without data-dependent or fabricated design choices.
- Propagate to paper repository: **No change needed unless final choices are later frozen**.

## AM-017 — Lexer-level decimal-numeral range and unsupported-construct detection shapes

- Source location: `grammar.ebnf` lexical constraint 7 and the `decimal`/`positive-decimal`/`capacity` productions; `SPEC_FREEZE.md` "Frozen language boundary"; `diagnostics-and-status.md` `LEX006`/`LEX007`.
- Ambiguity, part (a) — numeral range bound: `SPEC_FREEZE.md`'s "Frozen language boundary" states the `[0, 2^31-1]` bound only for *capacities* ("Capacities are decimal natural literals in `[0, 2^31-1]`"), and `grammar.ebnf` lexical constraint 7 ties its leading-zero/overflow rule to "Capacity evaluation" by name. But `grammar.ebnf` also defines `positive-decimal` (used only for the projection index `proj<j>(e)`) as a distinct nonterminal that lexically reduces to the exact same digit-run shape as `capacity`/`decimal`, and a standalone tokenizer cannot tell from the digits alone which of the two a given numeral will turn out to be used as — that distinction is made only once a parser places the token inside a `projection-expression` versus an `array-type`/`fold-expression`/`build-expression`/`array-literal`. The grammar and `SPEC_FREEZE.md` do not say whether an over-`2^31-1` or leading-zero projection-index numeral is a lexical error (`LEX006`) or must be accepted lexically and rejected later once its syntactic role is known (plausibly under `TYP007`, "Projection index is outside one-based product arity," or a new code).
- Ambiguity, part (b) — `LEX007` trigger shapes: `diagnostics-and-status.md` defines `LEX007` only as "Source comment or decimal floating literal is unsupported in language 0.1.0"; no document specifies what comment syntax or floating-literal shape should be *recognized* in order to raise this code instead of a generic `LEX002`/`LEX003`.
- Implemented in `src/source/lex` (Phase 2.1, 2026-10-05), pending this entry's approval:
  - (a) The lexer applies the same `[0, 2^31-1]` bound to every bare decimal numeral it tokenizes, regardless of eventual syntactic role, and reports `LEX006` for any violation. This is the narrowest resolution available to a context-free tokenizer (there is no other stated bound anywhere a decimal numeral appears), but it is an extension beyond what `SPEC_FREEZE.md`'s sentence literally names.
  - (b) The lexer treats `//` and `/*` (the standard C-family forms, matching this language's overall syntax) as comment attempts, and a digit-run immediately adjacent to `.` (either `digits.` or `.digits`, with or without digits on the far side) as a floating-literal attempt, both raising `LEX007`; nothing else does.
- Scientific effect: both are implementation-only lexical-diagnostics-and-recovery details. Neither changes which *programs* are ultimately accepted or rejected, what core node any accepted program elaborates to, any cost/trace event, or any ABI/BIR/Wasm behavior — they only change which specific diagnostic code (and, for (a), lexer-versus-parser stage) a specific class of rejected input receives. No kernel (K1/K2/K3) or dataset depends on either choice.
- Propagate to paper repository: **No** (concrete-syntax-conformance detail below the level the paper specifies; same category as `AM-001`'s grammar but narrower).
- Status: **APPROVED (2026-10-05)**, both parts as shipped. A `spec-auditor` review of the Phase 2.1 milestone found this behavior had been implemented and locked into shipped tests without first being logged here or put to the author, per `CLAUDE.md` §5's stop-and-ask protocol; this entry and the accompanying `AskUserQuestion` are the correction. Author approval given in a Claude Code session on 2026-10-05, in reply to the recommendation for each part above: (a) "Lexer applies the bound to every numeral" — the lexer continues to apply `[0, 2^31-1]` to every bare decimal numeral regardless of eventual syntactic role; (b) "Yes, keep `//` and `/*` plus digit-adjacent-dot" — the `LEX007` trigger shapes are unchanged. No code or test changes were required by this approval.

## AM-018 — Which module checks an array literal's length against its declared capacity

- Source location: `grammar.ebnf` "Static concrete-syntax constraints" item 6 ("An array literal with `m` elements requires `m <= N`"); `diagnostics-and-status.md` `SIZ002` ("Array literal length exceeds capacity"), catalogued under "Size/count diagnostics," a separate table from `LEX`/`SYN`; `docs/architecture.md`'s module-ownership table, which names `src/source/size` (not `src/source/parse`) as owning "Shapes, count refinements, finite certificate checking" and states `src/source/parse`'s row "Must not own: Name, type, size, or cost decisions" with no stated exception.
- Ambiguity: `grammar.ebnf`'s own "static concrete-syntax constraints" list mixes items that are clearly parser-level (item 2, product/array arity at least two, which `src/source/parse` already enforces as `SYN003`) with at least one item that is clearly *not* parser-level by the grammar's own design (item 4, duplicate declaration/parameter names, which is explicitly a name-resolution concern for `src/source/resolve`, Phase 3). The list's membership therefore does not, by itself, settle which module should enforce item 6 (the array-literal-length check): it could be parser-level like item 2, or deferred like item 4. Separately, the one precedent already set this phase for a code being raised outside its diagnostic-prefix's named module (`LEX005`, raised by the parser because the lexer lacks the grammatical-position context) supports a reading where diagnostic-code *prefixes* indicate semantic category rather than a strict module-ownership rule — but that precedent was for a case genuinely requiring the consuming module's extra context, whereas an array literal's element count and declared capacity are *both* fully known to the parser with no type information needed, so the "needs more context" justification does not straightforwardly apply here the way it did for `LEX005`.
- Implemented in `src/source/parse` (Phase 2.2, 2026-10-05), pending this entry's approval: `parse_array_literal()` compares the parsed element count against the declared capacity immediately and raises `SIZ002` itself if `m > N`, rather than accepting any length into the AST and leaving the check to a later phase.
- Scientific effect: none on any program's ultimately accepted/rejected status or the AST an accepted program elaborates to — every reading rejects exactly the same programs with the same diagnostic code. The only difference is which module/phase performs the check and therefore which stage's traceability/test suite is responsible for it, and whether an untyped AST may transiently carry an over-length array literal between Phase 2 and Phase 3.
- Propagate to paper repository: **No** (implementation module-boundary detail below the level the paper specifies).
- Status: **APPROVED (2026-10-05)**, option (b). A `spec-auditor` review of the Phase 2.2 milestone found this check had been implemented in `src/source/parse` without first being logged here or put to the author, in apparent tension with `docs/architecture.md`'s unqualified "must not own... size" line for that module — the same failure class `AM-017` corrected once already this phase. Author approval given in a Claude Code session on 2026-10-05, choosing "Defer to Phase 3's size module (Recommended)": `src/source/parse` no longer raises `SIZ002`; an array literal's length is not checked against its declared capacity until `src/source/size` (Phase 3) exists. `src/source/parse/src/parser.cpp`'s `parse_array_literal()` and `tests/source/parse/test_parse_arrays_products_proj_index_len.cpp` were updated accordingly (the array-literal-exceeds-capacity negative test became a positive test confirming the parser accepts the over-length literal's AST shape unchecked).

## AM-019 — Which code a direct self-recursive call raises, and which module raises it

- Source location: `main.pdf` §4.1 (p.10: "The body of `g` may call only `f` with `f ≺_M g`; hence the call graph is acyclic. There is no ... recursion...") and §4.4 (p.12, one rejection bullet: "mutation, address observation, import, host access, general allocation, recursion, or a call not satisfying `f ≺_M g`"); `diagnostics-and-status.md` `NAM006` ("Call target is not strictly earlier than caller") and `EFF004` ("Recursion or cyclic call graph"), the latter catalogued under "Effect/termination diagnostics"; `docs/architecture.md`'s module table, which assigns `src/source/resolve` only "Symbols, alpha-renaming, declaration ranks" and assigns "Exhaustive typing and effect rejection" to `src/source/typecheck` (Phase 3.2, not yet implemented).
- Ambiguity: the paper's one formal premise for every call is `f ≺_M g` (a single strict total order by declaration rank); self-recursion (`f` calling `f`) is simply the case `f = g`, which already falls inside `NAM006`'s plain-English scope ("not strictly earlier" includes "equal"). Nothing in the paper or `diagnostics-and-status.md` states that self-recursion specifically must be distinguished from any other order violation by diagnostic code, and `EFF004`'s placement under "Effect/termination" (the category `docs/architecture.md` ties to `typecheck`'s "effect rejection," not `resolve`'s "declaration ranks") is at least as strong a textual signal that `EFF004` belongs to a *different* module than the equally plausible reading that `resolve` is best-placed to give self-calls a clearer, dedicated diagnostic. Unlike `AM-017`/`AM-018`, the two readings are not merely "same programs, different code": `resolve` necessarily runs before `typecheck` and already rejects every `f ≺_M g` violation (self-calls included) via its own check, so if `resolve` does *not* special-case self-calls, `typecheck` would never receive an order-violating module to apply `EFF004` to at all -- making `EFF004` structurally unreachable in the implemented pipeline (the same kind of honestly-disclosed dead-code situation already accepted for `SYN003`), not merely differently-labeled.
- Implemented in `src/source/resolve` (Phase 3.1, 2026-10-05), pending this entry's approval: a call whose target has the *same* declaration rank as the caller (i.e. is the caller itself) raises `EFF004`; any other call whose target does not have a strictly smaller rank raises `NAM006`.
- Scientific effect: none on which programs are ultimately accepted — both readings reject exactly the same set of programs (every `f ⊀_M g` call, self-calls included). The difference is which diagnostic code a self-recursive call receives, which module's test suite is responsible for exercising it, and whether `EFF004` is ever reachable at all given `resolve` runs first.
- Propagate to paper repository: **No** (diagnostic-code/module-boundary detail below the level the paper specifies).
- Status: **APPROVED (2026-10-05)**, as shipped. A `spec-auditor` review of the Phase 3.1 milestone found this split had been implemented and asserted in `STATUS.md`/`docs/traceability.md` without first being logged here or put to the author — the same failure class `AM-017` and `AM-018` already corrected twice this phase. Author approval given in a Claude Code session on 2026-10-05, choosing "Resolve raises EFF004 for self-calls (Recommended)": `src/source/resolve` continues to raise `EFF004` for a direct self-call and `NAM006` for any other call-ordering violation. No code change was required by this approval.
