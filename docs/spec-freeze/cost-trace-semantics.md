# Dynamic cost traces and static footprint semantics

Normative version: `boundfin-cost-trace-0.1.0`

## 1. Runtime objects and roots

Only arrays are source aggregate objects. Products are finite values that may contain array identifiers. A store object is either:

```text
sealed(alpha, element_type, capacity, length, origin, elements)
prefix(alpha, element_type, capacity, target_length, initialized_count,
       origin, initialized_elements)
```

`origin` is `abi` or `local`. A prefix exists only inside literal/builder administration. `Reach_sigma(V)` is the least set of array identifiers in values `V` and recursively in reached logical/prefix elements.

For an enclosing evaluation with continuation-root collection `K`, a configuration snapshot is:

```text
S = (sigma, K, R, activation_stack, phase)
```

`R` contains every value needed by the current semantic configuration: active environment values, previously evaluated operands, free-variable values of an unexecuted suffix, the current accumulator, a partial literal/builder object and its prefix values, and any pending result. `activation_stack` is a nonempty list whose first item is the exported entry. `phase` names the normative rule/auxiliary position and has no cost.

The exact locally live set and bytes are:

```text
Live_K(S) = { alpha in Reach_sigma(K union R) minus Reach_sigma(K)
              where origin(alpha) = local }

liveBytes_K(S) = sum over alpha in Live_K(S) of
                 region(element_type(alpha), capacity(alpha))
```

Each object is counted once even if multiple product fields or elements alias it. ABI-origin objects and local objects already reachable from `K` are not charged to this subevaluation.

## 2. Events, traces, and outcomes

An event is one of:

```text
Primitive(primitive_id)
Product(arity)
Projection(field)
Decision(kind)                       kind = conditional | loop
Reserve(alpha, element_type, capacity, region_bytes)
ElementWrite(alpha, index, element_type, width_bytes)
Seal(alpha, logical_length)
LengthRead(alpha)
Index(alpha, index, element_type, success)
CallEnter(function)
CallReturn(function, outcome_kind)
Death(objects, reason)
DeclaredError(code)
```

`Death` and `DeclaredError` are zero-cost trace markers. They make lifetimes and first-error boundaries observable to instrumentation without changing the paper's event vector.

A trace under original continuation `K` is the finite datatype:

```text
Trace_K = Start(S0) Step(event_1,S1) ... Step(event_n,Sn) Stop(outcome)
outcome = ok(value) | err(Bounds|DivZero|DivOverflow)
```

Every `S_i` uses the same original `K`. `S_n` is the terminal rooted snapshot: on success its retained roots are `K union {value}`; on error they are `K`. A restriction/death boundary is represented by a `Death` step whose following snapshot contains the restricted store/roots. The pre-death snapshot remains in the trace and therefore remains eligible for the peak.

The trace is parameter complete: rule identifier, source span, object identifiers, activation stack, and the full root reasons are retained in the instrumented serialization even when the mathematical notation abbreviates them.

## 3. Composition

Two traces `T1` and `T2` are compatible when:

1. they have the same original `K` and invocation identity;
2. `T1` ends at an intermediate successful boundary rather than a terminal source error;
3. the store, activation stack, and declared active-root handoff at the last snapshot of `T1` equal those at the first snapshot of `T2`, modulo rooted fresh-identifier isomorphism; and
4. every value required by the later continuation is already in the handoff roots.

For compatible traces, `T1 diamond_K T2` concatenates the event/step sequences and removes exactly one duplicate boundary snapshot. Fresh identifiers in `T2` are alpha-renamed capture-free if required. The operator is undefined on incompatible traces. It is associative modulo rooted identifier isomorphism because concatenation and duplicate-boundary removal are associative under the compatibility relation. `epsilon_K(S)` is the zero-event trace with one snapshot and is the identity.

Composition is defined on traces, not on already summarized peak numbers. The cost-vector notation `C1 plus_trace C2` means “compose their compatible traces and summarize the result.” It must not be implemented as `max(p1,p2)` when earlier values become roots of the later premise.

## 4. Exact peak and dynamic cost

For a nonempty trace:

```text
peak_K(T) = max { liveBytes_K(S_i) | S_i is any snapshot in T }
depth(T)  = max { length(activation_stack(S_i)) | S_i is any snapshot in T }
```

Both functions are total because the trace is finite and contains at least its start snapshot.

Event weights are:

| Event | `o` | `a` | `h` | `r_b` | `w_b` | `q` |
|---|---:|---:|---:|---:|---:|---:|
| `Primitive`, `Product`, `Projection` | 1 | 0 | 0 | 0 | 0 | 0 |
| `Decision` | 1 | 0 | 0 | 0 | 0 | 1 |
| `Reserve(...,R)` | 1 | 1 | `R` | 0 | 0 | 0 |
| `ElementWrite(...,tau,w)` | 1 | 0 | 0 | 0 | `w` | 0 |
| `Seal` | 1 | 0 | 0 | 0 | 8 | 0 |
| `LengthRead` | 1 | 0 | 0 | 4 | 0 | 0 |
| successful `Index(...,tau,true)` | 1 | 0 | 0 | `4 + w(tau)` | 0 | 1 |
| failed `Index(...,false)` | 1 | 0 | 0 | 4 | 0 | 1 |
| `CallEnter`, `CallReturn` | 1 each | 0 | 0 | 0 | 0 | 0 |
| `Death`, `DeclaredError` | 0 | 0 | 0 | 0 | 0 | 0 |

The exact dynamic cost is:

```text
C_K(T) = <sum(o), sum(a), sum(h), peak_K(T),
          sum(r_b), sum(w_b), sum(q), depth(T)>
```

Thus one completed explicit DSL call contributes two abstract operations. The exported entry starts with stack length one and emits neither call event. An actual-expression error emits no `CallEnter`; a callee success or declared error emits both enter and return.

## 5. Construct-specific trace rules

### Sequential children

Children run left-to-right. While child `j` runs, `R` contains all prior successful values plus free-variable values required by the unexecuted suffix. If child `j` errors, its trace is the final prefix: no later child event exists. This rule applies to primitive operands, products, calls, index operands, and fold count/initial operands.

### Let and conditionals

The bound value remains active while the body may use it. After its last certified use it may die; without a liveness certificate it remains active through the body. A conditional guard retains roots needed by both branches. A successful Boolean guard emits one `Decision(conditional)` and only the selected branch trace. A guard error emits no decision and no branch trace.

### Array literal

The literal emits `Reserve` before any element. The prefix object is active at every element boundary. Each distinct element expression runs once in source order; after success, `ElementWrite` occurs and the returned element graph joins the prefix roots. An element error emits neither its write nor `Seal`, then a death boundary removes the unreachable prefix/descendants. Full success emits one `Seal`. A literal emits no loop decision.

### Builder

The count evaluates before reservation. Count failure has no reservation. On successful certified `n`, `Reserve` creates a prefix object. For `j=0..n-1`, the trace emits `Decision(loop)`, runs the single body with index `j` and the prefix active, then emits one write on success. Body failure emits no write/seal and kills the unreachable prefix after retaining its earlier events. Exit at `j=n` emits one final loop decision and one seal.

### Fold

The count and initial accumulator evaluate left-to-right. For each `j<n`, one loop decision precedes the body. The old accumulator remains active until the new result is available; if the new result retains it, the whole reachable chain remains live. Body error stops immediately. At `j=n`, one final loop decision precedes return of the accumulator.

### Calls

Actuals run left-to-right with caller roots. After all succeed, `CallEnter(f)` pushes `f`; the callee body runs with caller continuation and actual values retained. On body success or declared error, `CallReturn(f,kind)` occurs before the callee frame is popped. A successful aggregate result remains a source graph; target caller-owned placement does not change source trace liveness.

### Failures

The first declared error appends `DeclaredError(code)`, restricts roots to the enclosing `K`, records deaths, and terminates the trace. All earlier reservations/writes/decisions remain in cumulative components and all earlier snapshots remain in `peak_K`. A failure never retroactively reduces cost.

## 6. Static footprint descriptors and total `Lambda`

Let `B` be the grammar of nonnegative checked resource expressions. A footprint descriptor is indexed by type:

```text
D_bool = D_i32 = D_i64 = D_f64 = zero
D_prod(t1,...,tk) = product(D_t1,...,D_tk)
D_arr(t,N) = array(self_bytes, logical_upper, element_descriptor)
```

Flattening is total:

```text
flat(zero) = 0
flat(product(D1,...,Dk)) = sum_i flat(Di)
flat(array(S,u,De)) = S + u * flat(De)
```

For nonuniform element shapes, `De` is the recursive join and the multiplication denotes the certified capacity-bounded sum. Duplicate identifiers are intentionally overcounted by `flat`; this is conservative by the paper's alias-overcount lemma.

`Lambda` is a total map for every `x:tau` in the typing environment:

- any scalar variable maps to `zero`;
- an exported ABI aggregate parameter maps to the all-zero descriptor because ABI-origin objects are excluded from local scratch;
- an aggregate formal of an earlier called function maps to a fresh recursive footprint parameter `lambda_x`, constrained by the caller's actual descriptor;
- a `let` variable maps to the successful descriptor inferred for its bound expression;
- a fold accumulator maps to the recurrence descriptor `D_j` and its index maps to zero;
- a build index maps to zero; the private prefix is represented by a synthetic array descriptor whose self and completed-element terms are updated at each step;
- variables introduced by normalization map to the descriptor of the source value they name.

Products retain component descriptors, so projection is total. Array descriptors retain an element descriptor, so indexing is total. If a checker cannot construct the required descriptor or recursive join, it rejects with `CST003`; it never substitutes zero.

At the exported entry every formal footprint parameter is instantiated to zero. At a direct call, formal descriptors, exact/upper size terms, `Q_f`, `K_f`, and `Phi_f` are instantiated simultaneously from the same ordered actuals.

## 7. Total result-footprint equations

Write `D(e)` for the successful result descriptor and `L(e)=flat(D(e))`.

| Expression | `D(e)` |
|---|---|
| scalar literal, scalar primitive, `len` | `zero` |
| variable `x` | `Lambda(x)` |
| product `(e1,...,ek)` | `product(D(e1),...,D(ek))` |
| `proj<j>(e)` | component `j` of `D(e)` |
| `e_array[e_index]` | element descriptor of `D(e_array)` |
| `let x=e1 in e2` | `D(e2)` under `Lambda[x := D(e1)]` |
| conditional | recursive join of selected-branch descriptors; exact array length survives only if equal |
| array literal | `array(region(tau,N), m, join_i D(e_i))` |
| builder | `array(region(tau,N), B, join_{0<=i<B} D(body_i))` where `B` is exact count or certified upper |
| fold | recurrence `D_0=D(initial)`, `D_{j+1}=D(body)[acc:=D_j,index:=j]`; exact `D_n` or join through upper `B` |
| call `f(actuals)` | stored `D_f` instantiated simultaneously with actual size and footprint descriptors |

Array `self_bytes` is zero only for an ABI-origin value represented by a parameter descriptor. A fresh literal/builder always charges its immediate capacity region. An array containing aggregate elements additionally charges their upper-bounded local graphs through the element descriptor.

## 8. Static event and peak equations

The analysis result is `Phi(e)=(E(e),D(e),P(e))`, where `E` excludes peak but includes cumulative components/depth, `D` is above, and `P` is a nonnegative expression.

Every child sequence is analyzed left-to-right under synthetic bindings for already computed operands. Therefore the later child's `P` already includes retained earlier descriptors; sequence peak is the maximum of these context-aware peaks, not an additional blind sum.

- Variable: `E=0`, `D=Lambda(x)`, `P=flat(Lambda(x))`.
- Constant: zero event, zero descriptor, peak zero.
- Primitive/product: sequence summary plus its terminal event; product returns component descriptors.
- Projection/length/index: operand sequence plus the applicable terminal event; index joins success/failure event bounds and returns the element descriptor on success.
- Let: analyze `e1`, then `e2` under `Lambda[x:=D(e1)]`; `P=max(P1,P2)`.
- Conditional: guard events plus one decision and branch event join; `P=max(P_guard,P_true,P_false)` under their declared root contexts.
- Literal: let `A_0=region(tau,N)` and `A_{j+1}=A_j+L(e_j)`. Analyze element `j` with synthetic prefix footprint `A_j`. `P` is the maximum of the reservation, every successful/error prefix, `A_m`, and those context-aware body peaks. Events join full success with every finite first-error prefix; no loop event appears.
- Builder: let `A_0=region(tau,N)` and `A_{j+1}=A_j+L(body_j)`. Join count failure, every body-error prefix, and success. `P=max(P_count,A_B,max_{j<B} P_body_j[prefix:=A_j])`. Events include reservation only after count success, one decision per entered/exiting test, one write per successful body, and a seal only on success.
- Fold: descriptor recurrence is above. Analyze body `j` with old accumulator retained. `P=max(P_count,P_initial,max_{j<B} P_body_j[acc:=D_j])`; events include `B` entered tests/bodies and the exit test, with finite error-prefix joins.
- Call: analyze actuals in order, instantiate the earlier function transformer with their descriptors/size terms, retain caller-visible actual footprints through the callee peak, emit enter/return, and lift callee relative depth by one. Actual error prefixes contain no call events.

All maxima/sums/recurrences require a literal finite upper certificate. Branch and error joins are componentwise worst cases. The extracted candidate vector is `<E.o,E.a,E.h,P,E.r_b,E.w_b,E.q,E.d>`.

These equations are total for every accepted expression. They remain candidate upper bounds until the paper's PO-SIZE-SUBST, PO-LIVE, PO-COST-ERR, PO-COST-CALL, and PO-COST-SOUND obligations are discharged. Dynamic conformance can find a violation but cannot prove soundness.

