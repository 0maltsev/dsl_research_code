# Phase 0 readiness report

Date: 2026-08-17

Decision: **NO-GO for Phase 1**

The implementation-level language, numeric, trace, footprint, ABI, BIR, diagnostic, and record contracts are now frozen. The repository is nevertheless NO-GO because exact toolchain/machine/timing/randomization/equivalence/replication choices can materially affect the planned scientific claims and remain `AUTHOR DECISION REQUIRED`. The Phase 0 instruction explicitly prohibits GO in that state.

## Documents inspected

The repository initially contained exactly five files, all inspected in full:

| Document | Inspection |
|---|---|
| `spec/paper/main.pdf` | All 55 pages inspected through local text extraction, including normative Appendices A/B and references. |
| `spec/paper/EXPERIMENT_IMPLEMENTATION_SPEC.md` | All 440 lines inspected. |
| `spec/paper/notation.md` | Inspected in full. |
| `spec/paper/open-decisions.md` | Inspected in full. |
| `spec/paper/proof-obligations.md` | Inspected in full. |

There was no existing `AGENTS.md`, implementation, build configuration, schema, test, CI file, or visible Git repository metadata. The immutable paper hashes before and after Phase 0 match the values in `SPEC_FREEZE.md`.

Current official Wasmtime documentation was also reviewed for precompilation, compiler-free runtime builds, C/C++ embedding, `.cwasm` disassembly, feature configuration, and Linux performance profiling. This was a feasibility review only; no runtime release was selected or installed.

## Ambiguities found

1. The paper gives abstract grammar but no complete concrete module/token/operator grammar.
2. Count rule C-Call uses undefined `Q_f`, while the stated environment stores `K_f`/`Phi_f`.
3. Dynamic peak is described over configurations but lacks an explicit trace datatype and total composition function.
4. `Lambda` is not total for scalars, ABI parameters, structured products, local arrays, accumulators, call formals, and projected/indexed results.
5. Boolean ABI words, `A(tau)`, product padding/offsets, nested aggregate placement, exact roots, alias restrictions, output convention, export names, and status integers are absent.
6. Capability names conflict: `inR/fresh/imm/scratch/outW` versus `inputRead/freshInit/scratchInit/outputWrite`.
7. BIR omits a single implementation contract for capability phase changes, output coverage, certified death/recovery boundaries, and interpreter agreement.
8. Stable negative-conformance and runtime failure codes are not assigned.
9. Generated-record field lists have no typed/nullability/versioned schema.
10. Toolchain feasibility requirements are stated without an executable acceptance protocol.
11. The full workload cross-product, artifact count, sample formula, runtime/storage capacity, and realistic p99.9 scope are not enumerated.
12. Product minimum arity, concrete projection indexing, strict Boolean operator evaluation, output NaN serialization, padding contents, and nested capacity-tree ownership are implicit or absent.
13. Exact runtime/compiler/machine/ISA/floating flags/timer/frequency/cache/randomization/seed/equivalence/sample/statistical software decisions remain open.

## Ambiguities resolved

- A complete concrete UTF-8 EBNF freezes tokens, declarations/export, types, exact bit literals, arrays/products, fold/build, calls, precedence, associativity, and one-for-one primitive elaboration.
- `Sigma(f)` now stores `(signature,K_f,Q_f?,Phi_f,rank)`. `Q_f` exists only for checked `i32` count-return bodies and is simultaneously substituted with `K_f`/`Phi_f`; otherwise calls are forbidden only in the count fragment.
- Dynamic traces are finite start/step/snapshot/stop values with compatibility-checked concatenation. `peak_K` is the exact maximum rooted local-region sum over every snapshot, including pre-failure states.
- Recursive footprint descriptors make `Lambda` total and preserve product/array substructure. ABI aggregates map to zero local bytes; call formals receive caller-instantiated descriptors; every accepted expression has total result/peak equations.
- Numeric semantics are exact for all primitive types, integer overflow/errors, strict floating behavior, canonical source NaN, signed zero, and external observation.
- The little-endian Wasm32 ABI now fixes scalar words, `A(tau)`, widths/strides/product fields, array headers, nested capacity trees, record headers, zero padding, disjoint ownership, caller-owned deep result output, exports, statuses, validation, traps, and host duties.
- BIR uses exactly four canonical capability kinds with explicit internal phase/coverage state, complete validation and small-step behavior, terminal states, ordered faults, and interpreter obligations.
- Stable `LEX/SYN/NAM/TYP/SIZ/EFF/CST/BIR/ABI/WASM/INT/RUN` codes and source status words are assigned.
- Four strict Draft 2020-12 schemas type manifests, compiler certificates, correctness results, and benchmark samples, with explicit nullability and required version/configuration/toolchain/dataset/seed/checksum/failure fields.
- AOT feasibility is a gated pilot: real `.cwasm`, compiler-free timed loader, scalar/PO profiles, reproducible disassembly, counters, fixed boundary, numeric/memory checks, and clean-host replay.
- The matrix is 93 workload cells, 450 optimized execution conditions, 16 executable treatments, and 24 primary executable build files. Sample/runtime/storage are formulas and labelled planning scenarios, not fabricated results or frozen counts.

All resolutions are linked to paper locations and propagation requirements in `SPEC_AMENDMENTS.md`.

## Unresolved author decisions

The following remain blockers in `docs/decision-log.md`:

- `DEC-001`: exact AOT runtime/backend release.
- `DEC-002`: exact native compiler/linker/standard library release.
- `DEC-003`: experimental machine, target triple, and ISA feature lists.
- `DEC-004`: concrete strict floating-point flags.
- `DEC-005`: timing and cycle sources.
- `DEC-006`: frequency/turbo/SMT/thermal policy.
- `DEC-007`: cache/page/NUMA/ASLR/prefault policy.
- `DEC-008`: randomization algorithm, portable PRNG, and seeds.
- `DEC-009`: practical-equivalence limits.
- `DEC-010`: confirmatory sessions/pairs/runs/invocations after pilot.
- `DEC-011`: exact statistical software/version and secondary test inventory.
- `DEC-012`: p99.9 anchor family after pilot.
- `DEC-013`: session calendar and machine-access budget.

No benchmark implementation or measurement may assume a value for these decisions.

## Contradictions and inconsistencies

| Source locations | Inconsistency | Freeze resolution |
|---|---|---|
| Main Appendix B.1 vs Section 7.2 and hand-off 3.2 | Five short capability forms include `imm`; prose names four long capabilities. | Four long kinds; sealed state is a `freshInit` phase, not a fifth kind. |
| Table 6 C-Call vs Sections 4.2/4.3 | `Q_f` is used but never installed in `Sigma`. | Optional checked `Q_f` stored beside and derived under the same formals as `K_f`. |
| Appendix A.1 vs Phase 0 grammar requirement | Appendix says conveniences outside lexical categories have no core meaning; implementation needs operator precedence. | Versioned one-for-one surface elaboration recorded as AM-001; must be propagated/cited. |
| Section 7.5 | Memory export is optional, but the frozen byte-buffer ABI needs host memory access. | `memory` is always exported for ABI version 0.1.0. |
| Appendix A.7/A.8 vs nested aggregate ABI need | Immediate `region(tau,N)` does not define a complete nested external layout. | Keep source immediate region distinct from target `TreeBytes(tau)` capacity tree. |
| Main Section 8 labels vs `proof-obligations.md` | Main groups PO-IW while the ledger splits PO-IW-CFG/PO-IW-NUM; numbering and names vary. | Traceability uses stable descriptive obligation IDs and records both sub-obligations; no proof status changes. |
| User-facing paper title vs snapshot PDF title | The requested artifact title and snapshot's “BoundFin” manuscript title differ. | Repository describes the requested paper while preserving immutable snapshot identity/hashes. Authors should align titles before publication. |

No contradiction was resolved by editing `spec/paper/`.

## Completeness assessment

| Area | Assessment | Residual risk/status |
|---|---|---|
| Grammar | Complete for language 0.1.0 | No comments/decimal floats by design; parser implementation absent. |
| Numeric/operational semantics | Complete implementation contract | Type/size preservation remains an open paper obligation. |
| Dynamic cost trace | Complete datatype/composition/peak contract | Static soundness and call/alias proof obligations remain open. |
| Static footprint/cost analysis | Total equations for accepted syntax | Analyzer must reject unavailable transformers; candidate bounds are not proved. |
| ABI/layout | Complete source/Wasm/native boundary | Requires independent codec/address tests and exact machine memory manifest. |
| BIR | Complete syntax/state/capability/validation/transition contract | Source-to-BIR, memory, Wasm simulation, and no-fault proofs remain open. |
| Diagnostics/status | Complete initial stable catalog | New implementation cases require additive codes, never reused codes. |
| Schemas | Complete Draft 2020-12 source documents; JSON syntax valid | A Draft 2020-12 validator was unavailable locally, so meta-schema validation remains to rerun in Phase 1. |
| AOT feasibility | Complete pilot plan; documentation supports candidacy | Wasmtime and Ninja absent locally; no gate executed; release not frozen. |
| Experiment design | Complete cells/hierarchy/formulas/gates | Scientifically material machine, seeds, band, replication, and tail anchors remain open. |
| Traceability | Complete planned paper-to-evidence map | Planned module/test paths do not yet exist. |

## Validation performed

- `jq empty schemas/*.json`: passed for all four schemas.
- Python standard-library structural checks: passed for JSON parsing, Draft 2020-12 declarations, local `$ref` resolution, required/property consistency, and schema type names.
- Required-file inventory: all requested files present after this report.
- Immutable snapshot SHA-256 comparison: passed; all five hashes unchanged.
- Development tool inventory: CMake/Clang present; Ninja/Wasmtime/`wasm-tools` absent; no dependency installation attempted.
- Full Draft 2020-12 validation: not run because neither `jsonschema` nor `check-jsonschema` is installed.

## Readiness decision

**NO-GO.** No unresolved language, cost, ABI, or BIR semantic choice remains for implementation, but open decisions would force Phase 1/toolchain work and later experiment code to invent scientifically material configuration identities. That violates the Phase 0 GO rule.

Because the decision is NO-GO, there is no authorized first implementation milestone. The next permitted milestone is decision-closing feasibility work only. After the authors freeze every blocking decision and this report is reissued as GO, the exact first implementation milestone will be Phase 1.1: create a minimal C++23/CMake/Ninja project that builds and tests an empty library plus a deterministic toolchain-manifest/hash emitter, with no DSL functionality.

## Files created

Phase 0 created 22 requested files: four root governance/status files, thirteen documentation files, four schemas, and this readiness report. It created no production source, placeholder implementation, build target, kernel, runtime, or benchmark output.
