# Project status

Last updated: 2026-10-04

## Current phase

Phase 0 — Specification Freeze and repository preparation.

Readiness: **GO for Track C** (correctness and static evidence, development toolchain) since `DEC-014` was approved on 2026-10-04. **NO-GO for Track P** (timed artifacts and Phases 12–16) pending DEC-001–DEC-013. No production implementation exists yet.

## Completed milestones

| Milestone | Status | Evidence |
|---|---|---|
| Repository and paper inventory | Complete | All five files under `spec/paper/` inspected; PDF text extracted outside the repository; SHA-256 snapshot recorded. |
| Normative-gap analysis | Complete | Paper grammar, `Q_f`, dynamic trace, `Lambda`, ABI, BIR capability vocabulary, diagnostics, schemas, and experiment choices reviewed. |
| Repository governance and architecture | Complete | `AGENTS.md`, `README.md`, `PLAN.md`, `STATUS.md`, and `docs/architecture.md`. |
| Specification freeze documents | Complete | Normative documents under `docs/spec-freeze/`; deviations recorded in `SPEC_AMENDMENTS.md`. |
| Typed record schemas | Complete | Four Draft 2020-12 schemas under `schemas/`. |
| Traceability and readiness review | Complete | `docs/traceability.md` and `PHASE0_READINESS_REPORT.md`. |
| `DEC-014` approved (2026-10-04) | Complete | Author approved option (b); decision log updated; Track C unblocked. |
| Claude Code operating setup (2026-10-04) | Complete | `CLAUDE.md` (imports `AGENTS.md`); `.claude/settings.json` hooks guarding `spec/paper/`, the manuscript repository, and append-only `evidence/raw/`, tested on allowed, blocked, and tampered-copy cases; skills `/next-milestone` and `/paper-results`; subagents `spec-auditor` and `independent-oracle`; evidence layout in `docs/architecture.md`; proposed `DEC-014`. No production code. Snapshot hashes re-verified unchanged. |

## Open blockers

These block Track P only.

- Exact AOT runtime/backend release and embedding package.
- Exact native compiler/linker release.
- Target machine, target triple, ISA feature policy, and strict floating-point flags.
- Timer and cycle source, frequency/turbo policy, cache/page policy, and session calendar.
- Randomization PRNG/algorithm and all analysis seeds.
- Final practical-equivalence band.
- Confirmatory replication count and p99.9 anchor family, which must be selected under the frozen pilot protocol before unblinding.

Every blocker is `AUTHOR DECISION REQUIRED`; no benchmark implementation or performance work may assume a value.

## In progress

Phase 1.1 (Track C), started 2026-10-04. Acceptance: clean out-of-tree Ninja build of a C++23 support library with no DSL behaviour; SHA-256 known answers; toolchain-manifest emitter rejects version and hash drift (`RUN014`), emits byte-identical output across runs, and that output validates against `compiler-certificate.schema.json#/properties/toolchain_versions`; all four schemas pass Draft 2020-12 meta-schema validation.

## Next permitted work

Track C, Phase 1.1, on the development toolchain: create a minimal C++23/CMake/Ninja project that builds and tests an empty library plus a deterministic toolchain-manifest/hash emitter, without DSL functionality. Installing Ninja is part of this milestone and is recorded in the manifest. In Claude Code, run `/next-milestone`.

Track P remains limited to decision-closing feasibility work until DEC-001–DEC-013 are frozen.
