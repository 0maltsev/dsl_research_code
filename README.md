# BoundFin research artifact

This repository is the planned reference implementation and experimental artifact for “A Static Cost Model and AOT Compilation of a Restricted DSL for Predictable Execution of Stateless Financial Contracts.” It currently contains the Phase 0 specification freeze only. There is no lexer, parser, evaluator, compiler, BIR implementation, Wasm backend, financial kernel, native baseline, or benchmark harness.

## Current readiness

Phase 0 has frozen implementation-level language, numeric, trace, footprint, ABI, BIR, diagnostics, schema, and experiment contracts. Phase 1 is currently **NO-GO** because scientifically material toolchain and experiment choices still require author decisions. See [PHASE0_READINESS_REPORT.md](PHASE0_READINESS_REPORT.md) and [docs/decision-log.md](docs/decision-log.md).

## Specification map

- `spec/paper/`: immutable manuscript snapshot and hand-off documents.
- `docs/spec-freeze/SPEC_FREEZE.md`: authority, versions, scope, and freeze checklist.
- `docs/spec-freeze/grammar.ebnf`: mechanically parsable concrete grammar.
- `docs/spec-freeze/numeric-semantics.md`: scalar values and primitive behavior.
- `docs/spec-freeze/cost-trace-semantics.md`: dynamic traces, peak memory, and total footprint environment.
- `docs/spec-freeze/abi-layout.md`: source/Wasm ABI, representation, layout, status, and host obligations.
- `docs/spec-freeze/bir-spec.md`: executable BIR contract and capability state machine.
- `docs/spec-freeze/diagnostics-and-status.md`: stable diagnostic and runtime identifiers.
- `docs/spec-freeze/toolchain-feasibility.md`: AOT feasibility evidence and pilot gates.
- `docs/spec-freeze/experiment-matrix.md`: workload/build matrix and capacity estimates.
- `docs/spec-freeze/SPEC_AMENDMENTS.md`: implementation clarifications relative to the paper snapshot.
- `docs/architecture.md`: planned component and artifact boundaries.
- `docs/traceability.md`: paper-to-module-to-test-to-evidence-to-claim map.
- `docs/decision-log.md`: unresolved scientific choices and frozen decisions.
- `PLAN.md` and `STATUS.md`: phased execution plan and current progress.
- `schemas/`: Draft 2020-12 schemas for manifests, certificates, correctness results, and benchmark samples.

## Authority

The paper snapshot remains the scientific source. For implementation, `docs/spec-freeze/SPEC_FREEZE.md` and the documents it makes normative resolve concrete gaps. A divergence is valid only when it is listed in `SPEC_AMENDMENTS.md`. Neither specification nor testing closes an open proof obligation.

## Phase 0 validation

Run these read-only checks from the repository root:

```sh
jq empty schemas/*.json
python3 -m json.tool schemas/experiment-manifest.schema.json >/dev/null
shasum -a 256 spec/paper/*
```

The final command must match the immutable hashes recorded in `docs/spec-freeze/SPEC_FREEZE.md`. A Draft 2020-12 schema checker should also be used when one is available; JSON parsing alone does not prove schema conformance.

## Claim discipline

“Accepted by tests” means only that no counterexample was detected in the declared domain. It does not mean the static cost model is proved sound or the compiler is proved correct. Performance language is permitted only for frozen artifacts on the designated experimental machine after the correctness gates pass. Development-host timings must be labelled smoke measurements.

