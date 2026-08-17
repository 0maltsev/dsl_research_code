# AOT-Wasm toolchain feasibility plan

Review date: 2026-08-17

Status: candidate feasible on documentation evidence; no runtime/backend/version is frozen; execution pilot not run.

## Candidate and evidence boundary

Wasmtime/Cranelift is the primary candidate, not a selected dependency. Current official Wasmtime documentation states that `wasmtime compile` produces a precompiled `.cwasm` artifact, that a runtime build can execute precompiled modules without the Cranelift/Winch compiler features, and that compilation and execution configurations must match. It also warns that deserializing precompiled modules is an unsafe trusted-artifact boundary. The official C/C++ API provides a stable embedding route, while the CLI documentation labels direct `--invoke` syntax unstable. Official tooling includes `.cwasm` `objdump`/address maps and Linux `perfmap`/`jitdump` profiling support.

Primary sources consulted:

- <https://docs.wasmtime.dev/examples-pre-compiling-wasm.html>
- <https://docs.wasmtime.dev/cli-options.html>
- <https://docs.wasmtime.dev/c-api/>
- <https://docs.wasmtime.dev/examples-profiling-perf.html>
- <https://docs.wasmtime.dev/api/wasmtime/struct.Config.html>

These pages establish candidate capabilities, not their suitability for the future frozen version, machine, flags, or experiment. Every capability below requires an artifact-specific pilot.

## Development-host inventory

This inventory is a smoke observation, not paper evidence and not a version choice:

| Tool | Observed development-host state |
|---|---|
| CMake | `4.1.2` available. |
| Ninja | unavailable. |
| Homebrew Clang | `22.1.1` available. |
| Apple Clang | `15.0.0` available as `c++`/`g++`. |
| Wasmtime | unavailable. |
| `wasm-tools` | unavailable. |
| `wasm-validate` | `1.0.39` available. |

Therefore this host cannot execute the feasibility pilot today, and its macOS environment cannot establish the required Linux `perf`/frequency controls. No dependency is installed during Phase 0.

## Required architecture

Use two separately hashable Wasmtime roles:

1. an offline compiler process/build containing Cranelift, which validates `.wasm` and produces `.cwasm`; and
2. a timed runner linked to a runtime-only Wasmtime build with Cranelift and Winch compiler features absent, loading only trusted, pre-hashed `.cwasm` bytes.

The timed runner uses the C API/C++ wrapper and obtains one typed `boundfin_entry(i32,i32)->i32` handle during setup. CLI invocation is not the primary call boundary. Instantiation and first call are separate metrics and happen before steady-state observations.

Compiler and runner configurations are canonical serialized manifests and must match byte-for-byte in every Wasmtime semantic/codegen field that affects precompiled compatibility. The runner accepts only an artifact hash listed in the experiment manifest.

## Feasibility gates

### F-AOT-01 — True precompiled artifact

- Build a minimal closed scalar module and a representative K1/K2/K3 module.
- Produce `.cwasm` in a separate compiler process with the exact candidate release/configuration.
- Record source Wasm hash, `.cwasm` hash, compiler executable/library hashes, full configuration, target, and output size.
- Compile twice in clean directories with normalized environment. Compare `.cwasm` bytes and machine-code sections. If nondeterministic, identify and freeze the nondeterministic metadata or reject the candidate.
- Pass: a native-code-bearing precompiled artifact exists before runner startup and is reproducibly attributable to its inputs.

### F-AOT-02 — No hidden per-run compilation or tiering

- Build/link a runner without Cranelift and Winch compiler features; retain link map and symbol audit.
- Make the runner reject `.wasm` and accept only trusted `.cwasm`.
- Trace process file access, executable-memory mappings, threads, and startup CPU activity during deserialize/instantiate/first/steady calls.
- Disable and audit runtime code cache paths; use a clean cache environment even though a compiler-free runner should not compile.
- Compare first call and later calls but do not infer absence of compilation from timing alone.
- Pass: the timed process contains no compiler backend, consumes no source Wasm, creates no new guest-code artifact, and exhibits no tier-up/recompilation event.

### F-SCALAR-01 — Source feature gate

- Configure the scalar engine with Wasm SIMD and relaxed SIMD disabled and all other forbidden proposals disabled where configurable.
- Independently audit the module sections/opcodes and reject `v128`/SIMD, imports, tables, indirect calls, references, globals, memory growth, threads/atomics, exceptions, and start functions.
- Pass: both standard validator and independent feature audit accept the exact scalar subset.

### F-SCALAR-02 — Machine-code scalar profile

- Query and preserve candidate Cranelift settings.
- Determine whether stable flags can disable loop/SLP vectorization without disabling required scalar floating instructions.
- Audit disassembly for every kernel/size-independent artifact. Scalar SSE/AVX instructions such as `addsd` are permitted on x86-64; packed/lane instructions operating on multiple contract elements are not.
- Check no target transform removes required status/bounds behavior or changes floating order.
- Pass: MS has no packed-vector kernel transformation and strict semantics; otherwise the candidate/version cannot supply the main-study MS profile.

### F-PO-01 — Permitted optimization profile

- Freeze a separate normal production O3/LTO/target-feature manifest while retaining strict floating semantics and source checks.
- Capture optimization/vectorization reports when available and disassembly for observed transformations.
- Never pool PO with MS or explicit SIMD.
- Pass: PO is reproducible, semantically conforming, and distinguishable by configuration/artifact identity.

### F-DISASM-01 — Reproducible machine-code capture

- Run the candidate version's `.cwasm` object dump with bytes, relative addresses, and Wasm address map.
- Save raw `.cwasm`, canonical dump, tool hash/version, and an independent native object/disassembler view if supported.
- Normalize only explicitly nonsemantic absolute addresses/paths; retain both raw and normalized reports.
- Pass: function boundaries and instruction bytes are stable enough to audit MS/PO/SIMD and map `boundfin_entry`/kernel bodies.

### F-PERF-01 — Counter attribution

- On the designated Linux host, test `perf stat` for nonmultiplexed cycles, instructions, branches, branch misses, and named cache events on the exact optimized timing artifact.
- Test Wasmtime `perfmap` and `jitdump` (despite the naming) with `.cwasm`, and verify symbols/source maps attribute samples to the precompiled guest and host trampoline.
- Preserve event encodings, kernel/perf versions, permissions, sampling loss, time-enabled/time-running, and generated maps.
- Counter collection uses separate runs with the same executable/artifact; it is not inserted into primary timing.
- Pass: attribution distinguishes host wrapper, Wasmtime boundary, and guest kernel with no unsupported multiplexing.

### F-CALL-01 — Stable native-call boundary

- Use one C/C++ embedding API path, one instantiated typed export handle, fixed memory, and the ABI in `abi-layout.md`.
- Compare repeated direct typed calls while preventing whole-call elimination through symmetric checksum consumption.
- Record D1 no-work export, D2 ABI identity body, D3 kernel interval if supportable without changing the primary artifact, and D4 result/status handling.
- Inspect wrapper/trampoline disassembly and verify the same logical byte-buffer/status boundary in native C++.
- Pass: the call path does not change across invocations/configurations and its components can be measured or explicitly marked unavailable.

### F-NUM-01 — Numeric conformity

- Exhaustively cover feasible Boolean and exceptional integer classes and a stratified binary64 word corpus including all zero/infinity/NaN/signaling/subnormal boundaries.
- Confirm guarded division/remainder, no unexpected traps, strict operation order, and target NaN quotient.
- Pass: no source/reference/BIR/Wasm/native mismatch for the candidate flags.

### F-MEM-01 — Fixed memory and output ownership

- Confirm one non-growing memory with exact pages, no imported memory, canonical bases, all endpoint checks, and deep aggregate output.
- Exercise maximum legal endpoints and one-past/overflow/overlap negatives.
- Pass: valid calls do not grow/replace memory and invalid layouts return `InvalidABI` without target trap.

### F-REPRO-01 — Clean-host replay

- Rebuild and execute on a freshly provisioned machine matching the frozen target and tool hashes.
- Regenerate Wasm/AOT/native hashes, validation reports, disassembly, one correctness corpus, and one claims-excluded smoke pair.
- Pass: logical artifacts/evidence regenerate as specified; machine-specific addresses may differ only where declared.

## Configuration records

Each candidate run records release/tag/commit, source/archive SHA-256, build features, Rust toolchain if built from source, C API header/library hashes, Cranelift strategy/settings, target triple/CPU/features, Wasm feature flags, optimization level, NaN option, memory knobs, profiler strategy, cache configuration, debug-info policy, runner link mode, and every environment variable consumed.

## Decision rule

Wasmtime/Cranelift may be frozen only if all gates pass on the designated machine for one exact release and the authors close `DEC-001`, `DEC-003`, and `DEC-004`. A failure is not worked around by silently changing the ABI, MS meaning, counter scope, or invocation boundary. Alternatives are evaluated with the same gates and recorded as new configurations.

