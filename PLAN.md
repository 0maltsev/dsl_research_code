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
- Progress: **3.1 complete (2026-10-05)** — the resolver (`src/source/resolve`). 29/29 tests pass; `spec-auditor` found and fixed 1 issue (`AM-019`). **3.2 complete (2026-10-05)** — the typechecker (`src/source/typecheck`): every Table 7 typing rule and Table 5 primitive monomorphization. 37/37 tests pass; two spec ambiguities asked and approved before implementation (`AM-020`/`AM-021`); audit found and fixed a test-coverage gap. **3.3 complete (2026-10-05)** — the certificate checker (`src/source/size/certificate`): Appendix A.4's `Delta|-cert phi` judgment, 10 `CertificateKind` values. 45/45 tests pass; two `spec-auditor` FAILs fixed and re-verified (an unescalated judgment call, `AM-022`; a reachable null-pointer dereference). **3.4 in progress** — the count fragment (`src/source/size/count`), built across four slices, each `spec-auditor`-reviewed: first slice (`C-Const`/`C-If-E`/`C-If-U`, 2026-10-05, 1 FAIL + 1 PLAUSIBLE fixed); second slice (`C-Add`/`C-Sub`, 2026-10-05) — `AM-023`/`AM-024` approved before implementation, but audit then found `AM-023`'s `C-Sub` reading genuinely **unsound** (silently accepted a deterministically-underflowing program), corrected via `AM-025` and confirmed by independent re-audit; third slice (`C-Idx`, 2026-10-05, `AM-026`, asked before implementation, audit confirmed sound; 1 FAIL + 1 PLAUSIBLE fixed); fourth slice (`check_count_admissibility`, 2026-10-06) — the `T-Fold`/`T-Build` count-admissibility premise, no fresh amendment logged (audit was specifically asked to stress-test that call and confirmed it held), 1 FAIL (a vacuous test, fixed) + 2 PLAUSIBLE (documented) found. 50/50 tests pass; 6 of Table 6's 10 rules plus the admissibility premise now implemented. Full per-slice detail, evidence, and audit narratives: `STATUS.md`'s "Completed milestones" table and "Phase 3.4 evidence" sections. **3.4's next work**: wire `check_count_admissibility`/`C-Idx` into live `FoldExpr`/`BuildExpr` traversal (closing the gap Phase 3.2 left open), and separately, the full `∆n`/`ν_n` certificate-hypothesis machinery `AM-026` deferred (not needed until something inside a fold/build body requires a certificate over a symbolic fact). After that: `C-ABI`/`C-Len-E`/`C-Len-U` (Table 8/`abi-layout.md`, later phases) and `C-Call` (`Q_f`, `AM-022`'s deferred certificate "substitution" mechanism). **3.5** (shape/result-size, Table 8) follows 3.4.

## Phase 4 — Reference evaluator and dynamic cost traces

- Inputs: typed core, numeric semantics, trace datatype, ABI decoder.
- Deliverables: independent rooted evaluator, immutable store, literal/fold/build auxiliaries, exact event/store/root trace, declared outcomes.
- Tests: every evaluation rule and error prefix; fresh-name isomorphism; object death; retained aliases; canonical NaN; deterministic replay.
- Acceptance: exact first-error and trace agreement with hand-worked known answers; evaluator never calls generated code.
- Dependencies: Phase 3 and ABI codec subset.
- Scientific risks: premise-root omission, peak undercount, confusing literal with builder.
- Stop conditions: nondeterminism, trace composition mismatch, or an expression without an evaluator rule.

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

