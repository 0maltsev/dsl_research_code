# Implementation plan

This plan begins only after the Phase 0 readiness blockers are closed. Each phase is independently verifiable. A stop condition blocks the phase rather than authorizing an invented behavior. `spec/paper/` remains immutable throughout.

## Execution tracks

`DEC-014` was approved on 2026-10-04, so phases split into two tracks:

- **Track C (correctness and static evidence):** Phases 1–7 and the correctness-only parts of Phases 8–11. These may run on the recorded development toolchain.
- **Track P (performance evidence):** frozen AOT/native timed artifacts and Phases 12–16. These still require DEC-001–DEC-013 and the experimental machine.
- **Re-gate rule:** before any paper table is frozen, every artifact is rebuilt with the frozen toolchain and the full G1/G2 suites are re-run on the exact artifact hashes.

Track P phases keep the original dependency on Phase 0 GO for every decision they touch.

## Phase 0 — Specification freeze

- Inputs: immutable paper snapshot and implementation hand-off.
- Deliverables: the governance, freeze, architecture, traceability, schemas, decision log, experiment matrix, and readiness report in this repository.
- Tests: JSON/schema parsing, cross-document identifier checks, link/file inventory, immutable hash comparison.
- Acceptance: all semantic/ABI/BIR ambiguities resolved or explicit blocking author decisions; no production placeholders.
- Dependencies: none.
- Scientific risks: accidental paper modification; silently turning a proof obligation into a claim; freezing a material benchmark choice without authority.
- Stop conditions: unresolved implementation semantics or material empirical choice. Current result: complete; **GO for Track C** after `DEC-014` (2026-10-04), **NO-GO for Track P** pending DEC-001–DEC-013.

## Phase 1 — Repository and toolchain foundation

- Inputs: closed Phase 0 decisions, exact compiler/CMake/Ninja/AOT manifests, schema versions.
- Deliverables: minimal C++23 project, dependency policy, reproducible presets, warnings/sanitizers, test runner, manifest/hash utilities, CI smoke checks.
- Tests: configure/build/test on supported host; tool version rejection; deterministic manifest/checksum known answers.
- Acceptance: clean out-of-tree Ninja build; no DSL behavior; exact toolchain identity emitted and schema-valid.
- Dependencies: Phase 0 GO.
- Scientific risks: version drift, hidden host defaults, non-reproducible paths/timestamps.
- Stop conditions: Ninja or required exact tools unavailable; compiler/runtime choice not frozen; manifest nondeterminism.
- Progress: **Complete (2026-10-05)**. **1.1**: minimal C++23/CMake/Ninja project, support library (SHA-256, schema-fragment validation), toolchain-manifest library/CLI with RUN014 drift detection, and the dependency policy, all passing their tests (see `STATUS.md` "Phase 1.1 evidence"). The "version drift, hidden host defaults" risk this phase names materialized directly: Phase 1.1 was built on a host other than the one `DEC-014` named, which raised `DEC-015` ("does `DEC-014` cover any exactly-recorded host, or only the named one?"); the author approved the general reading (option (b)), so this host's evidence counts. **1.2**: `.github/workflows/ci.yml`, the last Phase 1 deliverable, authored, statically verified (`actionlint` clean), run locally end-to-end, and (after pushing commit `3c32e95` to `origin/main`) executed for real on a GitHub-hosted `ubuntu-24.04` runner ([run `37237124426`](https://github.com/0maltsev/dsl_research_code/actions/runs/37237124426), every step succeeded); `spec-auditor` review PASS. Acceptance criteria met on this (and, per `DEC-015`, any exactly-recorded) development host, now including a GitHub-hosted one: clean out-of-tree Ninja build, no DSL behavior, exact toolchain identity emitted and schema-valid.

## Phase 2 — Lexer, parser, and AST

- Inputs: `grammar.ebnf`, diagnostic catalog, language version.
- Deliverables: lossless spans, tokenizer, parser, untyped AST, deterministic serialization.
- Tests: token classes, every production, precedence/associativity, malformed UTF-8/bits/capacities, generated parse/print/parse cases.
- Acceptance: grammar coverage complete; diagnostics stable; no semantic recovery invents a core node.
- Dependencies: Phase 1.
- Scientific risks: surface elaboration changing operation order or primitive identity.
- Stop conditions: grammar conflict, ambiguous parse, or missing diagnostic code.
- Progress: **Complete (2026-10-05)**. **2.1**: the lossless-span tokenizer (`src/source/lex`), covering `LEX001`-`LEX004`/`LEX006`/`LEX007` (`LEX005` deferred to 2.2). 16/16 tests pass; `spec-auditor` review found and fixed 3 process/correctness issues (an unjustified lexical overgeneralization with no grammar support, two spec ambiguities implemented without a logged amendment/author sign-off, and a pending progress update) — see `STATUS.md` "Phase 2.1 evidence" and `SPEC_AMENDMENTS.md` `AM-017` (both parts author-approved as-shipped). **2.2**: the recursive-descent parser (`src/source/parse`) and untyped AST/serialization/round-trip printer (`src/source/ast`), implementing `LEX005` and every `SYNxxx` code (`SYN003` defensively, structurally unreachable through this grammar). 23/23 tests pass; `spec-auditor` review found and fixed 2 issues (the parser briefly owned `SIZ002`, in tension with `docs/architecture.md`'s module boundary, resolved by `SPEC_AMENDMENTS.md` `AM-018` — author-approved deferral to Phase 3's `src/source/size`; a false `SYN002` test-coverage claim, fixed by adding real coverage) — see `STATUS.md` "Phase 2.2 evidence". Acceptance criteria met: grammar coverage complete, diagnostics stable, no semantic recovery invents a core node (confirmed by independent spec-auditor re-derivation of the fold/build field semantics and the elaboration-target design from `cost-trace-semantics.md`/`SPEC_AMENDMENTS.md` AM-001, not just asserted).

## Phase 3 — Static semantics and bounded-size validation

- Inputs: AST, numeric/type/shape rules, certificate fragment, `Q_f` definition.
- Deliverables: resolver, alpha-renamed typed AST, effect audit, declaration ranks, shape/count derivations, finite certificate checker.
- Tests: every typing/size/count rule; negative name/type/effect/call-rank cases; exact/upper/capacity and nested substitution; certificate mutation.
- Acceptance: every accepted node has type/shape/source location; external solver output is accepted only through checked finite derivations.
- Dependencies: Phase 2.
- Scientific risks: trusting modular arithmetic or solver results; losing exactness unsafely.
- Stop conditions: unrepresented source form, unproved count, non-total layout/shape, or call-summary ambiguity.
- Progress: **3.1 complete (2026-10-05)** — the resolver (`src/source/resolve`). 29/29 tests pass; `spec-auditor` found and fixed 1 issue (`AM-019`). **3.2 complete (2026-10-05)** — the typechecker (`src/source/typecheck`): every Table 7 typing rule and Table 5 primitive monomorphization. 37/37 tests pass; two spec ambiguities asked and approved before implementation (`AM-020`/`AM-021`); audit found and fixed a test-coverage gap. **3.3 complete (2026-10-05)** — the certificate checker (`src/source/size/certificate`): Appendix A.4's `Delta|-cert phi` judgment, 10 `CertificateKind` values. 45/45 tests pass; two `spec-auditor` FAILs fixed and re-verified (an unescalated judgment call, `AM-022`; a reachable null-pointer dereference). **3.4 in progress** — the count fragment (`src/source/size/count`): 8 of Table 6's 10 rules plus the `T-Fold`/`T-Build` count-admissibility premise and its live module-wide wiring, built across six `spec-auditor`-reviewed slices (2026-10-05–07; one genuine soundness bug found and corrected via `AM-025`; one genuine wiring-exhaustiveness bug found and fixed in the fifth slice). **Sixth slice (`C-Len-E`/`C-Len-U`: `infer_len`) complete, 2026-10-07**: re-reading Table 6's full row list directly (`main.pdf` p.38) found this project's own prior records had wrongly treated `C-ABI`/`C-Len-E`/`C-Len-U`/`C-Call` as one uniformly-blocked group — only `C-ABI`/`C-Call` are genuinely tied to the still-undesigned `Σ`/`K_f`/`Q_f`-construction machinery; `C-Len-E`/`C-Len-U` read `(λ,u)` directly off an already-known Table 8 shape and are independently implementable. Logged and approved as `AM-034` before implementation; `infer_count` gained a new, optional `shape::ShapeContext` parameter (the first `src/source/size/count` dependency on `src/source/size/shape`), `infer_len` scoped to a `VarExpr` operand only, mirroring `C-Idx`'s own `AM-026` precedent. A `spec-auditor` review returned PASS (no FAIL) — it independently tried and failed to find an alternative reading of `C-ABI`'s trigger that would avoid deferral, and confirmed the new Table-6-reads-Table-8 dependency is a well-founded, non-circular mutual recursion with Table 8's own builder/fold rows; found 2 NOTE (missing test coverage), both addressed. 61/61 test binaries pass (`boundfin_count_len` now 9 cases). Full per-slice detail: `STATUS.md`'s "Completed milestones" table and "Phase 3.4 evidence" sections. **Seventh slice (`C-ABI`: `count_of_abi_param`) complete, 2026-10-07, `AM-035` step 1**: a dedicated research pass re-reading `main.pdf` pp.8-12 and `proof-obligations.md` in full found the whole remaining `C-ABI`/`C-Call`/`Σ`/`K_f`/`Q_f`/substitution territory is organized by declaration-rank induction (a call-free function needs no substitution mechanism at all); logged as `AM-035`, a five-step induction ladder, approved for step 1 only — `count_of_abi_param` together with `src/source/size/shape`'s `shape_of_abi_array_param` (`AM-030`'s own deferred categorization), sharing a deterministic `"abi#"` symbol-minting convention. A `spec-auditor` review returned PASS (no FAIL); 1 NOTE (certificate-omission justification needed the actual `InvalidABI`-gate citation, not just the `C-Idx` analogy) addressed. 63/63 test binaries pass. **Remaining: `C-Call` only, now blocked by `AM-038` (2026-10-07), not an open implementation-sequencing choice** — steps 1-3, 4a, 5, and the live-wiring follow-up of `AM-035`'s ladder are complete (see below). Researching step 4b (the certificate-level `Subst` mechanism `C-Call`'s own "is certified" obligation needs), re-reading `main.pdf` p.37 found Appendix A.4's one sentence on `Subst` ("a substitution node replaces equals in a previously checked formula") describes narrow equality substitution, structurally different from what `C-Call` actually needs (instantiating a callee's own formal-symbol-parameterized certificate with actual terms merely satisfying the same bound, not terms proven equal to the formals) — and, decisively, `spec/paper/proof-obligations.md`'s own `PO-SIZE-SUBST` row discloses, in the paper's own frozen text, that this exact argument is **"missing proof exposition; no counterexample is known."** Logged as `AM-038` and put to the author before any code was written; approved: do not implement a guessed `Subst` mechanism. `C-Call` is recorded as blocked on this paper-disclosed, unclosed proof obligation, to be revisited only if/when it is closed (most naturally by a manuscript-level change in the separate paper repository, a scientific-authorship decision, not an implementation one). **3.5 (result-size shapes, Table 8/App. A.5, `src/source/size/shape`) in progress** — a separate, *total* judgment (`κ::=scalar` or `prod(κ̄)` or `array(λ,u,N;κ̄)`, covering every expression form, unlike Table 6's bounded fragment). Eight of 9 rows complete and `spec-auditor`-reviewed (first through eighth slices, 2026-10-05–07; full per-slice detail in `STATUS.md`'s "Completed milestones" table and "Phase 3.5 evidence" sections): scalar/variable, product/proj, literal (`AM-028`/`AM-029`), conditional, length/index, capacity fallback (`AM-030`), builder (`AM-031`/`AM-032`), fold (`AM-033`) — several slices' first-version designs needed real audit-driven corrections, not just rubber-stamps. **Eighth slice (fold row: `shape_of_fold`) complete, 2026-10-07**: `AM-033` (asked and approved *before* implementation, the first row scoped up front rather than discovered by audit) settled that the row's genuine `κ0,κi+1=Kb(i,κi)` recurrence can be soundly decided by one fixed-point check (`F(κ0)` vs. `κ0`), re-deriving `AM-031`'s own "static judgment, no value-dependence on the index" argument; a structural mismatch is `INT001` (grounded in T-Fold/`TYP010`, mirroring `join_shapes`'s `AM-028`-settled reasoning), a same-structure-but-differing match is `SIZ008` (spent for the first time, exactly where `AM-028` reserved it). A `spec-auditor` review returned PASS (no FAIL) — it specifically stress-tested the crux soundness argument and confirmed, via a concrete period-two product-swap counterexample, that the implementation remains sound (conservatively rejects, never wrongly accepts) even on a sharper case than `AM-033`'s own text anticipated; found 2 PLAUSIBLE (a page-citation misattribution carried forward from `AM-031`, fixed in three places; the "sound but incomplete" disclosure sharpened to name the oscillating sub-case) and 1 NOTE (missing edge-case test coverage, 4 tests added), all addressed. 60/60 test binaries pass (`boundfin_shape_fold` now 16 cases). 8 of 9 rows now have a dedicated function ("let" needs none). No unifying `infer_shape` dispatcher yet — deferred until `call` is also scoped (`K_f`/`Q_f`/`AM-003`/`AM-022`). The full `∆n`/`ν_n` certificate-hypothesis machinery `AM-026` deferred for Table 6 remains deferred; `fold`'s own narrow scope (`AM-033`) did not end up needing it, since the fixed-point check needs no symbolic-iteration certificate at all. **Ninth slice (ABI-array-parameter shape rule: `shape_of_abi_array_param`) complete, 2026-10-07, `AM-035` step 1** — implemented together with Table 6's `C-ABI` above (see that entry for the full `AM-035` ladder/research narrative); `AM-030`'s own already-approved categorization finally implemented. A `spec-auditor` review returned PASS (no FAIL); 1 NOTE addressed. 63/63 test binaries pass (`boundfin_shape_abi_param` 5 cases).

**`AM-035` step 2 complete, 2026-10-07: the unifying `infer_shape` dispatcher**, a new module `src/source/size/infer` (not `src/source/size/shape` itself). Scoping it surfaced a real module-cycle risk (`shape_of_builder` needs Table 6's own count judgment, but `count` already depends on `shape`); logged and approved as `AM-036` before any code was written — a new module depending on both `count`/`shape` hosts the dispatcher, covering 8 of 9 non-`call` rows (every one with a real `ExprKind`); a new diagnostic `SIZ013` (`diagnostics-and-status.md` bumped to `0.1.1`) is reserved for `CallExpr` only. Two `spec-auditor` audit rounds: the first found 1 FAIL (`infer_fold` checked count admissibility *after* computing `initial`'s own shape, reversing E-Fold's left-to-right order — reproduced concretely, fixed) plus 2 PLAUSIBLE (both addressed, one disclosed as deferred: `check_count_admissibility` still lacks a `shape_context` parameter); a second, independent re-audit verified all three fixes via its own standalone mutants and found 1 further FAIL (the `Index` dispatch case had zero test coverage, an operand-swap mutation going uncaught — fixed). 64/64 test binaries pass (`boundfin_infer_shape`, new, 15 cases). 8 of 9 Table 8 rows now reachable from live `ast::Expr` traversal for the first time ("let" needs no new function either way). **`AM-035` step 3 complete, 2026-10-07: `compute_function_summary`**, `AM-003`'s own `Sigma(f)=(signature, K_f, Q_f?, Phi_f, rank)` construction for a call-free function, in `src/source/size/infer`. `k_f=infer_shape(body, shape_context)` (unconditionally required); `q_f=count::infer_count(body, {}, shape_context)` only when the result is `i32` and the derivation succeeds -- a SIZ-coded derivation failure leaves `q_f` absent (`AM-003`: "the call is legal... but prohibited in the count fragment"), an `INT`-coded one propagates as a real failure. `shape_context` seeded per parameter: array-typed via `shape_of_abi_array_param`; every other type via `capshape` directly. Implemented without a fresh `AskUserQuestion` (direct continuation of already-approved precedent); a `spec-auditor` review specifically stress-tested that call and found 1 FAIL (the first version blanket-rejected every product-typed parameter with `SIZ013`, an unescalated, inaccurately-justified scope decision -- `capshape` already gives a sound seeding for scalar/product parameters, fixed) plus 1 PLAUSIBLE (`Q_f` absorption needed an `INT0xx` exception, fixed). A second, independent re-audit verified both fixes and suggested one further test, added. 65/65 test binaries pass (`boundfin_function_summary`, new, 9 cases). **`AM-035` step 4a complete, 2026-10-07, per `AM-037`'s split**: re-reading `main.pdf` pp.10-12 and Table 6/Table 8's own row text while scoping step 4 found "the substitution mechanism" is actually two mechanisms — Table 6's `C-Call` states its own substitution "is certified"; Table 8's "call" row and the main-text `K_f`-substitution sentence do not. Logged as `AM-037` and approved: split step 4 into 4a (this slice -- a pure structural term/shape rewrite, no certificate involvement) and 4b (deferred -- the certificate-level `Subst` mechanism, `AM-022`, needed for `C-Call` specifically). `certificate::substitute_term` recursively substitutes `Symbol` leaves per a caller-supplied map, `std::nullopt` for a null term or an unmapped symbol (plain optional, no diagnostic code, mirroring `evaluate_closed`); `shape::substitute_shape` composes it over a whole `Shape` tree via two separate maps (`exact_substitution`/`upper_substitution`, since the same formal symbol resolves differently in each position) -- a failed exact substitution yields `star` (`ok=true`, main.pdf p.12's own named case), a failed upper substitution is `INT001`. A `spec-auditor` review returned PASS (no FAIL); 1 PLAUSIBLE (the `INT001` upper-substitution citation named `T-Call` when the real dependency is `K_f`'s own free-symbol confinement, currently an `AM-028`/`SIZ008`-deferral side effect, not an independently closed fact) fixed in both modules' doc comments. 67/67 test binaries pass (`boundfin_certificate_substitute`, new, 9 cases; `boundfin_shape_substitute`, new, 11 cases). **`AM-035` step 5 complete, 2026-10-07, re-sequenced per `AM-037`**: `src/source/size/infer` gains `SigmaContext` (rank-keyed `Sigma` lookup) and `infer_call`, implementing Table 8's own "call" row only (not `C-Call`). **Process deviation**: implemented on the generic instruction "continue," without first obtaining the separate confirmation `AM-037`'s own approved text explicitly requires -- a `spec-auditor` re-audit caught this as a FAIL; the author was asked directly afterward via `AskUserQuestion` and retroactively confirmed step 5 as the chosen path (recorded in `AM-037`'s own entry). A first `spec-auditor` review found 1 FAIL (computed every actual argument's own shape unconditionally, including non-load-bearing positions, an unforced scope decision resting on an inaccurate citation -- fixed, restricted to array-typed positions only, matching Table 8's own "call" row premise and this module's own established precedent). A second, independent re-audit of that fix found 2 further issues: the process-confirmation gap above, and one new regression test left vacuous by an empty `SigmaContext` masking the branch it claimed to cover -- both fixed. 68/68 test binaries pass (`boundfin_infer_call`, new, 10 cases). **Live-wiring follow-up complete, 2026-10-07, confirmed via `AskUserQuestion` BEFORE implementation this time**: `infer_shape`/`compute_function_summary` gain optional, defaulted `module`/`sigma` trailing parameters, threaded through the entire recursive dispatcher (`infer_shape_impl` plus all 9 helper functions, 13 call sites total); `infer_shape_impl`'s own `CallExpr` case now delegates to `infer_call` when a caller opts in (still `SIZ013` by default, every pre-existing call site unaffected). New `compute_module_summaries` builds a real whole-module `Σ` by calling `compute_function_summary` for every function in declaration-rank order (vector index confirmed, by direct reading of `ast.hpp`/`resolver.cpp`, to already BE rank), first-error, matching `check_module_count_admissibility`'s own established pattern and main.pdf p.12 Sec. 4.4's own `⊢_M wf` requirement. A `spec-auditor` review returned PASS (no FAIL) -- specifically directed to stress-test whether this slice's own "no further confirmation needed" judgment was correct, and confirmed it was, since none of its internal design choices rose to a genuine spec ambiguity. 2 PLAUSIBLE (missing "bad-first" first-error test direction; missing let-body-position nested-call test) fixed and independently mutation-verified; 1 NOTE (SIZ013's catalog wording slightly stale) deferred, non-blocking. 69/69 test binaries pass (`boundfin_module_summaries`, new, 5 cases). **`AM-035`'s five-step declaration-rank induction ladder is complete through step 5 and the live-wiring follow-up**; step 4b/`C-Call` is now blocked by `AM-038` (2026-10-07, approved: do not implement a guessed `Subst` mechanism), not an open implementation-sequencing choice -- `spec/paper/proof-obligations.md`'s own `PO-SIZE-SUBST` row discloses "missing proof exposition; no counterexample is known" for this exact argument, in the paper's own frozen text. Phase 3.5 (Table 8, below) is accordingly complete; Phase 3.4 (Table 6) remains blocked specifically on `C-Call`, pending a closed `PO-SIZE-SUBST` proof, most naturally via a manuscript-level change in the separate paper repository.

## Phase 4 — Reference evaluator and dynamic cost traces

- Inputs: typed core, numeric semantics, trace datatype, ABI decoder.
- Deliverables: independent rooted evaluator, immutable store, literal/fold/build auxiliaries, exact event/store/root trace, declared outcomes.
- Tests: every evaluation rule and error prefix; fresh-name isomorphism; object death; retained aliases; canonical NaN; deterministic replay.
- Acceptance: exact first-error and trace agreement with hand-worked known answers; evaluator never calls generated code.
- Dependencies: Phase 3 and ABI codec subset.
- Scientific risks: premise-root omission, peak undercount, confusing literal with builder.
- Stop conditions: nondeterminism, trace composition mismatch, or an expression without an evaluator rule.
- Progress: **Started 2026-10-07, confirmed via `AskUserQuestion` before implementation** (the author chose "Move to Phase 4" after `AM-038` recorded Table 6's `C-Call` as blocked on the paper's own disclosed `PO-SIZE-SUBST` gap, not an implementation choice -- Phase 4's own inputs don't need `C-Call` specifically). **First slice complete**: new module `src/source/eval`, implementing the core `Value`/`Store`/`Environment`/reachability infrastructure (main.pdf Sec. 5.1-5.2, pp.13-14; Appendix A.1-A.2, p.36) plus the two evaluation rules with no further dependency, `E-Var`/`E-Const` (Appendix A.6, p.39) -- Table 10 (p.44) confirms both contribute the all-zero cost vector, so no `Trace_K`/event-cost machinery was needed for this slice. `Value` is a tagged union (one wrapper struct per `ValueKind` alternative, mirroring `certificate::Term`'s/`shape::Shape`'s own established convention); `reachable_objects`/`restrict_store` implement `Reach_sigma(V)`/`sigma` restricted-to `V` via BFS; `EvalOutcome` mirrors the paper's own 2-case `r::=ok(v)` or `err(epsilon)` grammar exactly, with a separate outer `StepResult` distinguishing an internal precondition violation (plain string, not an `INT0xx` code -- that catalog is scoped to compiler diagnostics, not this module's own concern). A genuine bug was found and fixed during the slice's own test-writing (not a separate audit): a first-draft test wrongly expected a "dangling root" (an object id absent from the store) to be excluded from `Reach_sigma`; re-reading the definition literally showed the test was wrong, the implementation already correct. A `spec-auditor` review returned PASS (no FAIL), independently re-deriving every cited rule against the PDF directly and specifically confirming the dangling-root correction is itself right, not a second wrong turn; 2 PLAUSIBLE findings fixed (two doc-comment citations pointed at the wrong page for the exact grammar notation; a forward-looking signature-asymmetry disclosure for the future unifying dispatcher). 71/71 test binaries pass (`boundfin_eval_store_reachability`, new, 10 cases; `boundfin_eval_var_const`, new, 10 cases). Full per-slice detail: `STATUS.md`'s "Completed milestones" table and "Phase 4 evidence (first slice)" section. **Remaining**: the generic sequencing auxiliary, every other evaluation rule (`E-Prim`, `E-Let`, `E-If`, `E-Prod`/`E-Proj`/`E-Len`/`E-Index`, `E-Lit`, `E-Fold`, `E-Build`, `E-Call`), the full `Trace_K`/event-cost datatype, object-identifier minting, and a unifying recursive dispatcher -- each its own separately-scoped future slice.

## Phase 5 — Static cost analysis

- Inputs: typed shapes, total `Lambda`, trace semantics, terminal events.
- Deliverables: per-node `E/L/P` transformers, finite sums/recurrences, exact/upper/capacity reports, certificate serialization.
- Tests: terminal rows; sequential/branch/call/fold/build/literal success and error prefixes; alias-overcount; independent small-capacity exhaustive comparisons.
- Acceptance: every source rule maps to one analysis rule; no detected bound counterexample in the declared exhaustive domain; formal status still states open obligations.
- Dependencies: Phases 3–4.
- Scientific risks: call/retained-root peak undercount; treating candidate bounds as proved.
- Stop conditions: any reproducible violation or unavailable retained-root transformer.

## Phase 6 — BIR representation, validator, and evaluator

- Inputs: `bir-spec.md`, capability transitions, diagnostics, ABI types.
- Deliverables: deterministic BIR model/codec, exhaustive validator, fault-classifying small-step evaluator.
- Tests: each instruction/terminator success; ordered multi-fault precedence; dominance/type/control/capability/init/layout/loop/call negatives; deterministic transitions.
- Acceptance: two independent execution paths (direct and serialized round-trip) agree; no generic fallback rule.
- Dependencies: Phases 1 and 3; independent of lowering.
- Scientific risks: validator/evaluator sharing the same defect; capability overlap or incomplete-output escape.
- Stop conditions: unspecified dynamic premise, ambiguous fault, or capability transition.

## Phase 7 — Source-to-BIR lowering

- Inputs: normalized typed core, layout/liveness certificate format, BIR constructors.
- Deliverables: administrative normalization, root dataflow, no-reuse layout first, source maps, one lowering constructor per source form.
- Tests: exact source/BIR outcome and event-order differential cases; literal acyclic chain; fold/build certificates; aggregate calls/errors.
- Acceptance: emitted BIR validates; source and BIR evaluators find no mismatch in the declared suite; PO status remains explicit.
- Dependencies: Phases 4–6.
- Scientific risks: operation duplication/elision, first-error reorder, aggregate result escape.
- Stop conditions: invalid BIR, source-map gap, or mismatch.

## Phase 8 — Scalar BIR-to-Wasm lowering and AOT execution

- Inputs: validated BIR, frozen ABI, target subset, exact AOT feasibility decisions.
- Deliverables: scalar Wasm emitter, structuring certificate, standard validation/feature audit, `.cwasm` production, compiler-free timed loader, stable C/C++ call wrapper.
- Tests: every constructor, forbidden features, standard validator, ABI round trips, guarded divisions, address boundaries, load-only precompiled process audit, disassembly/hash reproducibility.
- Acceptance: scalar differential suite passes for exact artifacts; no compilation/tiering during timed process; performance counters attribute the export boundary.
- Dependencies: Phases 6–7 and closed AOT decisions.
- Scientific risks: hidden compilation/cache, target feature drift, Wasm trap, unstable host boundary.
- Stop conditions: runtime cannot prove load-only AOT, validator failure, target fault, or unstable machine-code capture.

## Phase 9 — Financial kernels and dataset generation

- Inputs: frozen K1/K2/K3 definitions, dataset encoding, PRNG/hash decisions.
- Deliverables: source kernels, deterministic synthetic generators, domain checks, immutable dataset manifests and oracle hashes.
- Tests: K1 known answers and bounds; K2 multiset/order strata; K3 outcome/tier/multiset/pattern invariants; regeneration hash equality.
- Acceptance: all planned cells have stable identity and valid domains; no benchmark execution yet.
- Dependencies: Phases 3–5 and closed randomization decisions.
- Scientific risks: generator changing the intended factor or floating operation tree.
- Stop conditions: domain/hash mismatch, unexpressible kernel, or changed workload factor.

## Phase 10 — Native C++ semantic baseline

- Inputs: typed kernels, numeric/ABI contract, exact native compiler flags.
- Deliverables: independent C++23 oracle/baseline, MS and PO configurations, UB audit, disassembly reports.
- Tests: bit-class primitives; checked division; modular arithmetic; NaN boundary handling; ABI differential cases; sanitizer/UB checks.
- Acceptance: matches the source evaluator under the stated observation relation; MS contains no vector instructions.
- Dependencies: Phases 8–9 and frozen compiler/ISA.
- Scientific risks: signed-overflow UB, contraction/reassociation, baseline derived from compiler internals.
- Stop conditions: mismatch, sanitizer finding, or flag/disassembly drift.

## Phase 11 — Differential correctness gates

- Inputs: reference, BIR, scalar Wasm/AOT, native artifacts, generators, certificate reports.
- Deliverables: G1/G2 runner, conformance/property/metamorphic/differential corpus, shrink/retention, gate report.
- Tests: all traceability rows and exceptional classes; malformed artifacts; exact configuration and checksum enforcement.
- Acceptance: no counterexample/mismatch/fault detected for every artifact proposed for timing; coverage explicit; open proofs not relabelled.
- Dependencies: Phases 4–10.
- Scientific risks: correlated oracle defects or test domain too weak.
- Stop conditions: any mismatch, target fault, unresolved oracle, missing mandatory rule coverage, or artifact hash drift.

## Phase 12 — Benchmark harness and metadata collection

- Inputs: gated artifacts, experiment manifest, machine/timing/cache/frequency decisions, sample schema.
- Deliverables: pinned paired runner, D0–D5 diagnostics, warm-up/calibration, raw append-only writer, counters, invalidity ledger.
- Tests: synthetic timer/counter known answers; failure injection; order counterbalancing; checksum consumption; schema and append-only checks.
- Acceptance: smoke runs reproduce schedules and metadata; development measurements excluded from evidence.
- Dependencies: Phase 11 and all machine decisions.
- Scientific risks: asymmetric ABI work, migration, eliminated result use, implicit exclusions.
- Stop conditions: control violation, magnitude-based exclusion, missing metadata, or counter attribution failure.

## Phase 13 — SIMD backend ablation

- Inputs: validated scalar BIR, frozen K1/K2 SIMD variants and ISA policy.
- Deliverables: explicit Wasm/native SIMD artifacts, variant oracles, lane/mask/remainder/reduction records and disassembly.
- Tests: lane and remainder cases, special floats, scalar-versus-variant numerical report, intended-vector audit.
- Acceptance: K1/K2 variant oracle passes and intended transformation is present; kept separate from MS/PO. K3 absent.
- Dependencies: Phases 8–12.
- Scientific risks: changing estimand or numerical order, accidental K3 treatment.
- Stop conditions: mismatch, absent/unintended vectorization, or configuration pooling.

## Phase 14 — Pilot study

- Inputs: claims-excluded pilot manifest, all gated artifacts, frozen randomization and analysis code.
- Deliverables: five-session pilot, block-length diagnostics, timer calibration, precision simulation, proposed confirmatory replication.
- Tests: pilot tag exclusion; deterministic extension to 16,384; 64-block rules; 10,000 fixed-seed simulation reproducibility.
- Acceptance: every confirmatory family has a valid block rule and feasible `(S,P)` or is explicitly reduced/unresolved before unblinding.
- Dependencies: Phases 12–13.
- Scientific risks: pilot leakage into claims or post-outcome tuning.
- Stop conditions: no valid block length, precision grid failure, machine instability, or missing p99.9 support plan.

## Phase 15 — Experimental freeze

- Inputs: pilot outputs and all prior decisions/artifacts.
- Deliverables: signed/checksummed preregistration manifest, exact schedule, artifacts, seeds, sample counts, invalidity rules, analysis environment.
- Tests: full schema/reference closure, artifact hash verification, schedule regeneration, one small end-to-end dry run excluded from claims.
- Acceptance: immutable, self-contained confirmatory package with no material `AUTHOR DECISION REQUIRED` item.
- Dependencies: Phase 14.
- Scientific risks: hidden degree of freedom or incomplete lineage.
- Stop conditions: unresolved decision, hash drift, or inability to regenerate the plan.

## Phase 16 — Confirmatory execution and analysis

- Inputs: frozen package only.
- Deliverables: append-only raw evidence, validity ledger, deterministic derived tables/plots, formal-status/gate/performance report.
- Tests: session calibrations, raw checksums, schema validation, independent analysis regeneration, deviation audit.
- Acceptance: report every planned family as classified or unresolved; p99.9 only where eligibility holds; archive raw and derived lineage.
- Dependencies: Phase 15 and machine access.
- Scientific risks: deviations, attrition, dependence, weak tail support, overgeneralization.
- Stop conditions: correctness-gate regression, material configuration drift, fewer than permitted sessions, corrupted raw data, or unpreregistered analysis change.

