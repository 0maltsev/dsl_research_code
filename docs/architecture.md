# Planned architecture

## Purpose

The architecture keeps semantic authority, implementation stages, evidence, and performance measurement separate. A later component may consume only a validated artifact and record from the preceding component. The reference evaluator must never invoke generated target code.

## Logical pipeline

```text
source bytes
  -> lexer/parser/name resolver
  -> typed AST + effect audit
  -> size/count certificates
  -> source cost/footprint summaries
  -> reference evaluator + dynamic trace
  -> proof-oriented normal form
  -> liveness/layout certificates
  -> BIR emitter -> BIR validator -> BIR evaluator
  -> scalar Wasm emitter -> standard validator/feature audit
  -> frozen AOT compiler -> trusted precompiled image
  -> ABI runner -> correctness and, only after gates, benchmark runner
```

The explicit-SIMD branch starts from validated BIR, applies only to K1/K2, emits separate transformation records, and never changes scalar source cost.

## Planned module ownership

| Planned path | Responsibility | Must not own |
|---|---|---|
| `src/source/lex` | UTF-8/token validation and source spans | Parsing or semantic recovery. |
| `src/source/parse` | Concrete grammar to untyped AST | Name, type, size, or cost decisions. |
| `src/source/resolve` | Symbols, alpha-renaming, declaration ranks | Type inference or implicit conversion. |
| `src/source/typecheck` | Exhaustive typing and effect rejection | Runtime execution. |
| `src/source/size` | Shapes, count refinements, finite certificate checking | Trusting an external solver. |
| `src/source/cost` | Static event/result/peak transformers | Wall-clock estimates. |
| `src/source/eval` | Independent rooted source semantics and exact dynamic trace | Calling BIR/Wasm/native code. |
| `src/normalize` | Administrative order-explicit form | Optimization or event-changing rewrites. |
| `src/layout` | Roots, death, colouring, frames, address certificates | Unchecked slot reuse. |
| `src/bir` | BIR data model, serialization, validation, evaluator | Source semantic invention. |
| `src/lower/source_to_bir` | One graph constructor per source form | BIR validation bypass. |
| `src/lower/bir_to_wasm` | Scalar structured Wasm and source maps | AOT compilation or runtime policy. |
| `src/lower/simd` | K1/K2-only explicit target variants | K3 SIMD or source SIMD events. |
| `src/abi` | Canonical codecs, region validation, status mapping | Kernel logic. |
| `src/runtime` | Frozen AOT image loading and stable call boundary | Hidden compilation in timed processes. |
| `src/kernels` | Frozen K1/K2/K3 source definitions and dataset contracts | Statistical analysis. |
| `src/native` | Semantically constrained C++ oracle/baseline | Serving as the source evaluator. |
| `src/correctness` | Conformance, generated, metamorphic, differential gates | Proof claims. |
| `src/benchmark` | Environment control and append-only samples | Data exclusion based on magnitude. |
| `analysis/` | Python-only frozen statistical analysis and plots | Production compiler/runtime code. |

Paths are planned interfaces, not existing implementation.

## Artifact boundaries

Every stage records input hashes, schema/spec versions, configuration identity, toolchain identity, status, diagnostics, and output hashes. At minimum, the following remain separately hashable:

1. source text and language manifest;
2. resolved and typed AST;
3. size/count certificate bundle;
4. cost/footprint certificate;
5. normalized core and source correspondence;
6. liveness and layout certificate;
7. BIR module and validation report;
8. Wasm module, feature report, and structured-control map;
9. AOT image and AOT configuration;
10. native artifact and flags;
11. dataset bytes and generator lineage;
12. correctness result records;
13. append-only benchmark rows;
14. deterministic derived tables and plots.

## Evidence layout

Generated evidence is kept apart from source and build trees:

| Path | Artifact class | Rule |
|---|---|---|
| `evidence/raw/` | Schema-valid generated records: correctness results, compiler certificates, benchmark samples, manifests. | Append-only. A correction is a new record that references the superseded one. |
| `evidence/derived/` | Tables, plots, and summaries regenerated deterministically from raw records by `analysis/`. | Reproducible from raw records plus script hashes. |
| `evidence/derived/paper/<version>/` | Manuscript hand-off package: table/figure fragments, `provenance.json`, `HANDOFF.md`. | A new version per regeneration; earlier versions are never modified. |

Large build artifacts (Wasm, AOT images, native binaries, disassembly) are identified by hash in records. Their storage location is fixed in Phase 1.

## Memory ownership

- ABI input and output are fixed, disjoint regions in non-growing Wasm32 memory.
- Source-local arrays map either to a validated frame slot or directly to caller-owned output, never both.
- Products are value structures; only arrays create source aggregate objects.
- A callee receives immutable input views and caller-owned aggregate destinations.
- A frame slot may be reused only after a certificate removes the old source object and capability from every live root. The no-reuse layout is mandatory fallback.
- Engine operand/call-stack memory outside linear memory is measured separately from source peak, ABI regions, BIR frames, and total linear memory.

## Failure boundaries

Lexical/parse/name/type/size/effect/cost errors reject before core execution. BIR and Wasm validation errors reject their artifacts. `Bounds`, `DivZero`, and `DivOverflow` are declared source outcomes. `InvalidABI`, BIR faults, Wasm traps, resource exhaustion, host/runtime errors, backend errors, OS termination, and hardware failure remain different categories. No layer may collapse them to a generic failure in generated evidence.

## Claim levels

| Level | Evidence | Permitted statement |
|---|---|---|
| Formal status | Paper theorem or explicitly open proof obligation | “proved in the paper” or “open”; never inferred from tests. |
| Functional artifact | Rule coverage, independent oracle, differential results | Counterexample/mismatch detected, none detected in the declared suite, or unresolved. |
| Static-bound artifact | Dynamic traces versus candidate bounds | Violation detected, none detected in the declared domain, or unresolved; PO-COST-SOUND remains open. |
| Machine performance | Frozen artifacts, machine controls, paired sessions, preserved raw rows | Estimate for the named kernel/configuration/machine only. |

