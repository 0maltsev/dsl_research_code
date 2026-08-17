# Specification-to-evidence traceability

This table maps normative sources to planned implementation ownership, mandatory test families, generated evidence, and the claim it may support. Planned module paths do not yet exist. Tests can detect counterexamples; they do not discharge open proof obligations.

| Source/rule | Planned module | Mandatory tests | Generated evidence | Claim/obligation |
|---|---|---|---|---|
| Appendix A.1; `grammar.ebnf` lexical rules | `src/source/lex` | UTF-8, ASCII tokens, reserved words, exact bits, numeral/comment negatives | token/span record, `LEXxxx` cases | Concrete-language conformance only |
| Appendix A.1; complete EBNF/precedence | `src/source/parse` | every production, grouping/product split, associativity, malformed delimiters | parse AST/hash, `SYNxxx` coverage | Parser conformance; PO-NORM input |
| Declarations and `f prec_M g` | `src/source/resolve` | alpha-renaming/shadowing, duplicates, missing names, forward/recursive calls | symbol/rank table | TH-TERM premise; call acyclicity |
| Table 7 typing | `src/source/typecheck` | positive/negative case for each source form/operator | typed AST, rule IDs | PO-TYPE evidence only |
| Table 5; `numeric-semantics.md` | `src/source/typecheck`, `src/source/eval`, `src/native` | every primitive signature; bit classes; integer errors; NaN/zero/subnormal/infinity | primitive conformance record | TH-DET premise; PO-COST-PRIM; PO-IW-NUM |
| Appendix A.4 certificate rules; TH-CERT | `src/source/size/certificate` | each node, malformed trees, mutation, closed arithmetic, max | checked derivation/hash | TH-CERT implementation conformance |
| Table 6 count fragment including amended `Q_f` | `src/source/size/count` | every accepted constructor; arbitrary i32 rejection; exact/upper call substitution | count refinement/certificate | TH-TERM premise; PO-SIZE-SUBST |
| Table 8 exact/upper/capacity shapes | `src/source/size/shape` | nested arrays/products, branches, lets, fold recurrences, capacity fallback | shape derivation | PO-SIZE and PO-SIZE-SUBST |
| Seq0/SeqS/SeqErr1/SeqErrS | `src/source/eval` | empty/success, first/middle/last error, suffix roots, earlier aliases | exact source trace | TH-DET; PO-COST-ERR |
| E-Var/E-Const | `src/source/eval` | scalar and aggregate lookup; every bit literal class | outcome/trace rows | TH-DET base cases |
| E-Prim/E-PrimErr/E-PrimArgErr | `src/source/eval` | success, declared error, failing operand prefixes | event/outcome trace | PO-COST-PRIM/ERR |
| E-Let/E-LetErr | `src/source/eval` | scalar/aggregate binding, shadowing, retained/dead value, bound failure | root/death trace | PO-LIVE; PO-COST-SOUND |
| E-IfT/E-IfF/E-IfErr | `src/source/eval` | both branches, unselected absence, guard failure, one decision | selected-path trace | TH-DET; branch cost join |
| E-Prod/E-Proj/strict errors | `src/source/eval` | arities, aliases, every field, child errors | value/root/event trace | PO-TYPE/PO-LIVE |
| E-Lit/Lit0/LitS/LitErr; C-Lit | `src/source/eval`, `src/source/cost` | 0/1/N, distinct elements, first/middle/last error, one reserve/seal, no loop | prefix/store/event and bound comparison | PO-COST-LIT; PO-CTRL |
| E-Len | `src/source/eval` | every length, nested array, child failure | read/event trace | PO-TYPE/PO-COST-PRIM |
| E-Index/E-IndexErr/E-IndexArgErr | `src/source/eval` | negative, zero, last, equal-length, malformed args, order | bounds/read/outcome trace | PO-COST-ERR; PO-ADDR |
| E-Fold/E-Fold0/S/Err; C-Fold | `src/source/eval`, `src/source/cost` | 0/1/N, replace/retain/alias accumulator, body/count/initial errors | recurrence, trace, candidate comparison | PO-COST-FOLD; PO-CTRL |
| E-Build/Build0/S/Err; C-Build | `src/source/eval`, `src/source/cost` | 0/1/N, nested elements, prefix, body/count errors, dead partial | prefix/trace/bound rows | PO-COST-BUILD; PO-CTRL |
| E-Call/E-CallErr/E-CallArgErr | `src/source/eval`, `src/source/cost` | scalar/aggregate nested calls, actual/body errors, caller aliases, depth | call/root/depth trace | PO-COST-CALL; PO-SIZE-SUBST |
| Dynamic trace and `peak_K` | `src/source/trace` | compatible composition, incompatible rejection, pre/post-death peak, failure prefixes | serialized snapshots/root reasons | TH-DET dynamic-cost clause; PO-COST-SOUND |
| Total `Lambda`/descriptors | `src/source/cost` | scalar/ABI/formal/local/product/array/accumulator/result cases | descriptor derivation | PO-LIVE/PO-COST-CALL/SOUND |
| A.8 and `abi-layout.md` scalars/products | `src/abi` | known offsets/padding, bool invalids, bit round trips | codec/layout certificate | PO-ADDR; ABI conformance |
| Array/nested capacity tree ABI | `src/abi` | 0/1/N, nested products/arrays, unused zeroes, endpoint/alias/cycle negatives | region tree/validation record | PO-MEM/PO-ADDR |
| Export/status/host obligations | `src/abi`, `src/runtime` | exact names/signature, every status, invalid output, no decode on error | ABI round-trip/result record | G2 boundary evidence |
| PO-NORM restrictions | `src/normalize` | each administrative rewrite; forbidden fold/eliminate/share/reorder/reassociate | source correspondence, before/after traces | PO-NORM |
| B.3 Roots equation | `src/layout/liveness` | equation equality, future roots, nested calls/failures | liveness certificate | PO-LIVE; PO-MEM |
| Slot colouring/no-reuse fallback | `src/layout/frame` | simultaneous-live pairs, death/reuse, alignment/endpoints, fallback | colouring/frame certificate | PO-MEM/PO-ADDR |
| BIR abstract syntax/types/SSA | `src/bir/model`, `src/bir/validate` | round trip, dominance, arity/type/label negatives | BIR/hash/validation report | PO-IR-WF evidence |
| Canonical BIR capability system | `src/bir/validate`, `src/bir/eval` | every allowed transition and forbidden overlap/write/read/seal/output case | capability transition trace | PO-MEM; PO-NOFAULT |
| BIR ordered faults | `src/bir/eval` | each fault and multi-fault precedence combinations | fault trace/code | PO-NOFAULT classification |
| BIR loops/builders | `src/bir/validate`, `src/bir/eval` | invariant/backedge/exit/prefix/write/error/seal mutation | certificate and negative report | PO-IR-WF; PO-CTRL |
| Table 13 source-to-BIR constructors | `src/lower/source_to_bir` | every source form, exact first-error/action order | source map, validated BIR | PO-SI-EXPR/PO-CTRL |
| Table 14 BIR-to-Wasm constructors | `src/lower/bir_to_wasm` | every instruction/terminator, paths/status/iterations | structuring/lowering map | PO-IW-CFG/PO-IW-NUM |
| Scalar Wasm forbidden subset | `src/lower/bir_to_wasm`, `src/runtime` | standard validation, feature mutation, import/grow/SIMD negatives | Wasm feature/validator report | PO-NOFAULT premise |
| AOT/no-tiering feasibility | `src/runtime` | F-AOT-01/02, trusted hashes, compiler-free runner | AOT manifest/process audit | RQ3 artifact identity, not semantic proof |
| Source/reference/BIR/Wasm/native relation | `src/correctness` | conformance, generated, metamorphic, differential, retained shrinks | correctness-result records | G2 only; PO-SEM-PRES remains open |
| Dynamic versus static components | `src/correctness` | exact/upper/capacity comparisons, error paths, peak instrumentation | correctness/bound rows | G1 only; PO-COST-SOUND remains open |
| K1 definition and known bounds | `src/kernels/k1`, `src/datasets/k1` | `n=0`, planned depths, dyadic known answers, row liveness | source/dataset/oracle/bound hashes | RQ1/2 and K1-specific RQ3/4/5 |
| K2 definition/dataset order | `src/kernels/k2`, `src/datasets/k2` | multiset strata, hash ordering, scalar/SIMD orders, special floats | dataset and variant-oracle records | K2-specific RQ3/4/5 |
| K3 definition/pattern factors | `src/kernels/k3`, `src/datasets/k3` | admission counts/tiers, equal multisets, pattern/site counters | dataset invariants/branch trace | K3-specific RQ3/4; no RQ5 |
| MS and PO build families | build manifests, `src/runtime`, `src/native` | flag/hash/disassembly audits, no MS vectors, strict semantics | compiler certs/disassembly | Separate RQ3 families |
| K1/K2 explicit SIMD | `src/lower/simd`, `src/native/simd` | lane/mask/remainder/order/NaN/zero, intended disassembly | variant result and transform map | PO-SIMD open; RQ5 evidence |
| Benchmark hierarchy/order | `src/benchmark/schedule` | counterbalance, PRNG test vectors, regeneration hash | experiment manifest/schedule | Estimand validity |
| Timer/environment/invalidity | `src/benchmark/runner` | calibration/failure injection/migration/frequency/page/counter checks | benchmark rows/invalidity ledger | RQ3–5 machine evidence |
| Pilot block/precision rules | `analysis/pilot` | synthetic known answers, extension/no-fallback, fixed-seed replay | pilot-only decision record | Phase 15 design, no empirical claim |
| Hierarchical simultaneous interval | `analysis/confirmatory` | hierarchy-preserving bootstrap, zero-SE/few-session/sensitivity failures | derived table with raw lineage | RQ3–5 classification |
| p99.9 eligibility | `analysis/tails` | million/1000/200-block/10-session/sensitivity gates | support diagnostic | Tail report or explicit omission |

## Coverage rules

- Every accepted source/BIR rule identifier must appear in at least one positive test; every premise/error boundary must have a negative or boundary test where meaningful.
- Every generated test result names source rule IDs, implementation artifact hashes, schema/spec versions, configuration, dataset, and seed.
- A paper theorem is cited as a theorem only at its stated mathematical boundary. Artifact tests are implementation conformance evidence.
- A failed G1/G2 row blocks the exact affected performance artifact and reopens the corresponding rule/obligation; a passing row never closes a proof obligation.
- Performance outputs name raw-row lineage, units, ratio direction, build family, kernel, size, stratum/pattern, runtime/compiler/machine, and validity decisions.

