# CLAUDE.md — BoundFin research artifact

@AGENTS.md

`AGENTS.md` (imported above) is the shared governance contract for every agent. This file adds the Claude Code operating procedure. If the two ever conflict, `AGENTS.md` wins and the conflict is reported to the author.

## 1. Purpose

This repository is the reference implementation and experimental artifact for the paper *BoundFin: Static Multi-Resource Cost Semantics and an AOT-Wasm Translation Specification for Stateless Financial Kernels*. The paper (snapshot in `spec/paper/main.pdf`) specifies a restricted DSL, a scalar cost/footprint analysis, a source → BIR → Wasm → AOT pipeline, and a preregistered experiment. Its Results section is empty. The main part of the research — the work done here — is to build the pipeline and produce the evidence that section requires:

| Paper output (`EXPERIMENT_IMPLEMENTATION_SPEC.md` §7.5) | Question | Track |
|---|---|---|
| Formal-status / G1 / G2 coverage table, retained counterexamples | RQ1, RQ2 | C |
| Exact / symbolic-upper / capacity bounds vs. dynamic traces; tightness and slack | RQ1 | C |
| Cumulative `h`, peak `p`, BIR frame/stack, ABI in/out, total linear memory | RQ1 | C |
| K1 expressibility and known-answer cross-check (closed forms in paper §9.3) | RQ1, RQ2 | C |
| MS and PO Wasm/native ratio forests with simultaneous intervals | RQ3 | P |
| Within-kernel size contrasts; K3 proportion × pattern and branch-site counters | RQ4 | P |
| K1/K2-only explicit-SIMD speedups and interactions | RQ5 | P |
| D0–D5 call-boundary decomposition, tails, counters, code size, disassembly | RQ3–5 | P |
| Invalid-run / missing-counter / deviation ledger | all | C+P |

Current state: Phase 0 (specification freeze) is complete. **Phase 1 is NO-GO**, and no code exists. Read `STATUS.md` first in every session.

## 2. Sources of truth

Authority order (from `docs/spec-freeze/SPEC_FREEZE.md`): paper snapshot → `docs/spec-freeze/*` normative files → `SPEC_AMENDMENTS.md` → `docs/decision-log.md` → informative docs (`PLAN.md`, `STATUS.md`, `docs/architecture.md`, `docs/traceability.md`).

- `spec/paper/main.pdf` is authoritative. It runs 55 pages; read it in page ranges with the Read tool's `pages` parameter.
- `../dsl_research_paper/sections/*.tex` are the LaTeX sources of the same PDF. On 2026-10-04, `../dsl_research_paper/main.pdf` was byte-identical to `spec/paper/main.pdf`, so the TeX may be read as a searchable text rendering. If the paper repository's PDF hash ever differs from `SPEC_FREEZE.md`, use only the snapshot.
- The paper repository is **read-only** from this project (hook-enforced). Manuscript changes, including amendment propagation, are done in a session started in that repository.

Read only what the current phase needs:

| Phase | Read first |
|---|---|
| 1 Foundation | `SPEC_FREEZE.md`; `toolchain-feasibility.md` (configuration records); `schemas/*`; decision log |
| 2 Lexer/parser | `grammar.ebnf`; `diagnostics-and-status.md` (LEX/SYN); AM-001, AM-002 |
| 3 Static semantics | paper §4 and App. A.4–A.5 (`11-formal-appendix.tex`: `app:count`, `app:typing-size`, Tables `count-rules`, `typing-rules`, `size-rules`); `numeric-semantics.md`; diagnostics NAM/TYP/SIZ/EFF; AM-003 |
| 4 Reference evaluator | paper §5 and App. A.6 (`app:evaluation`, Table `evaluation-rules`); `cost-trace-semantics.md` §1–5; `numeric-semantics.md`; `abi-layout.md` codec sections |
| 5 Cost analysis | paper §6 and App. A.7–A.8 (`app:cost-rules`, Table `construct-costs`, `app:layout`); `cost-trace-semantics.md` §6–8; AM-004, AM-005 |
| 6 BIR | `bir-spec.md`; paper App. B.1–B.2 (`12-bir-lowering-appendix.tex`: `app:bir-machine`, Tables `bir-faults`, `bir-steps`); diagnostics BIR; AM-009, AM-010, AM-015 |
| 7 Source→BIR | paper §7.1–7.4; App. B.3–B.4 (`app:bir-live`, `app:lowering`, Table `lowering-complete`); `bir-spec.md` §10 |
| 8 BIR→Wasm/AOT | `abi-layout.md`; paper §7.5–7.7 and Table `wasm-lowering`; `toolchain-feasibility.md`; diagnostics ABI/WASM; AM-006–AM-008, AM-013, AM-014 |
| 9 Kernels/datasets | paper §9.3 (`09-methodology.tex`, `sec:kernels`); implementation spec §5 |
| 10 Native baseline | `numeric-semantics.md` (native obligations); implementation spec §3.6; paper §7.6 |
| 11 Correctness gates | paper §9.1, §9.4; implementation spec §4; `docs/traceability.md` |
| 12–16 Benchmark/analysis | implementation spec §6–7; `experiment-matrix.md`; paper §9.2 and §9.5–9.8 |

Cite rule identifiers (for example `E-FoldErr`, `C-Build`, `AM-003`, `BIR012`) in test names, code comments where non-obvious, and records.

## 3. Execution tracks and gates

The implementation splits into two tracks with different blockers.

**Track C — correctness and static evidence (machine-independent semantics).** Phases 1–7, plus the correctness-only parts of 8–11: Wasm emission and validation, a Wasm engine used only to execute modules for G2, the native oracle, datasets, and the G1/G2 suites. Under the current plan these phases are blocked by DEC-001 and DEC-002. **DEC-014** proposes letting them run on a recorded development toolchain. Track C may start only after the author marks DEC-014 approved in `docs/decision-log.md`.

**Track P — performance evidence.** Frozen AOT images, MS/PO/SIMD timed artifacts, and Phases 12–16. Blocked by DEC-001–DEC-013 and requires the dedicated experimental machine. The current development host (Apple M1, macOS 14.1.1, no Linux `perf`, no frequency control) cannot produce Track P evidence. Any timing taken here is a smoke measurement, must be labelled so in every record and message, and must never reach a paper table.

Gate rules:

- A decision is closed only by the author's explicit statement, recorded in the decision log with a date. A recommendation in the log is never authorization, including your own.
- Track C artifacts carry `dev-toolchain` identity in every record. Before any paper table is frozen, rebuild them with the frozen toolchain and re-run the full G1/G2 suites on the exact artifacts. Development-track results are previews, not final evidence.
- No timing of any artifact before G1/G2 pass for that exact artifact hash (`AGENTS.md`).

## 4. Milestone loop

Use `/next-milestone` (`.claude/skills/next-milestone/`). In brief:

1. Orient from `STATUS.md` and the phase entry in `PLAN.md`; check the blocking decisions in the decision log.
2. Choose the smallest independently verifiable milestone (split phases as `N.1`, `N.2`, …). Mark it in progress in `STATUS.md`.
3. Read the phase's documents (§2 table). Scan for behaviour the spec does not determine; if found, follow §5.
4. Write the narrowest test that demonstrates the rule (named by rule ID), then implement, then run broader checks.
5. Validate generated records against `schemas/`; run the snapshot check.
6. Run the `spec-auditor` subagent on the diff and resolve its findings.
7. Update `STATUS.md`, `PLAN.md`, and `docs/traceability.md` with fresh evidence (commands run and their outcomes).
8. Report results at the correct claim level (§7) and name the next milestone or blocker.

Commit only when the user asks; then use one commit per milestone.

## 5. Stop-and-ask protocol

Stop the affected work when you hit any of the following:

- an `AUTHOR DECISION REQUIRED` item;
- behaviour not determined by the spec and amendments;
- a contradiction between documents;
- a G1/G2 counterexample, mismatch, or target fault;
- a phase stop condition in `PLAN.md`.

Then:

1. Record it: a decision-log entry (alternatives, recommendation, evidence required, status `AUTHOR DECISION REQUIRED`), or a *proposed* amendment draft in `SPEC_AMENDMENTS.md` marked as awaiting approval, or a retained counterexample record.
2. Ask the author with the AskUserQuestion tool. Give the alternatives from the log, put the recommendation first, and state its scientific effect.
3. Continue only with work that does not depend on the answer. Never implement a placeholder default "for now".

A counterexample is a result, not a bug to hide. Retain it (source, input bytes, seeds, configs, traces, minimized case), mark the affected obligation reopened, and block the affected artifacts.

## 6. Engineering conventions

- C++23, CMake presets, and Ninja. Production components contain no Python; Python is permitted only under `analysis/` for statistics, tables, and plots.
- Module ownership follows `docs/architecture.md`. One component must not take over another's responsibility (for example, the evaluator must never call generated code).
- **Independent implementations.** The reference evaluator, BIR evaluator, BIR validator, and native C++ oracle must not share semantic code; otherwise correlated defects invalidate G2. Write each second implementation with the `independent-oracle` subagent, giving it the spec sections and a list of forbidden source paths. Shared code is limited to the ABI byte codec and record serialization, and even that must have its own known-answer tests.
- Preserve left-to-right order, first-error behaviour, operation identity and count, source locations, and event identity across proof-oriented stages. No constant folding, CSE, reassociation, or hoisting before Wasm.
- Numerics: two's-complement modular integers without signed-overflow UB in the C++ (use unsigned arithmetic or explicit wrapping); canonicalise NaN after every floating primitive in the evaluator and oracle; never use fast-math or contraction. Development builds use `-ffp-contract=off` and no fast-math. Record flags verbatim. This satisfies the frozen numeric semantics but does not close DEC-004.
- Determinism: no wall-clock, address, hash-map iteration order, or locale in any serialized artifact. Every generator is seeded and its seed recorded.
- Development tools may be installed only as part of an approved track (for example, Ninja for Track C). Record each installed tool and version in the toolchain manifest.

Commands (planned; replace this block with the real ones when Phase 1 lands):

```sh
cmake --preset dev && cmake --build --preset dev   # build
ctest --preset dev                                  # all tests
ctest --preset dev -R <rule-id>                     # narrowest test
.claude/hooks/check-snapshot.sh session             # paper snapshot integrity
```

## 7. Evidence, results, and claim wording

Layout (defined in `docs/architecture.md`, "Evidence layout"):

- `evidence/raw/` — append-only schema-valid records (correctness results, certificates, benchmark samples, manifests). The hook blocks edits and overwrites. A correction is a new record that references the old one.
- `evidence/derived/` — deterministic outputs regenerated from raw records by scripts under `analysis/`.
- `evidence/derived/paper/<version>/` — the paper hand-off package produced by `/paper-results`: table fragments, figures, `provenance.json`, and `HANDOFF.md`.

Every number in a paper-facing output must be computed by a script from raw records. Never type, estimate, round by hand, or "fill in" a number. Missing evidence yields an explicit "not available: <reason>" cell.

Permitted wording by claim level:

| Level | Say | Never say |
|---|---|---|
| Formal | "proved in the paper (TH-…)" or "open (PO-…)" | "verified", "sound", "correct" because tests pass |
| G1 | "no bound counterexample detected in the declared domain <domain>" | "the bounds are sound" |
| G2 | "no observable mismatch detected in the declared suite <suite>" | "the compiler is correct" |
| Performance | "estimate for <kernel/config/machine>, MS (or PO)" | pooled MS+PO+SIMD, workload-class generalisations |
| Dev host | "smoke measurement, not paper evidence" | any ratio or latency in a paper table |

Keep exact, upper, and capacity bounds separate. Keep cumulative, peak, frame, output, and total memory separate. Label K1 SIMD batch time "amortised per-contract batch time".

## 8. Claude Code mechanics

- **Hooks** (`.claude/settings.json`): `guard-paths.sh` blocks writes to `spec/paper/`, to the manuscript repository, and edits or overwrites under `evidence/raw/`. `check-snapshot.sh` verifies the snapshot hashes at session start and blocks finishing a turn while they mismatch. Shell commands are not intercepted, so never use `sed -i`, `mv`, `rm`, or redirection on protected paths.
- **Skills**: `/next-milestone` advances one milestone. `/paper-results` builds the paper hand-off package.
- **Subagents** (`.claude/agents/`): `spec-auditor` is a read-only review of a milestone against the spec and claim discipline, and must pass before a milestone is marked complete. `independent-oracle` writes a second implementation from the spec alone.
- **Long sessions**: write progress, the current milestone, and the next concrete step into `STATUS.md` before context runs low, so a resumed or compacted session can continue from the repository rather than from memory.
- **Memory**: do not save spec facts or project status to auto-memory. The repository is the record.
