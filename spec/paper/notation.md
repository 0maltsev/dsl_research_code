# Canonical notation

| Symbol | Meaning |
|---|---|
| \(b\) | base scalar type: `bool`, `i32`, `i64`, or `f64` |
| \(\tau\) | DSL type |
| \(\operatorname{arr}\langle\tau,N\rangle\) | immutable array whose runtime length is at most static capacity \(N\) |
| \(N\) | compile-time natural capacity, with \(N\le N_{\max}=2^{31}-1\) |
| \(\ell\) | runtime array length, with \(0\le\ell\le N\) |
| \(\kappa=\mathsf{array}(\lambda,u,N;\bar\kappa)\) | array shape: exact symbolic length \(\lambda\), when known, proved upper bound \(u\), capacity \(N\), and conservative element-shape summary \(\bar\kappa\) |
| \(\star\) | unavailable exact symbolic length in a size summary |
| \((\lambda_n,u_n;\nu_n)\) | count refinement: exact expression or \(\star\), proved upper bound, and logical name for the runtime count |
| \(t,u,b\) | respectively a linear natural size term, an upper term additionally admitting maximum, and a resource expression additionally admitting capacity-bounded sums/recurrences |
| \(\Delta\vdash_{\mathsf{cert}}\phi\) | finite checked derivation of a size/count constraint; certificate generation is outside the checker |
| \(\mathsf{NW}_{+},\mathsf{NW}_{-}\) | certificate relations proving mathematical i32 count addition/subtraction is nonnegative and does not wrap |
| \(e,v,r\) | expression, value, and observable result (`ok` value or declared error) |
| \(M\) | checked module containing signatures, bodies, and declaration order |
| \(\Gamma\) | variable typing environment |
| \(\Delta\) | compile-time size-bound environment |
| \(\rho\) | source value environment |
| \(\eta\) | admissible assignment to symbolic input sizes |
| \(\Sigma\) | function signature/size/resource summary environment |
| \(\sigma\) | finite immutable source aggregate store, including administrative builder-prefix objects |
| \(K\) | continuation-root set used to define live aggregate reachability |
| \(\operatorname{Reach}(\Sigma;\rho,K,r)\) | aggregate identifiers reachable from the current environment, continuation roots, and result |
| \(\sigma\equiv_R\sigma'\) | stores agree up to an identifier bijection on every object reachable from root set \(R\); dead objects may differ |
| \(\epsilon\) | declared error code |
| \(q_{\mathrm{NaN}}\) | canonical source NaN word `0x7ff8000000000000` |
| \(\mathsf{can}(z)\) | maps every binary64 NaN word to \(q_{\mathrm{NaN}}\), leaving non-NaN words unchanged |
| \(\Sigma;K\vdash\langle e,\rho,\sigma\rangle\Downarrow\langle r,\sigma'\rangle\mid C\) | rooted deterministic big-step evaluation with final store and dynamic source cost |
| \(\Sigma;K\vdash\langle\vec e,\rho,\sigma\rangle\Downarrow_{\mathsf{seq}}\langle r,\sigma'\rangle\mid C\) | parameter-complete left-to-right sequence judgment; recursive premises add already obtained values to \(K\) and retain environment roots needed by the unexecuted suffix |
| \(\Sigma;K;\alpha;\vec v\vdash\langle\vec e,\rho,\sigma\rangle\Downarrow_{\mathsf{lit}}\langle r,\sigma'\rangle\mid C\) | literal-prefix judgment for one reserved object \(\alpha\), distinct remaining expressions, and successful prefix values \(\vec v\); it has no loop-test event |
| \(\Sigma;\Delta;\Gamma\vdash_{\mathrm{sz}}e\Rightarrow\kappa\) | static result-size judgment |
| \(\Sigma;\Delta;\Gamma;\Lambda\vdash e:\tau,\kappa\triangleright\Phi\) | static resource judgment with retained-root environment \(\Lambda\) and summary \(\Phi\) |
| \(C\) | scalar-source dynamic cost vector \(\langle o,a,h,p,r_b,w_b,q,d\rangle\) |
| \(o\) | abstract scalar/control operation events |
| \(a\) | logical local aggregate-reservation events |
| \(h\) | cumulative bytes reserved during an evaluation |
| \(p\) | peak bytes of simultaneously reachable local scratch, excluding ABI-owned roots |
| \(P_F,P_O,P_S\) | maximum frame-mapped live bytes, maximum caller-output-mapped live bytes, and actual source local peak \(\max_t(F_t+O_t)\) |
| \(r_b,w_b\) | source-model bytes read and written |
| \(q\) | control-decision events |
| \(d\) | maximum active DSL call depth; exported entry starts at one without a call/return event, and only explicit DSL calls add an activation/event |
| \(\Phi=\langle E,L,P\rangle\) | static resource summary: cumulative event bound \(E\), returned-local footprint bound \(L\), and peak-live scratch bound \(P\) |
| \(\oplus\) | sequential event composition: addition except maximum for call depth |
| \(\sqcup\) | componentwise worst-case join |
| \(\preceq\) | componentwise order on event vectors |
| \(f\prec_M g\) | function \(f\) is declared strictly before caller \(g\) in module \(M\), so calls are acyclic |
| \(B\) | a fold/builder execution bound: exact count when available, otherwise a proved upper bound |
| \(I\) | typed explicit-control-flow BIR program |
| \(W\) | validated WebAssembly module in the fixed scalar target subset |
| \(U=2^{32}\) | exclusive endpoint of the Wasm32 linear address space |
| \(\mu_t\) | time-indexed partial injection from live source objects to disjoint target regions |
| \(\operatorname{RegionOK},\operatorname{Disjoint}\) | target-region range/alignment and simultaneous-live-object separation predicates |
| \(R_{SI},R_{IW}\) | source--BIR and BIR--Wasm simulation relations |
| \(\approx_{\mathrm{obs}}\) | equal declared error or equal result type and scalar/aggregate contents, with NaN payloads quotiented as specified |
| \(F_f\) | certified local BIR frame size of function \(f\) |
| \(\operatorname{Stack}(f)\) | acyclic maximum frame recurrence for \(f\) and its callees |
| \(\mathsf{ABIIn},\mathsf{ABIOut},\mathsf{runtimeReserve},\mathsf{MemReq}\) | aligned input/output extents, nonoverlapping runtime metadata, and page-rounded total linear-memory requirement |
| \(S_I=\langle\Pi,\varphi,m\rangle\) | BIR call-stack/current-frame/linear-memory configuration |
| \(\mathsf{fault}_I(\kappa)\) | separately classified invalid BIR execution state; no source error is encoded as this fault |
| \(\mathsf{faultOf}\) | total ordered BIR failed-premise classifier: control, type, capability, initialisation, interval, then activation space |
| \(o_S,o_W\) | source and Wasm outcome classes, including separately named boundary/target failures |
| \(T_{spcj}\) | median of run medians for session \(s\), paired process \(p\), form \(c\), family \(j\) |
| \(D_{spj}=\log T_{spWj}-\log T_{spCj}\) | paired process-level log contrast |
| \(R_j=\exp\{\mathbb E_s\mathbb E_{p\mid s}D_{spj}\}\) | equal-session geometric-mean ratio estimand; not a ratio of pooled medians |
| \(S_X=T_{X,\mathrm{scalar}}/T_{X,\mathrm{SIMD}}\) | within-execution-form SIMD speedup for \(X\in\{W,C\}\) |
| \(B^{X,\mathsf{SIMD}}_{\mathrm{K1}}\) | full K1 two-contract SIMD batch time, including packing, call, both computations, unpacking, statuses, and result consumption |
| \(T^{X,\mathsf{SIMD}}_{\mathrm{K1}}=B^{X,\mathsf{SIMD}}_{\mathrm{K1}}/2\) | amortised per-contract batch time used in RQ5; never single-contract latency |
| \(B_j/D_j\) | tightness of static component \(j\) where \(D_j>0\); exact, symbolic-upper, and capacity-only modes are reported separately |

SIMD is not a source-language resource component. Optional SIMD counts belong to target lowering certificates and the K1/K2 experimental ablation; K3 is MS/PO only. `Cost` never denotes wall-clock time unless explicitly subscripted as a measured quantity.
