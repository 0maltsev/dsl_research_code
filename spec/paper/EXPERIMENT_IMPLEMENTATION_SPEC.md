# Experiment implementation specification

## Purpose and authority

This document is the hand-off contract for a later implementation of the paper “BoundFin: Static Multi-Resource Cost Semantics and an AOT-Wasm Translation Specification for Stateless Financial Kernels.” It specifies interfaces, invariants, evidence, workloads, and acceptance rules. It contains no production source code and does not claim that an implementation, proof, or experiment exists.

The manuscript's source-calculus and BIR/lowering appendices are normative. A later implementation must not fill an ambiguity by silently choosing semantics. Any amendment to a primitive, size rule, memory layout, comparison family, or statistical decision must update both documents before comparative data are observed.

## 1. Scope and deliverables

### 1.1 Required prototype capabilities

The prototype must support exactly the versioned scalar core:

- `bool`, `i32`, `i64`, and `f64` bit values and finite products;
- immutable `arr<tau,N>` values with a runtime length and literal capacity;
- variables, exact bit literals, the complete primitive table, `let`, conditionals, products/projections, array literals with distinct ordered element expressions, length, checked indexing, and direct rank-decreasing calls;
- `fold_N(n;i,x;e0;eb)`, executing exactly runtime count `n` after a static proof `0 <= n <= N`;
- `build_N(n;i.eb)`, producing a fresh immutable array of length `n` and capacity `N` by one ordered body evaluation per index;
- exact, symbolic-upper, and capacity-only size summaries for every aggregate-producing expression and function call;
- rooted reference semantics with immutable stores, reachability, object death, distinct partial literal/builder constructions, strict first-error propagation, and store equivalence modulo unreachable objects;
- scalar event inference for operations, allocation events, cumulative reserved bytes, semantic reads/writes, decisions, and call depth, plus a separate peak-live-scratch summary;
- typed BIR generation/validation, liveness certificates, frame layout, capability-restricted writes, scalar Wasm lowering/validation, and frozen AOT artifact production;
- matched-scalar and production-optimised Wasm/native configurations, plus a non-pooled K1/K2-only explicit-SIMD ablation;
- conformance, generated, metamorphic, differential, footprint, ABI, and benchmark evidence; and
- immutable manifests, raw data, counters, disassemblies, and analysis lineage.

The source cost model is scalar. SIMD events belong only to target/lowering records.

### 1.2 Excluded features

Reject recursion, unrestricted loops, general mutable arrays, persistent/global state, dynamic allocation beyond declared immutable construction, variable capacities, closures/function values, dynamic dispatch, exceptions/handlers, imports/callbacks, I/O, time, randomness, threads/shared memory/atomics, Wasm memory growth, strings/maps/recursive types, implicit conversions, transcendental functions, relaxed floating operations, and unmodelled primitives. Partial builder mutation is compiler-private initialization and is never source-visible general mutation.

### 1.3 Logical interfaces

Names may change once before preregistration; responsibilities may not be merged so that evidence becomes unauditable.

| Interface | Input | Required output | Completion criterion |
|---|---|---|---|
| Checker | source and language manifest | accepted typed core or stable diagnostics | complete positive/negative conformance by rule/category |
| Size analyzer | typed core and formal input lengths | exact/upper/capacity shapes and proof constraints | every array/product/call result has a checked shape |
| Reference evaluator | typed module, entry, ABI bytes | result/error, live-store trace, scalar event trace | deterministic replay modulo fresh identifiers/dead garbage |
| Cost/footprint analyzer | typed core, shapes, variable footprints | parametric event, result-footprint, peak-footprint summaries | every rule cites a normative appendix row |
| Normalizer/layout planner | checked core and summaries | administratively explicit core, live intervals, colored frame, certificates | value/error/event trace and first-error order retained; frame dominates peak |
| BIR emitter/validator | normalized core/layout | BIR, CFG/source maps, validation report | all typing/control/capability/liveness invariants pass |
| Scalar Wasm lowerer | validated BIR and scalar profile | Wasm, lowering map, feature/import report | standard validation and forbidden-feature audit pass |
| SIMD lowerer | validated BIR and explicit profile | separate Wasm and transformation record | lane/mask/remainder/reduction obligations complete |
| AOT wrapper | Wasm and frozen runtime manifest | native artifact and metadata | no timed dynamic compilation/tiering |
| Native baseline builder | frozen kernel and MS/PO/SIMD profile | native artifact and reports | semantic suite passes; flags match named matrix |
| Correctness runner | corpus/generator manifest | case outcomes, coverage, retained failures | all mandatory tests and gates reported |
| Benchmark runner | immutable schedule/configuration | append-only raw records and run manifest | pairing, environment, checksums, decomposition fields complete |
| Analysis driver | raw data and frozen plan | derived tables/plots/decision ledger | deterministic regeneration with raw-row lineage |

All interfaces emit structured versioned records and distinct failure statuses. Human-readable output is not the sole source for any paper claim.

### 1.4 Boundaries and completion

Separate source text, typed core, reference semantics, size/cost analysis, BIR/validation, scalar/SIMD lowering, ABI/runtime integration, native baselines, datasets, correctness tests, benchmark control, analysis, immutable configuration, generated artifacts, raw data, and derived outputs. The reference evaluator must not call generated target execution.

Component completion requires rule-complete conformance, deterministic serialization, negative validator tests, standard Wasm validation, exact ABI round trips, differential evidence, retained counterexamples, environment failure-injection tests, and known-answer analysis data. “No counterexample detected” is a gate outcome, not completion of a formal proof.

## 2. DSL conformance requirements

### 2.1 Lexical and grammar contract

Record a language version. Source is UTF-8; identifiers are ASCII letter/underscore followed by ASCII letters/digits/underscore. Keywords and primitive names are reserved and case-sensitive. Exact `i32`, `i64`, and `f64` bit literals are mandatory. Decimal binary64 syntax is optional only after a correctly rounded conversion is specified and tested.

Concrete syntax must preserve all distinctions in the mathematical grammar: capacities are compile-time literals; array literals contain actual elements and a capacity; fold/build contain separate runtime count and capacity; fold names index and accumulator; builder names one index; declarations have explicit parameter/result types. Surface sugar must elaborate before type/size/cost analysis and have its own conformance cases.

### 2.2 Primitive and numeric requirements

Implement only the appendix table: strict Boolean operations/equality; modular integer negation/add/subtract/multiply; checked signed division/remainder; signed/equality comparisons; binary64 negation/absolute/add/subtract/multiply/divide; and binary64 comparisons. Every primitive has one typed total semantic function and one source event entry. There are no implicit conversions, shifts, source SIMD operations, minimum/maximum, square root, fused multiply-add, `pow`, or `libm` calls.

Integer division yields `DivZero` or `DivOverflow` exactly as specified. Let the canonical source NaN word be `0x7ff8000000000000`. Every unary or binary floating primitive first takes the strict scalar Wasm result and then maps every NaN encoding to that word; non-NaN bits are unchanged. Comparisons use the scalar Wasm Boolean result. Source literals/ABI inputs may contain any NaN word, but subsequent source primitive results are canonical. External observations compare all non-NaN bits, including signed zero, and quotient all NaN encodings. The reference evaluator and C++ oracle must canonicalize at every primitive boundary; target Wasm may produce any permitted NaN and is compared through the quotient. Performance data are finite; correctness data cover every exceptional class.

### 2.3 Type, count, and size checks

The checker must enforce primitive signatures, equal branch types, product arity/projection, homogeneous literals, finite capacity, count proof, accumulator invariance, internal index facts, call order `f prec_M g`, nested representation limits, and ABI subset.

Size analysis must expose:

- fresh exact formal lengths for ABI arrays;
- exact literal length;
- exact/upper builder length, accepting an upper-only count only when its named runtime value is proved nonnegative and no greater than the capacity;
- branch exactness only when expressions agree and otherwise a maximum upper length;
- substituted shapes through `let`;
- component shapes for products/projections;
- accumulator shape recurrences for folds;
- parameterized function result/cost/footprint summaries;
- simultaneous actual substitution for nested calls;
- a conservative joined element-shape summary for every array, including arrays of aggregate values; and
- explicit capacity fallback when exact/upper facts are unavailable.

An array shape has the abstract form `array(lambda,u,N;element_shape)`: `lambda` is an exact expression or unavailable, `u` is a proved length upper bound, `N` is capacity, and `element_shape` bounds every logical element. Count analysis additionally returns a logical name for the runtime result. A body may assume `0 <= i < runtime_count <= u <= N` without falsely labelling an upper-only count exact.

The accepted size term grammar is natural constants, logical size names, addition, and multiplication by a natural constant; upper terms additionally contain `max`. Constraints are finite conjunctions of term equalities/inequalities. Count rules exist only for nonnegative constants, ABI lengths, `len`, bound indices, conditionals, checked addition/subtraction, and earlier-call summaries. Addition/subtraction needs an explicit no-wrap certificate establishing its mathematical result in `[0,2^31-1]`. Exactness is unavailable when conditional branches disagree or an exact actual is absent; the rule then records `star` and a certified upper term. Resource expressions may add capacity-bounded sums and recurrences, each with a literal finite upper certificate.

Certificate generation may use any recorded solver, but acceptance depends only on checking a finite derivation built from assumption, closed arithmetic, equality substitution, reflexivity/transitivity, addition, nonnegative scaling, and `max` rules. Certificate checking must terminate structurally. If no accepted certificate is supplied, reject even if an external solver reports satisfiable/valid. Modular arithmetic cannot justify a count proof. Reject `N > 2^31-1`; reject `n+1` unless the stronger bound is certified.

### 2.4 Reachability, liveness, and peak memory

The runtime model tags ABI and local objects and records immutable object graphs. Define reachability transitively through arrays and products. Store comparison and garbage reduction consider continuation, environment, accumulator, partial literal/builder construction, and result roots. An object dies only when absent from all such roots.

Instrumentation must record:

- allocation-event identity and region capacity;
- cumulative reserved bytes, including dead/failed construction;
- the reachable local-object set at each semantic event;
- peak simultaneously live local bytes excluding ABI input;
- object-death event and root reason;
- literal/builder prefix and seal state; and
- alias/reference edges needed to avoid double-counting dynamic peak.

The analyzer emits event summary `E`, successful result footprint `L`, and transient peak `P`. It must implement sequence and literal ordered-prefix rules, branch maximum, builder partial-footprint, fold accumulator-footprint, call substitution/depth, and early-error recurrences. A literal evaluates each distinct element expression once; a builder repeats one indexed body. It rejects when a retained-root transformer is unavailable. The target layout partitions each live local source object at every point into frame-mapped or caller-output-mapped storage. Instrument and report `P_frame`, `P_output`, and `max_t(frame_t+output_t)`; an aggregate built directly in output is not also charged to a callee frame. ABI input, ABI output (including internal padding), aligned active frames, runtime reserve, engine operand stack, and total linear memory remain separate quantities.

### 2.5 Deterministic evaluation

Operands, actuals, literal elements, bindings, fold bodies, and builder bodies evaluate left-to-right. The first error stops subsequent evaluation. An array literal reserves once, evaluates its syntactically distinct element expressions once each in order, writes after each success, performs no loop-test decision, and seals only after all elements succeed. A builder separately repeats its one indexed body for indices `0,...,n-1`, writes each success to a private region, and seals only after `n` successes. Fold indices are exactly `0,...,n-1`. Neither construction exposes a prefix; on error, partial storage may die but its cumulative events remain.

Aggregate return graphs remain source values. Target calls realize aggregate results in caller-owned destinations; no returned pointer may reference a dead callee frame.

The exported ABI invocation creates the initial active source activation with dynamic depth `d=1` and zero cumulative events. It does not add `chi_callReturn`. Only normative rules E-Call and E-CallErr for explicit DSL call expressions add that event and lift active depth; E-CallArgErr adds neither because no callee is entered.

### 2.6 Rejection and conformance cases

Stable diagnostics cover lexical, syntax, name, type, size, effect, termination, numeric, ABI, IR invariant, target validation, and internal compiler error. Required negative cases include recursion/call-order violation, unbounded-loop spelling, nonliteral/negative/overlarge capacity, unproved or out-of-range fold/build count, invalid accumulator, builder type mismatch, mutable-array request, unmodelled primitive, representation/address/frame overflow, malformed header, overlap/alias, invalid bit literal, and forbidden target feature.

Positive cases include every primitive success/error, both branches and guard error, literals at length 0/1/capacity with distinct element effects and failure at first/middle/last position, builder at 0/1/N and body failure at first/middle/last index, fold at 0/1/N and analogous body failure, accumulator replacement and retention, nested calls/products containing arrays, exact/upper/capacity size propagation, object death and slot reuse, zero-allocation subset, signed zeros, NaNs, and maximum legal address endpoints.

## 3. Compiler and IR requirements

### 3.1 Stages and records

| Stage | Mandatory record |
|---|---|
| Parse/resolve | source hash, language version, declarations/ranks, diagnostics |
| Type/effect | typed nodes, primitive-table version, forbidden-effect audit |
| Size | exact/upper/capacity shape derivation and count proofs |
| Cost/footprint | per-node event/L/P transformer and rule IDs |
| Normalize | source correspondence plus value, error, event-trace, and root-point audit |
| Liveness/layout | roots, intervals, aliases, deaths, slot coloring, frame/stack/linear-memory arithmetic |
| Source-to-BIR | CFG/source map, fold/builder certificates, capabilities, status edges |
| BIR validation | dominance/types/targets, initialization, liveness, address and call-rank evidence |
| BIR-to-Wasm scalar | structured-control and instruction/source maps, feature/import audit |
| BIR-to-Wasm SIMD | separate lane/mask/remainder/reduction map and PO-SIMD coverage |
| Standard validation/AOT | validator result; runtime/backend/version/target/flags/hash |

Each stage rejects an input without the preceding validation record. Source normalization is limited to administrative bindings and explicit control flow. It must not perform constant folding, dead-code or branch elimination, common-subexpression elimination, sharing introduction, duplication, reassociation, hoisting, sinking, call merging, allocation merging, or event-removing check elimination. Those transformations, if studied later, are target-only optimizations outside PO-NORM and outside comparison with source-event bounds. Debug and optimized builds must serialize semantically identical checked core/BIR before target-only optimizations.

### 3.2 BIR syntax, machine, and invariants

Represent exactly the appendix BIR: scalar/product/pointer types; register/literal operands; constant, primitive, product/projection, address, load, reserve, initialize, seal, and output-store instructions; branch, conditional branch, declared error, direct call, and return terminators; finite labelled blocks/functions/modules; capabilities carrying interval and builder-prefix metadata; frames with continuations/output destinations; a finite call stack; linear byte memory; and initial, final, declared-status, and internal-fault configurations. The validator and evaluator must cover every instruction and terminator—there is no generic “matching instruction” fallback.

Require dominated typed SSA uses; consistent block arguments; total labels; one terminator; direct rank-decreasing calls; unique status encoding; fold `(N,n,k)` certificates; builder `(N,n,j)` prefix and one-write/seal discipline; capability-checked memory; initialized-before-read; immutable-after-seal; aligned/disjoint simultaneous live slots; valid changing memory injection; caller-owned aggregate results; and no import/global/table/indirect call/memory growth/thread/exception.

BIR store capabilities are `inputRead`, `freshInit`, `scratchInit`, and `outputWrite`. `freshInit` writes only the next uninitialized builder location/header and becomes immutable after seal. `scratchInit` may touch only bytes with no current live source mapping. No operation mutates a live immutable source array.

The evaluator applies the appendix's total premise-to-fault map in this exact precedence: (1) invalid control fetch, label/depth/block arity, target/rank, or control-state shape gives `badControl`; (2) missing/ill-typed SSA operands, signatures, function arity, products, branches, memory widths, or returns give `badType`; (3) missing, wrong-kind, aliased, or write-forbidden capabilities give `badCap`; (4) uninitialized reads, non-next prefix writes, premature seal, or incomplete output give `uninit`; (5) invalid natural offsets, widths, count-derived regions, alignment, sums, or endpoints give `oob`; and (6) unavailable configured activation space gives `stackExhaust`. A primitive's declared arithmetic error follows its declared error edge and is not a BIR fault. Every failing dynamic premise maps to exactly one category by this order. Validation is intended to make these faults unreachable but does not prove PO-NOFAULT.

### 3.3 Liveness and frame requirements

A liveness certificate supplies the finite backward root-set solution at every normalized point and checked equality to the use/def/successor equation. A slot-colouring certificate assigns `(offset,extent,alignment)` and proves capacity dominance, frame endpoint, and pairwise disjointness for every pair live at a common point. A reuse edge identifies the dead source object, all former roots/capabilities, the point at which none reaches it, and the fresh object/interval. The injection removes the old mapping first. Reused bytes are unobservable until initialized through a valid capability. If any certificate fails, use an injective no-reuse layout in reservation order; do not continue with unchecked reuse.

Per-function frame `F_f` includes every slot and alignment gap and dominates frame-mapped live bytes. Maximum stacked frames follow the acyclic recurrence in the manuscript. Place half-open regions successively as aligned ABI input, ABI output, stacked frames, and runtime reserve, then round total linear memory to a Wasm page; check every intermediate endpoint in mathematical naturals. ABI output dominates output-mapped source objects. Engine operand-stack/call-stack storage outside linear memory is recorded separately. Cumulative source allocation is not substituted for any of these quantities and aggregate output is never double counted as a frame object.

### 3.4 Lowering by construct

Every source form must instantiate the appendix graph-fragment constructor with typed success and declared-error continuations. Every BIR instruction/terminator must instantiate the scalar Wasm constructor and structured-continuation certificate. Additional checks:

- numeric fold lowers to one preheader/header/body/latch/exit family and exactly one checked increment;
- builder lowers to a fresh assigned region, prefix parameter, ordered body, initialization-only store, error bypass, and seal;
- array literal lowers to a distinct finite acyclic chain with one reservation, one distinct element fragment per source position, one write after each success, no loop test or repeated body, first-error bypass, and one final seal;
- general index checks length before address/load; a removed check needs a dominance proof;
- aggregate return uses caller destination and no callee-frame escape;
- branch/let/call preserve first-error order; and
- target structuring cannot duplicate or reorder primitives, calls, or writes.

### 3.5 Wasm memory, ABI, and traps

Use one non-growing 32-bit little-endian linear memory. Header fields are 32-bit length and capacity, followed by aligned element storage. Before lowering, prove in mathematical naturals that every base is below `2^32`, every aligned endpoint is at most `2^32`, all region pairs required disjoint do not overlap, and the total module bound is within configured pages. Reject arithmetic overflow rather than relying on wrapped addresses.

The export receives input/output offsets and returns `i32` status. Host validation checks module identity, header types/capacities/lengths, initialized bytes, alignment, endpoints, and alias prohibition. Source errors return status. Distinct failure categories are ABI rejection; declared source error; BIR validation fault; Wasm trap; target resource exhaustion; host/runtime failure; AOT-backend miscompilation; OS termination; and hardware fault. Preserve the exact category in every test/run record. G2 is blocked by any unexpected category. Target-fault exclusion is PO-NOFAULT and is not implied by standard validation alone.

### 3.6 Scalar, production, and SIMD records

Record the following non-pooled matrices:

1. **MS:** strict O3 scalar Wasm/native with loop and SLP vectorization disabled and audited.
2. **PO:** strict-semantics production O3/LTO/target-feature Wasm/native with normal legal auto-vectorization and settings recorded.
3. **SIMD:** explicit K1/K2 Wasm/native vectors with fixed lanes, masks, remainder, reduction, and auto-vectorization disabled around the explicit region. K3 has no explicit-SIMD build; incidental legal K3 vectorization is an observed PO outcome only.

The same input bytes, contract ABI, error checks, precision, and output consumption apply. Fast-math/unsafe reassociation remain prohibited in every confirmatory family. Build identifiers include family; ratios never cross or pool families.

## 4. Correctness and proof-obligation plan

### 4.1 Formal status and empirical gates

The manuscript proves source termination in Theorem 4.1, source determinism in Theorem 5.1, and finite capacity instantiation in Theorem 6.2. Proof Obligation 5.2 is type/size/rooted-store preservation, Proof Obligation 6.1 is scalar resource soundness, and Proof Obligations 8.6 and 8.7 are target-fault exclusion and end-to-end observable preservation. Source-to-BIR simulation, memory-injection preservation, and BIR-to-Wasm simulation are supporting open obligations. Finite certificate checking and finite capacity instantiation do not close any of those obligations. Test outcomes use only: counterexample/mismatch/fault detected; none detected in the declared domain; or unresolved coverage/oracle. No test report may use “proved,” “verified compiler,” “sound analysis,” or “preserves” as an empirical conclusion.

### 4.2 Oracle and test categories

The reference evaluator follows the normative big-step, reachability, and event tables and never invokes generated output. Unit tests cover primitive semantics/events, parameter-complete sequence prefixes, literal prefixes distinct from builders, shape algebra, footprint recurrence, rooted graph size, layout arithmetic, status encoding, and ABI codecs. Conformance, property-based, controlled invalid mutation, metamorphic, differential, small-capacity translation validation, literal/builder/fold traces, and frame-reuse tests are all required.

Seeds, generator versions, derived paths, shrink history, exact inputs, configurations, artifacts, outcomes, and traces are retained. Every corrected failure becomes a permanent regression case.

### 4.3 Obligation traceability

| Obligation | Required evidence family |
|---|---|
| PO-NORM | every administrative normalisation rule; value/error equivalence, exact event trace, size shape, literal/builder allocation order, and live-root trace; forbidden optimizing transforms rejected |
| Determinism theorem | every expression family, Seq0/SeqS/SeqErr1/SeqErrS, literal-prefix rules, ordered error prefix, canonical NaN, fresh/dead store isomorphism |
| Termination theorem | literal suffix induction, zero/max/nested fold/build counts, maximum call rank, recursion rejection |
| PO-TYPE / PO-SIZE | generated typed terms, builder result lengths, fold shape recurrence, call substitution |
| PO-SIZE-SUBST | conditional/star/upper/capacity and nested-call substitution cases |
| PO-LIVE | roots across let/branch/fold/builder/call; retained versus dead accumulators |
| PO-COST-PRIM / PO-COST-ERR | every primitive result/error and every failing ordered prefix |
| PO-COST-LIT | length 0/1/N, distinct element fragments, first/middle/last error, one reserve/seal, no loop-test event |
| PO-COST-BUILD | 0/1/N, nested aggregate elements, early errors, cumulative/peak traces |
| PO-COST-FOLD | accumulator replacement and retention, 0/1/N, early errors |
| PO-COST-CALL / PO-COST-SOUND | nested parametric summaries, depth and caller/callee peak |
| PO-IR-WF | invalid dominance/type/status/capability/loop/builder/address cases |
| PO-MEM / PO-ADDR | alignment/endpoints/alias, seal discipline, slot reuse, caller result region |
| PO-SI-EXPR | every source construct and composition |
| PO-IW-CFG / PO-IW-NUM | path/status/iteration plus exhaustive feasible numeric bit classes |
| PO-NOFAULT | guarded division, bounds-before-address, initialization, endpoints, structured branch depth, frame/linear-memory capacity, closed-host assumptions |
| PO-SEM-PRES | end-to-end composition corpus; evidence only, not proof |
| PO-COST-TRANS | target trace/certificate checks when implemented; optional for functional readiness |
| PO-SIMD | lane/mask/remainder/order/NaN/zero plus variant-oracle and disassembly |

MS/PO performance is blocked unless G1/G2 have no unresolved issue for the exact scalar artifacts. SIMD is blocked independently on its oracle/transformation evidence.

The semantic-rule coverage manifest uses these exact rule identifiers and test families:

| Normative rule identifiers | Mandatory test family |
|---|---|
| Seq0, SeqS, SeqErr1, SeqErrS | empty/successful vectors; first/middle/last child failure; suffix free roots; prior aggregate operands and aliases |
| E-Var, E-Const | every scalar/aggregate environment lookup and exact bit literal class |
| E-Prim, E-PrimErr, E-PrimArgErr | every primitive signature, successful word class, declared arithmetic error, and ordered failing argument |
| E-Let, E-LetErr | scalar/aggregate binding, shadowing, retained suffix root, and bound-expression failure |
| E-IfT, E-IfF, E-IfErr | both selected branches, unselected side effects absent, guard failure, one decision only after a successful guard |
| E-Prod, E-ProdArgErr, E-Proj and its E-StrictErr instance | product arities, aggregate aliases, every field, and failing child prefixes |
| E-Lit, Lit0, LitS, LitErr | 0/1/N distinct elements, first/middle/last failure, one reserve/write-prefix/seal, no loop-test event, dead partial object |
| E-Len and its E-StrictErr instance | every valid array length and child failure |
| E-Index, E-IndexErr, E-IndexArgErr | lower/upper boundary success, negative/equal-length failure, array/index argument failures, length-before-address order |
| E-Fold, E-Fold0, E-FoldS, E-FoldErr, E-FoldArgErr | 0/1/N, accumulator replace/retain/alias, first/middle/last body failure, count/initial argument failure |
| E-Build, E-BuildCountErr, Build0, BuildS, BuildErr | 0/1/N repeated body, prefix invariant, first/middle/last failure, one seal, dead partial object |
| E-Call, E-CallErr, E-CallArgErr | rank-decreasing scalar/aggregate call, callee success/error, first/middle/last actual failure, caller-root alias, entry-versus-explicit-call cost |

Every test record names one or more rows and every accepted rule identifier has at least one positive and, where it has a premise or error edge, one boundary/negative case. This traceability is evidence coverage only, not a proof.

## 5. Financial kernels and datasets

The three kernel labels are design motivations, not sampled workload classes. Claims and models identify K1, K2, and K3.

### 5.1 K1: bounded binomial valuation

Let runtime depth `n <= D` and `D <= 2^31-2`. Terminal price at index `j` is expressed by two numeric folds of capacities `D`, one multiplying by `u` exactly `j` times and one by `d` exactly `n-j` times. An immutable builder of capacity `D+1` produces `n+1` terminal payoffs. An outer numeric fold of count `n` carries the current row; its body builds a fresh row of length `n-k` from adjacent values. The final scalar is index zero of the length-one row.

The conformance record must prove, for outer index `k`, current length `n+1-k`; next length `n-k`; and for builder index `j`, `j < n-k` and `j+1 < n+1-k`. No mutable source update is permitted.

Expected source bounds used as independent known answers:

- allocation events: `n+1` row builders;
- capacity region `R_D = region(f64,D+1)`;
- cumulative reservation: `(n+1) R_D`;
- peak live row scratch: `R_D` at `n=0`, otherwise `2 R_D`, contingent on a certificate that the new row does not retain the old;
- logical element writes: `(n+1)(n+2)/2` plus one header per row;
- backward nodes `B_n = n(n+1)/2`, initialized elements `W_n = (n+1)(n+2)/2`, and successful index reads `2 B_n + 1`;
- source loop tests `L_n = (n+1)(n+2) + W_n + 2(n+1)`, covering both terminal power folds, all builders, and the outer fold;
- conservative scalar primitive bound `P_n = n(n+1) + 3(n+1) + 5 B_n + (B_n + 2n + 2)`, where the last parenthesis is runtime count/index arithmetic and the payoff subtraction is charged on every terminal node;
- semantic reads `12(2 B_n + 1)`, writes `8 W_n + H(n+1)`, decisions `L_n + (n+1) + (2 B_n + 1)`, and active DSL depth one;
- abstract-operation upper bound `P_n + (2 B_n + 1) + (n+1) + L_n + (n+1) + W_n + (n+1)`, whose terms are primitives, index actions, payoff selections, loop tests, reservations, element writes, and seals.

Depths are `{8,16,32,64,128,256,512}`. Freeze exact-dyadic strata: `(S0,K)=(1,1),(5/4,1),(3/4,1)` and common `(u,d,q,beta)=(9/8,7/8,1/2,1)`. These values satisfy the risk-neutral relation `q=(beta^-1-d)/(u-d)=1/2` exactly. `beta` remains an ordinary input and the multiplication by `beta` remains in the source operation tree despite its frozen timed value one. Fixed across implementations: parameter bits, order, builder capacities, checks, layout, precision, ABI, and checksum.

K1 explicit SIMD uses exactly two `f64` lanes in Wasm and native. A full batch is two independent contracts at the same depth/stratum. For execution form `X`, define the RQ5 observation exactly as `T_K1_SIMD^X = B_K1_SIMD^X / 2`, where `B` is elapsed full-batch time including packing, the actual call boundary, both computations, unpacking, both statuses, and symmetric consumption of both results. Process/run summaries apply to these divided observations. Call this quantity “amortized per-contract batch time,” never single-contract latency. Report full-batch time and contracts/second as secondary quantities. An odd final contract takes a scalar remainder and is excluded from the full-batch estimator. Scalar single-contract latency remains separate.

### 5.2 K2: portfolio mark-to-market

Use equal-length `f64` arrays `q` (signed quantity), `p` (positive price), and `c` (signed adjustment), and increasing-index operation tree `v + ((q_i*p_i)+c_i)`. Sizes are `{16,64,256,1024,4096,16384}`. Timed values come from `{s*m*2^e}` with `s in {-1,1}`, `m in {1,1+2^-10,1+2^-20}`, and integer `e in [-8,8]`; price sign is positive. Construct field multisets by cycling this finite set and order by the 256-bit `SHA-256(seed || field-tag || index)`, tie-breaking by index. Independent uses distinct tags; concordant shares absolute quantity/price exponent rank; discordant reverses price rank. Freeze canonical concatenation, hash implementation/version, seed, and array hash.

Fix structure-of-arrays, 8-byte alignment/widths, scalar order, and no prefetch. Scalar reference/Wasm/C++ requires bit equality modulo NaN. SIMD uses two lane accumulators for `i mod 2` and final lane-0-plus-lane-1; Wasm/native SIMD must agree under that order. Report scalar/SIMD ULP/absolute/relative differences without using tolerance to mask a same-variant mismatch. Correctness adds empty/maximal arrays, signed zero, subnormal, infinity, NaN, and exponent boundaries; timed inputs are finite.

### 5.3 K3: order admission and fee aggregation

Record schema is `(active:bool,buy:bool,q:f64,p:f64,limit:f64,qmax:f64)`. State is `(count:i64,notional:f64,fee:f64)`, initially zero. Evaluate nested conditions in exact order: active; `q>0`; `q<=qmax`; side; buy `p<=limit` or sell `p>=limit`. Rejection leaves state unchanged. Admission computes `x=q*p`, then `rate = r0 if x<t1 else (r1 if x<t2 else r2)`, and returns `(count+1, notional+x, fee+(x*rate))` in that order. Freeze `(t1,t2)=(2,8)` and `(r0,r1,r2)=(2^-10,2^-9,2^-8)`. Sizes match K2; the count cannot overflow. Same-variant results require bit/NaN-quotient equality.

K3 runs in MS and PO only. Do not create an explicit-SIMD, masked, or predicated K3 treatment. Any legal auto-vectorization in PO is an observed compiler outcome and is not part of RQ5.

For target proportions `{0.10,0.50,0.90}`, use nearest integer admitted count with ties to even. Admitted templates cycle buy/sell and exact `x in {1,4,16}` fee tiers. Rejections are divided as evenly as possible among inactive, nonpositive quantity, over-cap, and price failure, with earlier predicates passing for later assigned failures. Hash-ranked dyadic perturbations are accepted only if assigned outcome/tier is preserved. Freeze template/rejection counts and multiset hash.

Order the identical multiset as hash-ranked uniform; maximum-transition alternating while both outcomes remain; and clustered with run lengths cycling `(1,2,4,8)` and hash-selected initial outcome. Record realized proportion, binary entropy, lag-one transitions, and run-length summary. For static sites active, positive quantity, cap, side, price, tier 1, and tier 2, record evaluations, true, false, and skipped-by-earlier counts. Outcome proportion and pattern are separate factors; do not infer predictor behavior from proportion alone.

### 5.4 Dataset preservation

Each dataset record contains schema/kernel/version, size and capacity, stratum and pattern, instance, generator/seed lineage, input hash, domain check, expected oracle hash, record-multiset hash, order hash, and storage path. Inputs become immutable before the randomized schedule. Real data are optional licensed sensitivity only.

## 6. Benchmark protocol

### 6.1 Metadata and environment

Record manufacturer/model/firmware; CPU model/stepping/microcode/ISA; sockets/cores/SMT; caches; RAM channels/speed; NUMA; OS/kernel/scheduler/isolation/mitigations; governor/frequency/turbo; temperature; pages/locking/ASLR; container/VM; counter permissions; and every compiler/runtime/validator/disassembly version and flag.

Pin to one physical core, disable or enforce idle SMT sibling, bind memory to one NUMA node, prevent migration, dedicate the host, and predeclare background/frequency/thermal limits. Primary fixed-frequency/turbo policy and any production-like sensitivity are separate. Prefault matched regions and record page/cache policies.

### 6.2 Build and run matrices

Run MS and PO for every kernel/size/stratum/pattern; run explicit SIMD separately for K1 and K2 only. Debug, sanitizer, reference, instrumented-cost, footprint, and certificate builds are correctness-only. Counter runs use the exact optimized timing artifact with different runner instrumentation.

Verify no vector instructions in MS, record legal generated vectors in PO, and verify intended explicit vectors in SIMD. Flags and disassembly reports are immutable.

### 6.3 Calibration, hierarchy, and order

Calibrate timer monotonicity/resolution, invariant cycles, empty brackets, harness dispatch, counters, frequency/NUMA/temperature controls, and checksums before every session. Warm-up has at least 10,000 calls and a frozen batch-stability rule/cap; retain the whole warm-up.

Before confirmatory data, run a claims-excluded pilot with five independent sessions, four fresh pairs/session, all kernels at smallest/middle/largest sizes, and initially `L0=4,096` valid ordered observations/run. For a run of length `L`, compute sample autocorrelation `rho_hat_L(l)` and the finite candidate set `B(L)={b in N: 1 <= b <= floor(L/64)-9 and |rho_hat_L(l)| < 0.1 for every l=b,...,b+9}`. If `B(L0)` is nonempty, freeze its minimum. This lag limit ensures at least 64 nonoverlapping blocks through lag `b+9`. If the set is empty, extend only the affected run deterministically to a total `L1=16,384` valid observations and apply the same rule from scratch to the complete extended run. Freeze sensitivity lengths `max(1,floor(b/2))` and `2b`. If `B(L1)` remains empty, or if the base or either sensitivity length cannot form at least 64 nonoverlapping blocks, mark the affected family unresolved and redesign it before confirmatory unblinding; there is no default block length.

Estimate centred session/pair/within-run residual distributions for paired log process-median contrasts. For candidate sessions `S={10,12,...,30}` and pairs/session `P={4,6,8,10}`, simulate 10,000 fixed-seed hierarchical datasets and apply the final simultaneous interval. Freeze the smallest pair for which at least 90% of simulations have maximum family log-half-width no greater than `log(1.05)`; tie-break by minimum `S*P`, then larger `S`. If none qualifies, reduce/preregister the confirmatory family or declare precision infeasible before unblinding. Pilot rows never enter confirmatory estimates.

Use the pilot-selected design, with at least ten independent time-block/day sessions and at least four fresh paired processes per session. Randomise/counterbalance Wasm/native order inside each build family and randomise kernel/size/stratum blocks from a preserved schedule. The hierarchy is session, paired process, counterbalanced block, run, batch, invocation, and stored sample. These are distinct levels and not interchangeable replications. Fewer than eight valid sessions makes confirmatory session-level inference unresolved; do not apply a three-cluster bootstrap.

### 6.4 Primary timing and diagnostic decomposition

Primary RQ3 latency times one actual contract invocation, including host/export boundary, required ABI work, kernel, status/output, and symmetric result consumption. It is never replaced by batching. The sole separate timing unit is the K1 RQ5 SIMD full two-contract batch; its derived `B/2` quantity is an amortized per-contract batch time and is never reported as primary or single-contract latency.

Separate diagnostics measure:

- D0 timestamp and empty harness;
- D1 host-to-Wasm no-work export boundary and analogous native call;
- D2 ABI validation/copying with identity body;
- D3 kernel-body region where scientifically instrumentable without changing the primary artifact claim;
- D4 result/status handling; and
- D5 optional in-module repeated execution that amortises boundary work.

D5 is labelled an amortised diagnostic and is not pooled with primary latency. Compilation, validation, load/instantiation, and first invocation are also separate.

### 6.5 Tail requirements

Preserve invocation order and analyse autocorrelation/drift. Report p50/p95/p99 only with dependence-aware intervals. p99.9 requires at least one million valid invocations per condition, at least 1,000 nominal upper-tail observations, at least 200 tail-containing dependence blocks across ten sessions, and stable finite block-bootstrap intervals under the frozen block-length sensitivity. Otherwise omit p99.9 and state which eligibility condition failed.

### 6.6 Raw schema

Every row includes schema/experiment/config/build-family/artifact/machine/session/process/pair/block/run/batch/iteration/invocation/sample IDs; kernel/size/capacity/stratum/pattern/input and multiset hashes; execution form; scheduled/actual order; phase (warm-up/primary/D0--D5/counter); metric/sample kind; raw ticks/unit/ns; iterations; checksum/status; CPU/core/NUMA/frequency/temperature/page/background evidence; counters and multiplex status; validity/reason; tool/environment hashes; and timestamp. Footprint traces additionally include object/root/death/peak/frame/linear-memory fields. Missing values are explicit.

Technical invalidity includes checksum/mismatch, migration, NUMA/frequency/thermal violation, timer/counter error, process/runtime fault, unexpected page fault/background load, incomplete batch, or hash drift. Observation magnitude is never an exclusion reason. Raw rows are append-only and checksummed.

## 7. Analysis plan

### 7.1 Metrics and estimands

For raw primary latency `y_spbricj`, compute each run median over invocations, then the median across scheduled runs/blocks within a process condition `T_spcj`. Define paired log contrast `D_spj=log(T_spWj)-log(T_spCj)`. The population estimand is `R_j=exp(E_session[E_pair|session(D_spj)])`; estimate it as the exponential of the equal-session average of within-session pair means. This is the geometric mean of paired ratios of process-level medians, not a ratio of pooled medians or a median of invocation ratios. Apply the analogous paired construction to K1/K2 SIMD and other named metrics. For K1 SIMD specifically, the observation entering the run median is the complete two-contract batch time divided by two; retain its “amortized per-contract batch time” label throughout.

Report named-direction ratios:

- `R_MS = T_W_MS / T_C_MS` and `R_PO = T_W_PO / T_C_PO`;
- relative overhead `100(R-1)%`;
- `S_W = T_W_MS / T_W_SIMD`, `S_C = T_C_MS / T_C_SIMD`, and interaction `S_W/S_C`;
- throughput/cycle/code-size/startup ratios with numerator and denominator named;
- bound violation, absolute/relative slack, and tightness for all scalar source components;
- cumulative reserved bytes and peak live scratch as distinct metrics;
- BIR frame, stacked-frame, and total linear-memory bounds separately;
- normalized counters per invocation and, where meaningful, per kernel item; and
- variant numerical absolute/relative/ULP differences.

### 7.2 Uncertainty and decisions

Analyze the defined paired process summaries on log scale. With at least ten valid sessions, resample sessions, pairs within selected sessions, and circular moving blocks within selected runs; recompute all medians and contrasts jointly for exactly 10,000 fixed-seed replicates. For each interval family, compute bootstrap standard deviation `s_j`, replicate maximum `M_b=max_j |(d*_bj-dhat_j)/s_j|`, its 0.95 quantile `c`, and simultaneous log intervals `dhat_j +/- c*s_j`, exponentiated. MS, PO, and each SIMD contrast set are distinct families.

If any `s_j=0`, fewer than ten sessions remain, moving blocks cannot be formed, or interval endpoints vary by more than 10% under frozen half/double block-length sensitivity, classification is unresolved. Session sign-flip and cluster-robust t intervals are sensitivity analyses and cannot replace the primary interval. Do not treat invocations as independent replicates.

Freeze practical limits before unblinding. Simultaneous 95% intervals classify every MS and PO family separately as faster, equivalent, slower, or unresolved. Holm applies only to separately labelled secondary point-null tests within a family; it is not applied to or conflated with simultaneous intervals. Report mixed family outcomes, not an omnibus preferred direction.

### 7.3 RQ4 and branch analysis

Primary size analysis is within kernel. Use kernel-specific size encoding and execution-form interaction with session/pair blocking. In a combined model, kernel identity is categorical and every size term is kernel-specific. Do not use “kernel class” as a factor. A secondary cross-kernel regression requires a preregistered normalized source bound (operations, bytes, or decisions); it never supersedes kernel-specific results.

For K3, estimate outcome-proportion and sequence-pattern effects and interactions separately. Verify equal record bytes/multisets/marginals. Analyze per-static-site evaluations, true, false, skipped, taken fraction, and transition/run summaries before relating hardware branch misses to a named site or factor.

### 7.4 Correctness gate reporting

G1 outcomes are counterexample detected, no counterexample detected in the declared test domain, or unresolved. G2 uses mismatch or target fault detected, none detected in the suite, or unresolved. A formal-status column reports the exact open proof obligations. Counterexamples/faults block affected performance; absence never discharges an obligation.

### 7.5 Required outputs

- formal-status/correctness-gate coverage table;
- exact/upper/capacity bound and peak/cumulative memory tables;
- K1 expressibility and known-answer cross-check table;
- separate MS and PO ratio forests with simultaneous intervals;
- within-kernel size contrasts and diagnostics;
- K3 proportion-by-pattern contrasts and branch counters;
- eligible latency quantiles with raw/effective tail support;
- call-boundary D0--D5 decomposition;
- K1/K2-only explicit SIMD speedups/interactions and numerical differences;
- code/counter/frame/linear-memory/disassembly tables; and
- invalid-run/missing-counter/deviation ledger.

Every output names its derived table, raw-row lineage, units, and ratio direction. Outliers remain unless a predeclared technical invalidity applies.

## 8. Reproducibility and artifact checklist

- [ ] language, primitive, semantics, size, cost, liveness, BIR, ABI, kernel, dataset, and schema versions frozen;
- [ ] exact runtime/compiler/validator/linker/library/tool manifests and hashes;
- [ ] immutable MS, PO, and SIMD matrices with verbatim settings;
- [ ] machine/environment capabilities and control evidence;
- [ ] randomization, sessions, seeds, inputs/multisets/orders, and hashes;
- [ ] rule/obligation/gate coverage and retained failures;
- [ ] size derivations, event/footprint summaries, liveness/frame certificates, and address proofs;
- [ ] core/BIR/Wasm/native/AOT artifacts, maps, reports, disassemblies, and checksums;
- [ ] append-only timing/counter/decomposition/invalidity data;
- [ ] one million/tail-support evidence where p99.9 is reported;
- [ ] deterministic analysis environment, seeds, derived lineage, plots/tables;
- [ ] one-command logical goals for later build/conformance/benchmark/analysis;
- [ ] README with privileges, time/disk, smoke/full reproduction, expected hashes, and paper map;
- [ ] minimal evaluation: verify hashes, check one module, run correctness, execute one small MS/PO pair, regenerate summary; and
- [ ] archive/license and read-only raw backup.

## 9. Risks and unresolved decisions

| Issue | Alternatives | Scientific impact | Required evidence/action | Resolution/status |
|---|---|---|---|---|
| Exact AOT release | stable Wasmtime/Cranelift candidates | code generation, call boundary, overhead | compatibility and AOT/no-tiering audit before data | must freeze |
| Exact Clang release | stable candidates | MS/PO code generation | semantic/flag/disassembly audit | must freeze |
| Machine/ISA | dedicated x86-64 or AArch64 | counters, SIMD, tail control | capability audit | must freeze |
| Timer | invariant cycles plus raw clock; raw clock | resolution/portability | calibration and cross-check | must freeze per machine |
| CPU policy | fixed/turbo off; production default | noise/ecological validity | access verification | fixed primary, optional separate sensitivity; must freeze |
| Practical band | provisional ±5%; domain SLA | equivalence classification | author/preregistration decision | must freeze |
| PO feature policy | target-native; portable subset | production interpretation | runtime/compiler capability audit | must freeze |
| Session calendar | 10+ independent blocks | hierarchy validity | machine access and preregistered schedule | must freeze |
| Tail block rule | finite stable-lag search with one extension; arbitrary fallback | p99.9 uncertainty | blinded dependence pilot and 64-block eligibility for base/half/double lengths | exact `L0=4096`, `L1=16384`, lag range, threshold, and unresolved outcome fixed |
| Comment/decimal syntax | core exact-bit/no-comment; optional pre-core erasure/conversion | parse/numeric conformance | amendment and separate conformance before use | core fixed; extension deferred |
| Count proof engine | trusted SMT; checked finite certificates | analysis trust boundary | certificate checker soundness and negative cases | checked finite certificates; resolved |
| Source NaN | relational primitives; canonical source result | determinism and native oracle | primitive bit-class conformance | canonical `0x7ff8000000000000`; resolved |
| Frame reuse | unique slots; certified coloring | footprint/tightness/proof complexity | PO-LIVE/PO-MEM and negative alias tests | certified reuse only; resolved |
| PO auto-vectorization | enabled normal safe transforms | production relevance | report/disassembly and semantic oracle | resolved |
| SIMD scope/reduction | K1/K2 fixed variants; masked K3 | numerical comparability and estimand identity | variant oracle/disassembly | K1 two-contract lanes and K2 lane-0-plus-lane-1; K3 excluded; resolved |
| Confirmatory replication | arbitrary minimum; pilot precision simulation | interval precision | claims-excluded pilot and frozen simulation manifest | exact grid/criterion fixed; chosen count awaits pilot |
| Real data | sensitivity or none | realism/license | immutable licensed corpus | synthetic primary; resolved |
| Statistical fallback | silent replacement; unresolved | conclusion validity | blinded diagnostics | no silent replacement; resolved |

If a must-freeze choice cannot be made without comparative outcomes, stop before unblinding. If the machine cannot meet the replication, timer, isolation, or p99.9 conditions, report the affected estimand unresolved or preregister an amended experiment; do not silently relax the rule.
