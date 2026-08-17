# Project status

Last updated: 2026-08-17

## Current phase

Phase 0 — Specification Freeze and repository preparation.

Readiness: **NO-GO for Phase 1** pending the author decisions listed in `docs/decision-log.md`. No production implementation exists.

## Completed milestones

| Milestone | Status | Evidence |
|---|---|---|
| Repository and paper inventory | Complete | All five files under `spec/paper/` inspected; PDF text extracted outside the repository; SHA-256 snapshot recorded. |
| Normative-gap analysis | Complete | Paper grammar, `Q_f`, dynamic trace, `Lambda`, ABI, BIR capability vocabulary, diagnostics, schemas, and experiment choices reviewed. |
| Repository governance and architecture | Complete | `AGENTS.md`, `README.md`, `PLAN.md`, `STATUS.md`, and `docs/architecture.md`. |
| Specification freeze documents | Complete | Normative documents under `docs/spec-freeze/`; deviations recorded in `SPEC_AMENDMENTS.md`. |
| Typed record schemas | Complete | Four Draft 2020-12 schemas under `schemas/`. |
| Traceability and readiness review | Complete | `docs/traceability.md` and `PHASE0_READINESS_REPORT.md`. |

## Open blockers

- Exact AOT runtime/backend release and embedding package.
- Exact native compiler/linker release.
- Target machine, target triple, ISA feature policy, and strict floating-point flags.
- Timer and cycle source, frequency/turbo policy, cache/page policy, and session calendar.
- Randomization PRNG/algorithm and all analysis seeds.
- Final practical-equivalence band.
- Confirmatory replication count and p99.9 anchor family, which must be selected under the frozen pilot protocol before unblinding.

Every blocker is `AUTHOR DECISION REQUIRED`; no benchmark implementation or performance work may assume a value.

## Next permitted work

Only decision-closing feasibility pilots and documentation updates are permitted. Once all blocking decisions are frozen, the first implementation milestone is Phase 1.1: create a minimal C++23/CMake/Ninja project that builds and tests an empty library plus a toolchain-manifest emitter, without DSL functionality.

