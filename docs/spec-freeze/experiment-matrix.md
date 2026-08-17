# Experiment matrix and capacity plan

Normative design version: `boundfin-experiment-0.1.0`

The cells and formulas below are frozen. Sample-count/runtime/storage numbers are planning scenarios, not chosen confirmatory values and not benchmark results. Final replication and p99.9 anchors remain author decisions selected under the claims-excluded pilot.

## Workload cells

| Kernel | Size levels | Other crossed factors | Workload cells |
|---|---:|---|---:|
| K1 bounded binomial valuation | 7 depths: `8,16,32,64,128,256,512` | 3 moneyness strata | `7*3 = 21` |
| K2 portfolio mark-to-market | 6 lengths: `16,64,256,1024,4096,16384` | 3 dependence strata: independent/concordant/discordant | `6*3 = 18` |
| K3 admission/fee aggregation | same 6 lengths | 3 admission proportions times 3 sequence patterns | `6*3*3 = 54` |
| Total | | | `93` |

K3 patterns are hash-ranked uniform, maximum-transition, and clustered. K3 admission proportions are `0.10,0.50,0.90`. Kernel labels are motivations, not statistical workload-class factors.

## Optimized treatment matrix

| Family | Kernels | Execution forms | Workload cells | Execution conditions |
|---|---|---:|---:|---:|
| MS | K1/K2/K3 | Wasm and native | 93 | 186 |
| PO | K1/K2/K3 | Wasm and native | 93 | 186 |
| Explicit SIMD | K1/K2 only | Wasm and native | `21+18=39` | 78 |
| Total | | | | `450` |

The 225 paired contrast cells are `93 MS + 93 PO + 39 SIMD`. K3 has no explicit-SIMD cell. D0–D5, counters, compilation, instantiation, and first-call measurements are separate metrics/phases and are not extra primary treatments.

## Build artifacts

Capacities are maximum per kernel, so runtime size/stratum/pattern normally changes data, not executable code.

- Scalar treatments: `3 kernels * 2 build families * 2 execution forms = 12` executable treatment artifacts.
- Explicit SIMD treatments: `2 kernels * 1 family * 2 execution forms = 4`.
- Total optimized executable treatments: `16`.
- Physical primary build files: 8 Wasm modules, 8 precompiled AOT images, and 8 native libraries/executables = `24`, because each of 8 Wasm treatments has both `.wasm` and `.cwasm` while each matching native treatment has one executable artifact.

Validation reports, compiler certificates, normalized disassemblies, raw disassemblies, symbol/source maps, manifests, and checksums are evidence artifacts and add at least one separately hashable file per treatment/stage. Debug, sanitizer, source-instrumented, BIR-evaluator, and reference artifacts are correctness-only and are not counted among the 16 timed treatments.

If the implementation instead specializes code by runtime size or stratum, artifact counts and the scientific matrix change. That requires an amendment and new decision; it is not the default.

## Pilot matrix

The pilot uses smallest/middle/largest sizes:

- K1: `3 sizes * 3 strata = 9` cells.
- K2: `3 sizes * 3 strata = 9` cells.
- K3: `3 sizes * 3 proportions * 3 patterns = 27` cells.
- Base workload cells: 45.
- MS/PO Wasm/native execution conditions: `45*2*2 = 180`.
- Optional-but-recommended SIMD feasibility conditions for selected K1/K2 cells: `(9+9)*2 = 36`.
- Full pilot execution conditions: 216.

At five sessions, four fresh pairs/session, and initial `L0=4096` valid ordered observations per condition, the primary-row capacity is:

```text
216 * 5 * 4 * 4096 = 17,694,720 observations.
```

If every affected run required the one permitted extension to total `L1=16384`, the upper capacity would be 70,778,880 observations. This is a storage bound, not a prediction. Warm-up, calibration, invalid records, and D0–D5 rows are additional.

## Confirmatory sample formula

Let:

- `C=450` execution conditions;
- `S` sessions;
- `P` paired processes per session;
- `R` scheduled runs per condition/process;
- `L` valid primary observations per run.

Then primary rows are:

```text
N_primary = C*S*P*R*L.
```

Two capacity scenarios with `R=1` and `L=4096` illustrate scale only:

| Scenario | `S` | `P` | Primary rows |
|---|---:|---:|---:|
| Paper minimum | 10 | 4 | 73,728,000 |
| Candidate-grid maximum | 30 | 10 | 552,960,000 |

The pilot must choose `(S,P)` by the frozen precision simulation. `R` and `L` must then support the selected moving-block rule, timer calibration, and schedule; they are not silently fixed by this table. Invalid rows remain stored and increase physical row count.

## Runtime capacity

For average measured per-invocation time `t_bar`, raw primary compute time is `N_primary*t_bar`. The following arithmetic is a planning sensitivity, not observed performance:

| Rows | At 1 microsecond | At 10 microseconds | At 100 microseconds |
|---:|---:|---:|---:|
| 73,728,000 | 1.2 min | 12.3 min | 2.05 h |
| 552,960,000 | 9.2 min | 1.54 h | 15.36 h |

Actual wall time also includes at least 10,000 warm-up calls per scheduled condition, process launches, pair counterbalancing, session separation, calibration, D0–D5, counters, checksum consumption, invalid/replacement runs, and slow high-depth K1. The experimental freeze must budget from pilot timings. A provisional operational reservation of multiple dedicated days is prudent but is not a frozen sample or performance claim.

K1 work grows quadratically with depth; K2/K3 grow linearly with records. The maximum K1 depth is therefore the largest runtime risk even though K2/K3 have larger element counts.

## Storage capacity

Storage depends on the final encoding and metadata dictionary strategy. Retaining a fully expanded JSON record conforming to `benchmark-sample.schema.json` is estimated for capacity planning at 0.75–1.5 KiB/row; a lossless columnar/dictionary encoding may be roughly 0.15–0.4 KiB/row. These are engineering sizing assumptions to be measured in the pilot, not data.

| Scenario | Expanded JSON capacity | Compact lossless capacity |
|---|---:|---:|
| 73,728,000 rows | about 53–106 GiB | about 11–28 GiB |
| 552,960,000 rows | about 396–791 GiB | about 79–211 GiB |

Add warm-up, invalid, diagnostic/counter rows, artifacts, disassemblies, datasets, checksums, and at least one read-only backup. The experimental freeze records measured bytes/row and a two-copy storage budget before execution. Compression must not remove row identity, raw ticks, ordering, validity evidence, or configuration lineage.

## p99.9 feasibility

p99.9 is eligible per exact condition only with:

- at least 1,000,000 valid invocations;
- at least 1,000 nominal upper-tail observations;
- at least 200 tail-containing dependence blocks;
- at least ten sessions; and
- stable finite block-bootstrap intervals under base/half/double block sensitivity.

Applying one million to all 450 execution conditions would require at least 450 million valid tail-focused invocations before warm-up/invalid/diagnostic rows and would multiply storage and session scheduling. The design does not require this.

Realistic candidate families, subject to pilot evidence, are:

- K1 depths up to 64; larger depths have quadratic work and should not receive a million-call commitment without measured feasibility.
- K2 and K3 lengths up to 4096; length 16384 remains possible only if pilot wall time and block support are acceptable.
- One representative predeclared stratum/pattern per kernel rather than every cross-product, unless the tail claim explicitly concerns those factors.

A concrete planning candidate is one K1 `n=64` at-the-money cell, one K2 `n=1024` independent cell, and one K3 `n=1024`, 50% admission, uniform-pattern cell across MS/PO Wasm/native (`3*4=12` conditions), plus matching K1/K2 explicit-SIMD Wasm/native (`2*2=4`), for 16 million valid invocations minimum. This is a recommendation for `DEC-012`, not a frozen choice.

If any eligibility condition fails, omit p99.9 for that condition and state the failed criterion. p50/p95/p99 still require dependence-aware intervals.

## Randomization and hierarchy

The hierarchy is session -> paired process -> counterbalanced block -> run -> batch -> invocation -> stored sample. Levels are not interchangeable replications. Wasm/native order is randomized and counterbalanced within each build family; kernel/size/stratum/pattern blocks follow a preserved schedule. PRNG/seed choices remain `DEC-008`.

## Gates and stop conditions

- No primary/counter timing before G1/G2 pass for the exact scalar artifacts.
- SIMD timing additionally requires its variant oracle and intended disassembly.
- Fewer than eight valid sessions makes session-level inference unresolved; fewer than ten forbids the primary bootstrap classification.
- No arbitrary block-length fallback, silent family reduction, sample-count change after comparative unblinding, or magnitude-based exclusion.
- Pilot rows are permanently claims-excluded.

