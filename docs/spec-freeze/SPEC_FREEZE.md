# BoundFin implementation specification freeze

Freeze identifier: `boundfin-spec-0.1.0`

Freeze date: 2026-08-17

Status: normative implementation specification complete; Phase 1 blocked by open scientific decisions.

## Authority order

1. The immutable paper snapshot under `spec/paper/` defines the research object, formal claims, and proof status.
2. This freeze document and the normative files listed below define the implementation contract where the paper is concrete.
3. `SPEC_AMENDMENTS.md` is the only authorized bridge where implementation detail clarifies or overrides an ambiguous paper passage.
4. `docs/decision-log.md` controls scientifically material choices not yet frozen.
5. Architecture, traceability, plan, and status documents are informative unless a normative document cites them.

No implementation may silently resolve a conflict. A new conflict increments the specification version, adds an amendment, and returns readiness to NO-GO until reviewed.

## Immutable source snapshot

| File | SHA-256 |
|---|---|
| `spec/paper/EXPERIMENT_IMPLEMENTATION_SPEC.md` | `76ef68820d7de6beae79feeee36b7bb73045324db1386858052c83e34b15475e` |
| `spec/paper/main.pdf` | `ac59cd8695488d71fa2e69dc17bcc1b9f3f6c561f08a902970ddd783df994dcd` |
| `spec/paper/notation.md` | `2eaca35ea9ab8aa0dc602cfc37618eb6e0b9c6ba683aa3b7f21bc58f15320b14` |
| `spec/paper/open-decisions.md` | `8d14e7c19fe0758716b1e4928fe0bbc06daf396636888fed765edf2695357799` |
| `spec/paper/proof-obligations.md` | `23ff80da0e60bcc5200817e1ab3e3fd3c03539c380cd9ba3571f9aebde381ec7` |

Any hash mismatch is a release-blocking integrity failure.

## Normative files

- `grammar.ebnf`: UTF-8 lexical rules, complete concrete grammar, surface elaboration, and precedence.
- `numeric-semantics.md`: value domains, primitive functions, exceptional arithmetic, floating behavior, and observations.
- `cost-trace-semantics.md`: exact dynamic trace datatype/composition, `peak_K`, event accounting, `Lambda`, and total footprint equations.
- `abi-layout.md`: type representation, `A(tau)`, field offsets, region trees, input/output records, export signature, status, aliasing, and host obligations.
- `bir-spec.md`: syntax, validation, capability state transitions, small-step machine, faults, and interpreter contract.
- `diagnostics-and-status.md`: stable diagnostic identifiers, source statuses, BIR faults, target/runtime failures, and record rules.
- `toolchain-feasibility.md`: required AOT feasibility tests; it does not freeze a runtime version.
- `experiment-matrix.md`: kernel/build cells and capacity estimates; numeric sample scenarios are planning estimates until decisions close.
- `SPEC_AMENDMENTS.md`: source-linked resolutions and propagation requirements.
- `schemas/*.schema.json`: versioned generated-record contracts.

## Frozen language boundary

- Modules are ordered, closed, first-order declarations with exactly one export.
- Types are `bool`, `i32`, `i64`, `f64`, products of arity at least two, and immutable literal-capacity arrays.
- Capacities are decimal natural literals in `[0, 2^31-1]`; K1 additionally requires `D <= 2^31-2` where `D+1` occurs.
- Calls target strictly earlier declarations. There is no recursion, higher-order value, implicit conversion, mutation, general allocation, external effect, or unbounded control.
- Array literals evaluate distinct element expressions once. Builders repeat one indexed body. Folds repeat one body with an invariant accumulator. All evaluation is strict left-to-right with first-error propagation.
- Runtime counts are admitted only by the finite checked count fragment. An external solver is untrusted.
- Surface operators elaborate one-for-one to the primitive table before type, size, or cost analysis. Elaboration may not reorder or simplify.
- Comments and decimal floating literals are not in version 0.1.0.

## Frozen semantic boundary

- Integer negation/addition/subtraction/multiplication are modular fixed-width operations.
- Signed division/remainder produce `DivZero` or `DivOverflow`; they do not trap at source level.
- Floating arithmetic is strict scalar Wasm binary64, round-to-nearest ties-to-even, followed at every primitive result by source NaN canonicalization to `0x7ff8000000000000`.
- External comparison is bitwise for non-NaNs, including signed zero, and quotients all NaN encodings.
- Dynamic source cost is derived from the exact trace, not reconstructed by componentwise maxima of isolated premise vectors.
- The exported entry starts at active depth one without an internal call event. Only explicit DSL calls emit enter/return events and lift depth.
- Local array liveness is reachability from declared continuation and active roots; failures retain earlier events but may kill unreachable partial objects.
- Static analyzer output is a finite candidate bound while PO-COST-SOUND remains open.

## Frozen ABI and BIR boundary

- The ABI is little-endian, Wasm32, non-growing memory with exact-width scalar words, canonical record headers, deterministic product/array layout, absolute 32-bit offsets, disjoint input/output/frame/reserve regions, and caller-owned deep aggregate output.
- Exported names are `memory` and `boundfin_entry`; the function type is `(i32 input_offset, i32 output_offset) -> i32 status`.
- Status values are fixed in `diagnostics-and-status.md`; source errors are status returns, never Wasm traps.
- BIR has exactly four canonical capability kinds: `inputRead`, `freshInit`, `scratchInit`, and `outputWrite`. Permission phase is state within a kind, not an additional kind.
- BIR faults use the ordered precedence `badControl`, `badType`, `badCap`, `uninit`, `oob`, `stackExhaust`.
- Validated scalar Wasm has no imports, start function, mutable global, table, indirect call, reference type, memory growth, shared memory, thread/atomic, exception, callback, or relaxed/SIMD operation. SIMD uses separate K1/K2 artifacts.

## Proof and evidence status

The paper proves certificate soundness, source termination, source determinism modulo rooted fresh-name equivalence, alias overcount, declared-root closure, and finite capacity instantiation at its stated boundaries. Type/size preservation, size substitution through semantic calls, liveness composition, scalar resource soundness, source-to-BIR construction/validation, memory/address preservation, BIR-to-Wasm simulation, target-fault exclusion, end-to-end preservation, target-cost envelopes, and SIMD equivalence remain open obligations.

Testing may report only a counterexample/mismatch/fault, none detected in the declared domain, or unresolved. It never changes this proof ledger.

## Freeze checklist

| Area | Implementation semantics | Scientific configuration |
|---|---|---|
| Grammar and typing | Frozen | Not applicable |
| Numeric behavior | Frozen | Concrete compiler flags open |
| Dynamic/static cost contract | Frozen | Proof obligations open by design |
| ABI/layout | Frozen | Machine-specific total memory open |
| BIR machine/capabilities | Frozen | Not applicable |
| Diagnostics/status | Frozen | Not applicable |
| Record schemas | Frozen at schema version `1.0.0` | Tool versions/identities open |
| AOT feasibility protocol | Frozen | Runtime release open |
| Experiment cells | Frozen | Replication, anchors, machine, timer, seeds, band open |

Because the open scientific choices can change benchmark claims, the repository remains NO-GO for Phase 1 under the user's Phase 0 readiness rule.

