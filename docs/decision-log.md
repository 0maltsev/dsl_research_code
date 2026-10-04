# Scientific decision log

Last updated: 2026-10-05

This log contains choices that can change a scientific interpretation, artifact, or comparison. A recommendation is not authorization. Every open row has status **AUTHOR DECISION REQUIRED** and blocks Phase 1 or later work at the boundary shown. No comparative measurement may be observed before all choices that affect it are frozen.

## Open decisions

### DEC-001 — AOT runtime, backend, and exact release

- Boundary blocked: Phase 1 dependency acquisition and Phase 8.
- Alternatives: Wasmtime/Cranelift; WAMR AOT; Wasmer single-pass/Cranelift; a second runtime as sensitivity only.
- Scientific effect: machine code, validation path, host boundary, startup, counters, and runtime overhead.
- Recommendation: select one stable Wasmtime release with Cranelift for the primary implementation, pin release archive/source and SHA-256, and treat every version change as a new configuration. Do not use a moving `dev` build.
- Evidence required: all feasibility gates in `docs/spec-freeze/toolchain-feasibility.md`, including compiler-free `.cwasm` loading in the timed process.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-002 — Native compiler, linker, standard library, and exact releases

- Boundary blocked: Phase 1 and Phase 10.
- Alternatives: frozen upstream Clang/LLVM toolchain; GCC; platform compiler plus explicit linker.
- Scientific effect: optimization, vectorization, floating lowering, ABI, native baseline quality, and disassembly.
- Recommendation: frozen upstream Clang/LLVM plus its recorded linker, with C++23 support and separately named MS/PO flag manifests.
- Evidence required: C++23 configure/build probe, UB/sanitizer suite, strict-float audit, vectorization reports, and exact binary hashes.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-003 — Experimental machine, target triple, and ISA features

- Boundary blocked: Phase 8 machine-code freeze and all performance work.
- Alternatives: dedicated x86-64 host; dedicated AArch64 host; portable baseline ISA; host-native feature set.
- Scientific effect: instruction selection, SIMD width, counters, frequency control, and external validity.
- Recommendation: choose one dedicated Linux host and freeze exact triple, CPU model/stepping/microcode, Wasmtime target, Clang target, and feature allow/deny lists. Use a portable profile only as a separately named sensitivity.
- Evidence required: capability inventory, cross-tool feature agreement, MS no-vector audit, PO feature audit, and AOT compatibility test.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-004 — Floating-point compilation mode and concrete flags

- Boundary blocked: Phases 8, 10, and 11.
- Alternatives: strict IEEE/Wasm-compatible flags; compiler defaults after audit; explicit NaN canonicalization in target code.
- Scientific effect: bitwise results, NaN behavior, reassociation, contraction, and baseline comparability.
- Recommendation: strict semantics with fast-math, unsafe reassociation, reciprocal approximation, excess precision, and contraction disabled; preserve the source operation tree. Do not enable target NaN canonicalization unless both its semantic and performance roles are separately frozen.
- Evidence required: verbatim flags, compiler reports, special-value bit tests, and disassembly confirming no FMA contraction or reassociation.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-005 — Primary timing and cycle sources

- Boundary blocked: Phase 12.
- Alternatives: invariant hardware counter cross-checked by `clock_gettime(CLOCK_MONOTONIC_RAW)`; raw monotonic clock only; platform-specific counter plus calibration.
- Scientific effect: resolution, overhead, portability, frequency interpretation, and tail estimates.
- Recommendation: invariant cycle counter as primary low-level source only if documented invariant on the selected host, cross-checked against monotonic raw time; retain both raw values and calibration.
- Evidence required: monotonicity, serialization, overhead, drift, migration, and frequency calibration across sessions.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-006 — CPU frequency, turbo, SMT, and thermal policy

- Boundary blocked: Phase 12.
- Alternatives: fixed frequency/turbo disabled; fixed performance governor with turbo; production-default sensitivity.
- Scientific effect: variance, tail behavior, reproducibility, and ecological validity.
- Recommendation: controlled fixed-frequency primary with the sibling disabled or demonstrably idle; an optional production-like policy is a non-pooled sensitivity.
- Evidence required: host privilege/capability audit, logged effective frequencies, thermal limits, and invalidation thresholds.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-007 — Cache, page, NUMA, ASLR, and prefault policy

- Boundary blocked: Phase 12.
- Alternatives: warm code/data caches; randomized/cold cache protocol; both as separate conditions; normal versus huge pages.
- Scientific effect: latency level, tails, memory traffic, and boundary attribution.
- Recommendation: warm steady-state primary after deterministic prefault/warm-up, normal fixed pages, single NUMA node, and recorded ASLR/locking policy. Cold-cache behavior, if studied, must be a separate matrix.
- Evidence required: page-fault checks, warm-up stability, NUMA binding, cache protocol validation, and environment manifest.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-008 — Randomization algorithm, PRNG, and seeds

- Boundary blocked: Phases 9, 12, 14, and 15.
- Alternatives: a specified counter-based generator; PCG family; language-library generator; cryptographic hash ranking for every random order.
- Scientific effect: schedule balance, simulation reproducibility, dataset order, and bootstrap results.
- Recommendation: use SHA-256 ranking where already specified for datasets; use one portable, test-vector-defined counter-based PRNG for schedules/simulation; freeze independent domain-separated seeds before data.
- Evidence required: algorithm/version, byte encoding, test vectors, seed derivation, and schedule regeneration hashes.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-009 — Practical-equivalence limits

- Boundary blocked: Phase 15 and confirmatory interpretation.
- Alternatives: +/-1%; provisional +/-5%; an externally justified SLA/domain band.
- Scientific effect: faster/equivalent/slower classification and pilot precision target.
- Recommendation: retain the paper's provisional ratio band `[0.95, 1.05]` only if the authors explicitly justify it as a design threshold, not a financial-industry standard; otherwise freeze a domain-derived band before pilot simulation is interpreted.
- Evidence required: signed author rationale and preregistration entry.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-010 — Confirmatory sessions, pairs, runs, and invocations

- Boundary blocked: Phase 15.
- Alternatives: arbitrary fixed minimum; the paper's claims-excluded precision pilot; reduced preregistered family when infeasible.
- Scientific effect: interval precision, dependence support, runtime, and storage.
- Recommendation: use the frozen pilot selection over `S={10,12,...,30}` and `P={4,6,8,10}`; then freeze runs and invocations per cell from calibrated block support. Never select counts from comparative effect direction.
- Evidence required: five-session pilot, valid block rules, fixed-seed 10,000-replicate precision simulation, and capacity budget.
- Status: **AUTHOR DECISION REQUIRED** after the pilot.

### DEC-011 — Statistical implementation and test inventory

- Boundary blocked: Phase 15.
- Alternatives: paper-specified hierarchical max-standardized bootstrap; cluster-robust intervals; sign-flip intervals; point-null tests.
- Scientific effect: uncertainty, multiplicity, and classification.
- Recommendation: use the hierarchical simultaneous interval as primary exactly as specified; use sign-flip and cluster-robust methods only as named sensitivities; Holm only for secondary point-null tests. Freeze software/library versions and numerical quantile conventions.
- Evidence required: known-answer synthetic datasets, deterministic seed, hierarchy preservation, and method audit.
- Status: **AUTHOR DECISION REQUIRED** for exact software/version and the final list of secondary tests; the primary method is specification-frozen.

### DEC-012 — p99.9 reporting family

- Boundary blocked: Phase 15.
- Alternatives: every condition; selected anchor conditions; omit p99.9 entirely.
- Scientific effect: multiplicity, runtime/storage, and claims about extreme tails.
- Recommendation: preregister a small anchor set only after the pilot demonstrates block support; initially consider K1 depths up to 64 and K2/K3 sizes up to 4096, with one representative stratum/pattern per kernel. Omit rather than weaken any eligibility rule.
- Evidence required: at least one million valid invocations, 1,000 nominal tail observations, 200 tail-containing blocks, ten sessions, and stable block sensitivity per selected condition.
- Status: **AUTHOR DECISION REQUIRED** after the pilot.

### DEC-013 — Session calendar and machine-access budget

- Boundary blocked: Phases 14–16.
- Alternatives: distinct days; separated time blocks on fewer days; multiple machines.
- Scientific effect: session independence and external validity.
- Recommendation: one frozen machine with sessions on distinct days/time blocks and a preregistered replacement schedule; do not pool machines in the primary estimand.
- Evidence required: access calendar, maintenance/change policy, and minimum-valid-session contingency.
- Status: **AUTHOR DECISION REQUIRED**.

### DEC-014 — Correctness track on a development toolchain before the performance freeze

- Boundary blocked: Phases 1–7 and the correctness-only parts of Phases 8–11. These are currently blocked through DEC-001 and DEC-002 because Phase 1 requires a frozen compiler/runtime identity.
- Alternatives:
  - (a) Keep all implementation blocked until DEC-001–DEC-013 close (status quo).
  - (b) Permit correctness-track work on a recorded development toolchain. This covers the development host: Apple M1, macOS 14.1.1, Homebrew Clang 22.1.1, CMake 4.1.2, Ninja once installed, WABT 1.0.39 validator, and a development Wasm engine used only to execute modules for G2. No timed claim is made from these artifacts. Before any paper table is frozen, all artifacts are rebuilt with the frozen toolchain on the experimental machine and the full G1/G2 suites are re-run on the exact artifact hashes.
  - (c) Freeze the final toolchain now on the development host.
- Scientific effect:
  - (a) delays every result, including the machine-independent RQ1/RQ2 evidence.
  - (b) lets the source semantics, static analysis, BIR, lowering, datasets, and G1/G2 suites mature before the machine is chosen. It risks toolchain-dependent behaviour, which the mandatory frozen re-gate is designed to detect.
  - (c) would bind RQ3–RQ5 to a laptop without Linux `perf`, frequency control, or core isolation, which conflicts with paper §9.5.
- Recommendation: (b). Choosing it does not select the AOT runtime, compiler, machine, or floating flags for any timed artifact. DEC-001–DEC-013 remain open.
- Evidence required: an author statement approving (b); a development toolchain manifest emitted in Phase 1; the re-gate rule recorded in `PLAN.md`; and an audit showing no development-host timing in any derived paper output.
- Status: **APPROVED — option (b)**. Author approval given in a Claude Code session on 2026-10-04 ("approve"), in reply to the recommendation above. Track C is unblocked. DEC-001–DEC-013 remain open and continue to block Track P and every timed artifact.

### DEC-015 — Whether DEC-014's "recorded development toolchain" names one specific host or any exactly-recorded host

- Boundary blocked: whether Phase 1.1's Track C evidence (and any later Track C work done on a host other than the one DEC-014 names) counts as covered by `DEC-014`'s approval, pending this decision.
- Context: `DEC-014` option (b)'s approved text reads "This covers the development host: Apple M1, macOS 14.1.1, Homebrew Clang 22.1.1, CMake 4.1.2, Ninja once installed, WABT 1.0.39 validator...". Phase 1.1 was implemented and tested in a Claude Code session running on a categorically different machine: Linux x86_64 (Arch Linux), CMake 4.3.3, Ninja 1.13.2 (already present, not installed as part of the milestone, unlike STATUS.md's anticipated "Installing Ninja is part of this milestone"), and GCC 16.1.1 at `/usr/bin/c++` (not Clang at all, let alone Homebrew Clang 22.1.1). The session proceeded without stopping, reasoning informally that DEC-014(b)'s purpose is "permit correctness-track work on a recorded development toolchain" with Phase 1 being exactly where that toolchain gets recorded, not a restriction to one named physical machine; a `spec-auditor` review of the milestone disagreed and flagged this as a FAIL, since that reasoning was never logged or put to the author per the stop-and-ask protocol (`CLAUDE.md` §5), and version/host drift is Phase 1's explicitly named scientific risk (`PLAN.md`).
- Alternatives:
  - (a) Strict scope: `DEC-014`(b) covers only the named Apple M1/macOS/Homebrew Clang host. This session's Phase 1.1 evidence does not count toward `DEC-014` and must be reproduced on that exact host (or a newly and explicitly approved host) before any later milestone treats Phase 1 as satisfying its Track C gate.
  - (b) General scope: `DEC-014`(b) permits correctness-track work on *any* host whose exact toolchain identity is captured by the Phase 1 manifest emitter, not only the one named at approval time; the macOS/M1 description was the state observed when the recommendation was written, not an enumerated restriction.
  - (c) Additive scope: approve this Linux host as a second, explicitly named recorded development toolchain alongside the original macOS one (both individually valid, no open-ended "any host" authorization).
- Scientific effect:
  - (a) is the most conservative reading of what was literally approved, but makes Track C unable to proceed in any session that does not run on that exact laptop — which may be most or all Claude Code sessions — effectively re-blocking Phase 1 work pending host access.
  - (b) lets Track C proceed wherever a session happens to run, which may vary between sessions/days, at the cost of a broader authorization than the literal approved text; the mandatory frozen re-gate before any paper table already bounds the risk this creates, since no Track C development-host artifact is ever treated as a timed or paper-facing result regardless of which host produced it.
  - (c) is narrower than (b) (no open-ended future hosts) but requires a fresh decision-log entry and author approval every time the working host changes, which is likely to recur often in an agentic-session environment.
- Recommendation: (b), because Phase 1's acceptance criterion is literally "exact toolchain identity emitted" — the manifest is the mechanism that makes the recording exact regardless of which host produced it — and DEC-014's own stated purpose ("lets the source semantics, static analysis, BIR, lowering, datasets, and G1/G2 suites mature before the machine is chosen") reads as host-agnostic by design. The mandatory re-gate rule already means no development-host artifact, on any host, ever reaches a paper table without being rebuilt and re-gated on the frozen toolchain.
- Evidence required: an author statement choosing (a), (b), or (c). If (b) or (c): an explicit note added to `STATUS.md` that Track C evidence may span multiple development hosts, each exactly recorded via the Phase 1 toolchain manifest, with no change to the DEC-001–DEC-013 performance-track gates.
- Status: **APPROVED — option (b)**. Author approval given in a Claude Code session on 2026-10-05, in reply to the recommendation above (AskUserQuestion, "Any recorded host (Recommended)"). `DEC-014`(b) covers correctness-track work on any development host whose exact toolchain identity is captured by the Phase 1 manifest emitter, not only the host named at `DEC-014`'s approval. This Linux host's Phase 1.1 evidence counts as `DEC-014` Track C evidence. DEC-001–DEC-013 remain open and continue to block Track P and every timed artifact; the mandatory re-gate on the frozen toolchain before any paper table is unaffected.

## Frozen decisions

- `DEC-014` (2026-10-04): correctness-track work proceeds on the recorded development toolchain under the Track C and re-gate rules in `PLAN.md`.
- `DEC-015` (2026-10-05): `DEC-014`(b) covers any development host whose exact toolchain is recorded by the Phase 1 manifest emitter, not only the host named at `DEC-014`'s approval.

Implementation-level semantic decisions are frozen in `docs/spec-freeze/` and traced in `SPEC_AMENDMENTS.md`. In particular: C++23/CMake/Ninja intent; no Python in production components; strict scalar source numerics; K1/K2-only explicit SIMD; three non-pooled matrices; fixed export/ABI contract; four canonical BIR capability kinds; and distinct failure classes.
