# Diagnostics, statuses, and failure identities

Normative version: `boundfin-diagnostics-0.1.0`

Diagnostic codes are stable API. A code's meaning may be clarified without changing its category, but it may not be reused. A behavior change requires a new code and specification version. Diagnostics include code, severity, primary source/artifact location, deterministic message template, ordered related locations, and structured parameters. Human text is not used as an evidence key.

## Lexical diagnostics

| Code | Meaning |
|---|---|
| `LEX001` | Invalid, non-shortest, or BOM-prefixed UTF-8. |
| `LEX002` | Character is outside the permitted ASCII token/whitespace set. |
| `LEX003` | Unknown or malformed token. |
| `LEX004` | Bit literal has wrong prefix, digit, or exact width. |
| `LEX005` | Reserved word used where an identifier is required. |
| `LEX006` | Decimal numeral has a forbidden leading zero or lexical overflow during natural conversion. |
| `LEX007` | Source comment or decimal floating literal is unsupported in language 0.1.0. |

## Syntax diagnostics

| Code | Meaning |
|---|---|
| `SYN001` | Unexpected token for the current production. |
| `SYN002` | Expected token is missing at end of input or before another token. |
| `SYN003` | Product type/value has arity below two. |
| `SYN004` | Chained non-associative equality or comparison. |
| `SYN005` | Malformed function declaration, parameter, or result annotation. |
| `SYN006` | Malformed array literal, fold, or builder punctuation/binders. |
| `SYN007` | Export is missing, duplicated, nonfinal, or followed by source tokens. |

## Name-resolution diagnostics

| Code | Meaning |
|---|---|
| `NAM001` | Duplicate function declaration. |
| `NAM002` | Duplicate parameter or binder in one binding list. |
| `NAM003` | Unbound variable. |
| `NAM004` | Unknown function. |
| `NAM005` | Export names no declaration. |
| `NAM006` | Call target is not strictly earlier than caller. |
| `NAM007` | Variable/function namespace used in the wrong syntactic role. |

Lexical shadowing by `let`/fold/build is accepted and alpha-renamed; it is not `NAM002` unless the same binding list duplicates a name.

## Type diagnostics

| Code | Meaning |
|---|---|
| `TYP001` | Primitive/operator operand types do not match one exact signature. |
| `TYP002` | Function actual arity or type mismatch. |
| `TYP003` | Let/binder annotation or resolved use type mismatch. |
| `TYP004` | Conditional guard is not `bool`. |
| `TYP005` | Conditional branch result types differ. |
| `TYP006` | Product field types/arity mismatch. |
| `TYP007` | Projection index is outside one-based product arity. |
| `TYP008` | Array literal elements are heterogeneous. |
| `TYP009` | `len`/index applied to nonarray or index is not `i32`. |
| `TYP010` | Fold initial/body/accumulator types are not invariant. |
| `TYP011` | Builder body type does not match result element type. |
| `TYP012` | Unsupported implicit conversion, floating remainder, or Boolean ordering. |

## Size/count diagnostics

| Code | Meaning |
|---|---|
| `SIZ001` | Capacity is negative, nonliteral, or greater than `2^31-1`. |
| `SIZ002` | Array literal length exceeds capacity. |
| `SIZ003` | Expression has no accepted count-refinement rule. |
| `SIZ004` | Missing/invalid finite count certificate. |
| `SIZ005` | Count upper bound is not proved within capacity. |
| `SIZ006` | Count add/sub lacks the required no-wrap/nonnegative certificate. |
| `SIZ007` | Exact/upper/capacity shape is malformed or constraint-inconsistent. |
| `SIZ008` | Recursive element-shape join or fold recurrence cannot be formed. |
| `SIZ009` | `Q_f` is absent or invalid for a call used as a count. |
| `SIZ010` | Simultaneous call-summary substitution is incomplete/capturing. |
| `SIZ011` | Representation, frame, address, increment, or total-memory natural exceeds its admitted bound. |
| `SIZ012` | Nested type/capacity tree exceeds configured representation limits. |

## Effect/termination diagnostics

| Code | Meaning |
|---|---|
| `EFF001` | Mutation or mutable-array request. |
| `EFF002` | Address observation or pointer operation. |
| `EFF003` | Import, callback, host access, I/O, clock, randomness, or persistent/global state. |
| `EFF004` | Recursion or cyclic call graph. |
| `EFF005` | Unbounded/general loop syntax. |
| `EFF006` | Dynamic allocation, variable capacity, or unbounded container. |
| `EFF007` | Higher-order value, closure, dynamic dispatch, or indirect call. |
| `EFF008` | Exception/handler, thread, shared memory, or atomic behavior. |
| `EFF009` | Unmodelled primitive, transcendental, relaxed floating, or forbidden conversion. |

## Cost/footprint diagnostics

| Code | Meaning |
|---|---|
| `CST001` | No terminal event row for a typed source action. |
| `CST002` | Resource sum/recurrence lacks a literal finite upper certificate. |
| `CST003` | Required retained-root/footprint descriptor is unavailable. |
| `CST004` | `Lambda` is missing or ill-typed for a variable. |
| `CST005` | Literal/fold/builder error-prefix summary is incomplete. |
| `CST006` | Function cost/footprint transformer substitution is incomplete. |
| `CST007` | Candidate resource expression is negative, cyclic, or not evaluable to a finite natural. |
| `CST008` | Static/dynamic rule identifier or event-table version mismatch. |

## BIR validation diagnostics

| Code | Meaning |
|---|---|
| `BIR001` | Unsupported BIR version or malformed module/function/block structure. |
| `BIR002` | Missing/duplicate label, invalid target, block arity, or terminator count. |
| `BIR003` | SSA definition/use or dominance violation. |
| `BIR004` | Operand/destination/instruction/return type mismatch. |
| `BIR005` | Invalid direct-call target, rank, actual, result, or error continuation. |
| `BIR006` | Missing, forged, wrong-kind, overlapping, or permission-invalid capability. |
| `BIR007` | Uninitialized read, non-next initialization, premature seal, or duplicate output write. |
| `BIR008` | Natural offset, width, alignment, endpoint, frame, or memory layout invalid. |
| `BIR009` | Loop certificate invalid or incomplete. |
| `BIR010` | Builder prefix/write/seal certificate invalid. |
| `BIR011` | Liveness/root/death/reuse/slot-colouring certificate invalid. |
| `BIR012` | Declared status map is noninjective or incomplete. |
| `BIR013` | Caller-owned aggregate result/output completeness invariant fails. |
| `BIR014` | Forbidden BIR construct or generic/unclassified operation. |

## ABI diagnostics

| Code | Meaning |
|---|---|
| `ABI001` | Input/output base or configured memory extent mismatch. |
| `ABI002` | Region endpoint is outside Wasm32 or required regions overlap. |
| `ABI003` | Input/output magic mismatch. |
| `ABI004` | ABI version mismatch. |
| `ABI005` | Exact region extent or parameter count mismatch. |
| `ABI006` | Interface identity mismatch. |
| `ABI007` | Invalid Boolean/scalar encoding. |
| `ABI008` | Array capacity differs from static type. |
| `ABI009` | Array logical length exceeds capacity. |
| `ABI010` | Aggregate offset is zero when logical, misaligned, out of region, aliased, cyclic, or not canonically owned. |
| `ABI011` | Padding, reserved, unused-capacity, or unused-preallocation byte is nonzero. |
| `ABI012` | Required logical input byte is missing/uninitialized. |
| `ABI013` | Output status/header/payload completeness is inconsistent. |
| `ABI014` | Host attempted payload decode on nonzero status. |

## Wasm validation/AOT diagnostics

| Code | Meaning |
|---|---|
| `WASM001` | Standard WebAssembly validation failed. |
| `WASM002` | Required export name/signature/memory contract mismatch. |
| `WASM003` | Import, start, global, table, indirect call, reference type, or callback present. |
| `WASM004` | Memory growth, shared memory, thread, atomic, or exception present. |
| `WASM005` | SIMD/relaxed SIMD present in scalar artifact or forbidden target feature present. |
| `WASM006` | Numeric lowering/profile violates strict scalar requirements. |
| `WASM007` | Structured-control/source map certificate invalid. |
| `WASM008` | AOT artifact/configuration is incompatible, unhashed, or not load-only. |
| `WASM009` | Machine-code/disassembly capture or artifact reproducibility check failed. |
| `WASM010` | Counter attribution or stable native-call-boundary feasibility check failed. |

## Internal compiler diagnostics

| Code | Meaning |
|---|---|
| `INT001` | Internal invariant failed after a validated preceding stage. |
| `INT002` | Serialization/deserialization changed a checked artifact. |
| `INT003` | Nondeterministic artifact bytes for identical inputs/configuration. |

An `INTxxx` diagnostic is always a defect and blocks the artifact. It is never presented as user source rejection.

## Source and boundary status words

| Word | Stable name | Layer |
|---:|---|---|
| `0` | `Success` | ABI/Wasm/native. |
| `1` | `Bounds` | Declared source error. |
| `2` | `DivZero` | Declared source error. |
| `3` | `DivOverflow` | Declared source error. |
| `4` | `InvalidABI` | Pre-source boundary result. |

No compiler diagnostic is a status word. No BIR/target/runtime failure is mapped to a status word.

## BIR dynamic fault names

Fault names and precedence are exactly: `badControl`, `badType`, `badCap`, `uninit`, `oob`, `stackExhaust`. Records additionally use stable codes `RUN001` through `RUN006` in that order.

## Runtime/experimental failure codes

| Code | Failure status enum | Meaning |
|---|---|---|
| `RUN001` | `bir_fault` | `badControl`. |
| `RUN002` | `bir_fault` | `badType`. |
| `RUN003` | `bir_fault` | `badCap`. |
| `RUN004` | `bir_fault` | `uninit`. |
| `RUN005` | `bir_fault` | `oob`. |
| `RUN006` | `resource_exhaustion` | `stackExhaust`. |
| `RUN007` | `wasm_trap` | Unexpected validated-Wasm trap. |
| `RUN008` | `resource_exhaustion` | Linear memory, process, or runtime resource exhaustion. |
| `RUN009` | `host_failure` | Host/runtime API or closed-boundary failure. |
| `RUN010` | `backend_failure` | AOT/backend miscompilation or artifact-load defect. |
| `RUN011` | `os_termination` | Signal, scheduler/OS termination, or process loss. |
| `RUN012` | `hardware_failure` | Machine-check or attributable hardware failure. |
| `RUN013` | `mismatch` | Cross-implementation observable mismatch. |
| `RUN014` | `checksum_failure` | Artifact/input/output checksum or identity drift. |
| `RUN015` | `environment_invalid` | Migration, NUMA/frequency/thermal/page/background/timer policy violation. |
| `RUN016` | `counter_failure` | Counter unavailable, multiplexed contrary to policy, or attribution invalid. |
| `RUN017` | `incomplete` | Interrupted/incomplete batch, run, or required metadata. |

Latency magnitude is never a failure or exclusion code.

## Record rule

Every generated record has required fields `status`, `failure_status`, `diagnostic_codes`, and `failure_detail`. On success, `failure_status` and `failure_detail` are explicit `null` and diagnostics may be empty. On failure, `failure_status` is nonnull, at least one stable code is present, and no successful claim is emitted. Inapplicable data remains explicit `null`, not omitted.

