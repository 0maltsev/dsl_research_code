# Repository instructions

These instructions apply to the entire repository.

## Scientific integrity

- `spec/paper/` is an immutable paper snapshot. Never edit, reformat, regenerate, or replace a file below it.
- Every scientific choice must be traceable to a frozen specification item or an entry in `docs/decision-log.md`.
- Never fabricate benchmark results, raw samples, counters, checksums, proof results, or tool output.
- Measurements from a development machine are smoke tests, not paper evidence.
- Functional equivalence, static-bound soundness, and machine-performance evidence are separate claim levels and must be reported separately.
- SIMD is a separate K1/K2 ablation. It must not be pooled with matched-scalar or production-optimised results, and K3 has no explicit-SIMD treatment.
- Compiler correctness must not be inferred from matching benchmark outputs alone.
- Performance experiments must not start until the applicable correctness gates pass for the exact artifacts being timed.
- A material semantic or experimental ambiguity requires an explicit stop and an author decision. Do not invent a default.

## Engineering discipline

- The intended production language is C++23 and the intended build system is CMake with Ninja.
- Python may be used later only for experiment analysis and plotting, not for production language or compiler components.
- Production changes require tests at the narrowest level that demonstrates the behavior, followed by proportionate broader checks.
- Preserve left-to-right evaluation, first-error behavior, exact operation order, source event identity, and source locations across proof-oriented stages.
- Generated records must validate against the versioned schemas in `schemas/` and retain configuration, dataset, seed, toolchain, and artifact identity.
- Raw experiment data are append-only. Corrections produce a new record or dataset version; they do not rewrite observed rows.
- Keep source inputs, checked core, BIR, Wasm, AOT images, native artifacts, manifests, certificates, disassembly, raw data, and derived outputs as distinct artifact classes.

## Progress records

- Update `STATUS.md` and `PLAN.md` after every completed milestone.
- Record a semantic clarification or paper contradiction in `docs/spec-freeze/SPEC_AMENDMENTS.md` before implementing behavior that depends on it.
- Record a scientifically material open choice in `docs/decision-log.md` with alternatives, a recommendation, evidence required, and `AUTHOR DECISION REQUIRED` status.
- A phase is complete only when its acceptance criteria and stop conditions in `PLAN.md` have been checked with fresh evidence.

