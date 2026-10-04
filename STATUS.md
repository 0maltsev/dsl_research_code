# Project status

Last updated: 2026-10-05

## Current phase

Phase 1 — Repository and toolchain foundation (Track C) is complete. Phase 2 — Lexer, parser, and AST has not started.

Readiness: **GO for Track C** (correctness and static evidence, any development host whose toolchain is exactly recorded) since `DEC-014` was approved on 2026-10-04 and `DEC-015` confirmed its host scope on 2026-10-05. **NO-GO for Track P** (timed artifacts and Phases 12–16) pending DEC-001–DEC-013. No DSL implementation exists yet — Phase 1 is pure build/toolchain infrastructure; Phase 2 is the first phase that touches the language itself.

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
| `DEC-015` approved (2026-10-05) | Complete | Author approved option (b); `DEC-014`(b) covers any development host whose exact toolchain is recorded by the Phase 1 manifest emitter, not only the host named at `DEC-014`'s approval. |
| Phase 1.1 (2026-10-05) | Complete | See "Phase 1.1 evidence" below; a `spec-auditor` review's two FAIL findings were fixed and re-verified (7/7 tests) before this update; its `DEC-015` finding is resolved above. |
| Phase 1.2 / Phase 1 overall (2026-10-05) | Complete | `.github/workflows/ci.yml` authored and statically verified (`actionlint` clean) plus its full command sequence run locally end-to-end (`npm ci`, fresh configure/build, 7/7 `ctest`, snapshot check); a `spec-auditor` review returned **PASS**, no FAIL findings. Pushed to `origin/main` (commit `3c32e95`); the workflow's first real run on a GitHub-hosted `ubuntu-24.04` runner (run `37237124426`) **succeeded** — every step, including Build and Test, passed, so the audit's flagged GCC-version risk (that runner's default GCC 12–14 vs. this session's GCC 16.1.1) did not materialize. See "Phase 1.2 evidence" below. |

## Phase 1.1 evidence

Minimal C++23/CMake/Ninja project (`CMakeLists.txt`, `CMakePresets.json`, `cmake/Dependencies.cmake`), a host-independent support library (`src/support`: OpenSSL-backed SHA-256, a JSON-Schema-fragment instance validator), and a toolchain-manifest library/CLI (`src/toolchain_manifest`) that records this host's exact cmake/ninja/C++-compiler/OpenSSL/node/npm identity plus the dev-only schema-meta-validation npm packages, and detects version/hash drift as `RUN014`.

Commands run this session and their outcomes:

- `rm -rf build && cmake --preset dev` — configures cleanly out-of-tree (`build/dev`) in ~20–30s (dependencies fetched as pinned-commit GitHub archives with a recorded `URL_HASH SHA256`, not a full git clone; see `docs/dependency-policy.md`).
- `cmake --build --preset dev` — builds cleanly with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror` on every BoundFin target and AddressSanitizer+UndefinedBehaviorSanitizer linked in.
- `ctest --preset dev` — 7/7 tests pass: `schema_meta_validation_draft_2020_12` (dev-only Node/Ajv2020 check that all four `schemas/*.schema.json` files conform to the real Draft 2020-12 meta-schema, including its `$dynamicRef`/`$dynamicAnchor` self-reference, which no available C++ library implements), `SHA256_known_answers` (vectors generated live from `sha256sum`, an independent reference, not transcribed), `record_schema_toolchain_versions_subschema`, `extract_first_version_token_from_real_tool_output`, `toolchain_manifest_emit_is_byte_identical_across_runs`, `toolchain_manifest_output_validates_against_schema`, `RUN014_toolchain_manifest_drift_rejected`.
- Manual CLI smoke test: `boundfin-toolchain-manifest emit` produces a schema-valid manifest (`cmake`, `ninja`, `cxx_compiler`, `openssl_crypto`, `node`, `npm`, `ajv`, `ajv-cli`, `ajv-formats`, each `{version, sha256}`, verified programmatically as 64 lowercase hex characters); `check --baseline <self>` reports a match; `check --baseline <tampered>` reports `RUN014 checksum_failure` and exits nonzero.
- `cmake -S . -B .` (in-tree) is refused by a `FATAL_ERROR` guard.
- `.claude/hooks/check-snapshot.sh session` — paper snapshot unchanged.
- A `spec-auditor` review of this milestone found two FAIL defects, both fixed and re-verified (rebuild + 7/7 tests) before this update: `ajv`/`ajv-cli`/`ajv-formats` identity now hashes each package's `package-lock.json` `resolved`+`integrity` fields (npm's own content-addressed identity) rather than the installed `package.json` alone, which would have missed a tarball change without a version bump; and `docs/dependency-policy.md` falsely claimed OpenSSL's identity was folded into "the usual toolchain versioning" when it was not recorded anywhere — OpenSSL now has its own `openssl_crypto` manifest entry. The audit also surfaced the `DEC-015` host-scope question, resolved above (approved, option (b)).

## Phase 1.2 evidence

`.github/workflows/ci.yml`: a GitHub Actions workflow ("Track C smoke check") that on push to `main`/PRs/manual dispatch checks out the repo, installs `ninja-build`+`libssl-dev`, installs Node 22, runs `npm ci` in `tools/schema-validate/`, then `cmake --preset dev` → `cmake --build --preset dev` → `ctest --preset dev --output-on-failure` → `.claude/hooks/check-snapshot.sh session`, on a pinned `ubuntu-24.04` runner image (not the floating `ubuntu-latest` alias). `actions/checkout` and `actions/setup-node` are pinned to exact commit SHAs; the `spec-auditor` independently re-verified both SHAs against the real upstream tags (v4.4.0 each) via `git ls-remote --tags`, confirming neither silently pins the wrong code. The workflow's header comment states explicitly that it is not a performance artifact and that a green run is not itself a G1/G2 result. Also added: a new `tests/toolchain_manifest/test_manifest_drift_rejected.cpp` case exercising a realistic tool-version upgrade (not just a tampered hash), closing `PLAN.md` Phase 1's named "tool version rejection" test item; and a `docs/dependency-policy.md` section recording `actionlint` v1.7.12's one-off, non-committed use to statically check the workflow file (SHA-256 of the downloaded release archive independently re-verified by the `spec-auditor` against the release's own published checksum).

Commands run and their outcomes:

- `actionlint -color .github/workflows/ci.yml` — exit 0, no findings.
- `cd tools/schema-validate && rm -rf node_modules && npm ci` — clean install from the committed `package-lock.json`; `node check-meta-schema.cjs` — all four schemas PASS.
- `rm -rf build && cmake --preset dev && cmake --build --preset dev` — clean out-of-tree configure and build, 0 warnings.
- `ctest --preset dev` — 7/7 pass, including the new version-upgrade drift case.
- `.claude/hooks/check-snapshot.sh session` — paper snapshot unchanged.
- A `spec-auditor` review returned **PASS** (no FAIL findings) and additionally independently re-verified: both pinned Action SHAs match their claimed tags; the SHA-256 recorded for `actionlint`'s release archive matches its published checksum; `cmake_minimum_required(VERSION 3.25)` is satisfied by `ubuntu-24.04`'s shipped CMake; the new test case genuinely asserts `RUN014`/`DriftStatus::kDrift`/the drifted tool name, not merely executing the code path; the workflow has minimal `permissions: contents: read`, touches no secrets and no protected path. It flagged one **PLAUSIBLE** risk (no real-runner execution yet, so a GCC-version-sensitive warning under `-Werror` was not yet ruled out — resolved below), and one **NOTE** (addressed: `actionlint`'s non-blocking status is now stated explicitly in `docs/dependency-policy.md`).
- The audit also reported, as an aside unrelated to this milestone's substance, that a tool-result in its session contained a prompt-injection attempt (an impersonated "MCP Server Instructions" block trying to direct it to create a docs artifact); it correctly disregarded this and took no action on it.
- **First real CI run (2026-10-05, after pushing commit `3c32e95` to `origin/main`)**: [run `37237124426`](https://github.com/0maltsev/dsl_research_code/actions/runs/37237124426) on a GitHub-hosted `ubuntu-24.04` runner — **succeeded**, every step green (checkout, install Ninja/OpenSSL/Node, `npm ci`, configure, build, test, snapshot check), total job time 34s. This resolves the `spec-auditor`'s PLAUSIBLE risk: the real runner's default GCC did not trip `-Werror` differently from this session's GCC 16.1.1.

## Open blockers

These block Track P only.

- Exact AOT runtime/backend release and embedding package.
- Exact native compiler/linker release.
- Target machine, target triple, ISA feature policy, and strict floating-point flags.
- Timer and cycle source, frequency/turbo policy, cache/page policy, and session calendar.
- Randomization PRNG/algorithm and all analysis seeds.
- Final practical-equivalence band.
- Confirmatory replication count and p99.9 anchor family, which must be selected under the frozen pilot protocol before unblinding.

Every blocker above is `AUTHOR DECISION REQUIRED`; no benchmark implementation or performance work may assume a value.

## Next permitted work

Track C, Phase 2 (Lexer, parser, and AST): read `grammar.ebnf` and `diagnostics-and-status.md` (LEX/SYN) per `CLAUDE.md` §2's phase-reading table, plus `SPEC_AMENDMENTS.md` AM-001/AM-002, then implement the lossless-span tokenizer first (the narrowest independently verifiable slice). In Claude Code, run `/next-milestone`.

Track P remains limited to decision-closing feasibility work until DEC-001–DEC-013 are frozen.
