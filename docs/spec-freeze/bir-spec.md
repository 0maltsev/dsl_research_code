# BoundFin Intermediate Representation specification

Normative version: `bir-0.1.0`

## 1. Scope and determinism

BIR is a finite, first-order, typed, SSA control-flow graph between normalized BoundFin and scalar Wasm. It has no implicit operation, generic instruction fallback, dynamic dispatch, exception handler, import, table, global state, memory growth, thread, or source-visible mutation. Every instruction and terminator below has one validation clause and one small-step clause.

A validated BIR module plus initial memory has a deterministic transition relation. Validation is not itself a proof that lowering emits valid BIR or that faults are unreachable.

## 2. Abstract syntax

Identifiers for functions, blocks, registers, capabilities, and source objects are distinct finite namespaces. Natural metadata is arbitrary precision during validation.

```text
Type ::= bool | i32 | i64 | f64 | product<Type,...,Type>
       | pointer<source_type,capacity,capability_id>

Operand ::= Register(register_id) | Literal(typed_word)

Instruction ::=
    Const(dst,literal)
  | Primitive(dst,primitive_id,operands,error_edges)
  | Product(dst,operands)
  | Project(dst,field,operand)
  | Address(dst,capability,offset_operand,width,alignment)
  | Load(dst,type,capability,address_operand)
  | Reserve(dst_pointer,dst_capability,backing_capability,
            object_id,element_type,capacity,count_operand,storage_class)
  | Initialize(type,capability,address_operand,value_operand,element_index)
  | Seal(capability)
  | StoreScratch(type,capability,address_operand,value_operand)
  | StoreOutput(type,capability,address_operand,value_operand,output_field)

Terminator ::=
    Branch(target,arguments)
  | Conditional(condition,true_target,true_arguments,false_target,false_arguments)
  | Fail(declared_error)
  | Call(function,actuals,output_capability,
         success_target,success_arguments,error_targets)
  | Return(result_operand)

Block ::= Block(label,parameters,instructions,terminator,boundary_metadata)
Function ::= Function(name,rank,parameters,result_type,entry,blocks,
                      frame_layout,input_descriptors,output_descriptor,
                      loop_certificates,builder_certificates)
Module ::= Module(version,functions,entry,status_map,memory_layout,interface_id)
```

`error_edges` is empty for total primitives and maps each feasible one of `DivZero`/`DivOverflow` to a distinct block for error-capable integer division/remainder. A call has one target for each declared callee error. `InvalidABI` is outside BIR source execution.

`boundary_metadata` may contain checked source-point, root, death, injection-removal, and capability-recovery annotations. These are administrative and emit no source event. They are part of validation and transition state; they are not optional comments.

## 3. Values, memory, and frames

BIR scalar values are exact typed words; products are finite tuples; pointers are 32-bit addresses paired with an unforgeable capability identifier. Programs cannot inspect, compare, serialize, or arithmetically create a capability.

Linear memory is a fixed partial byte map `m:[0,2^32)->byte` with configured finite extent. It never grows. All offsets and endpoints are checked as naturals before their i32 address words are used.

A frame is:

```text
Frame = (function, block, next_instruction, valuation, frame_base,
         capability_map, output_destination,
         caller_success, caller_errors, source_point)
```

A running state is `(caller_stack,current_frame,memory)`. The caller stack is finite. Initial state has the one entry frame, decoded parameters, validated input/output capabilities, fixed memory, and no caller. Callee frame bases follow the validated aligned layout and configured stack limit.

## 4. Canonical capability vocabulary

There are exactly four serialized capability kinds:

```text
inputRead
freshInit
scratchInit
outputWrite
```

Every capability is linear metadata `(id,kind,[base,end),alignment,state,source_mapping)`. Except for a temporarily split parent ledger described below, two live capabilities cannot overlap. The program never sees this metadata.

### `inputRead`

- State: `complete(type_or_region)`.
- Permissions: aligned reads wholly inside the interval.
- Prohibitions: every write, reserve, seal, state change, or release.
- Source mapping: one or more immutable ABI source values fixed by validated decoding; ABI aliasing is prohibited, so implemented intervals are unique.

### `freshInit`

- Building state: `building(object,tau,N,n,j,storage)` with `0<=j<=n<=N` and `storage=frame|output`.
- Sealed state: `sealed(object,tau,N,n,storage)`.
- Building reads: initialized element representations with index `<j`; header and uninitialized elements are unreadable. Source lowering normally does not expose the prefix, but the evaluator defines the permission.
- Building write: only `Initialize` at exactly index `j`, correct element address/type/width, advancing to `j+1`.
- Seal: only at `j=n`; writes length/capacity header and changes to sealed state.
- Sealed reads: header and logical elements. No write or second seal.
- Source mapping: prefix object while building, immutable local object when sealed.

### `scratchInit`

- State: `scratch(initialized_byte_set,live_source_mapping=false)`.
- Permissions: `StoreScratch` inside the interval updates initialized coverage; `Load` may read only fully initialized requested bytes.
- Reserve: a properly aligned disjoint subrange may be consumed to create `freshInit(storage=frame)`.
- Prohibitions: overlap with a current source mapping, output serialization, or read of uninitialized bytes.
- Recovery: only certified death metadata may remove a sealed local mapping and re-create `scratchInit` for its interval. Without such metadata, the interval is not reusable.

### `outputWrite`

- State: `output(required_fields,written_fields,subregions,complete=false|true)`.
- Permissions: `StoreOutput` writes each declared scalar/product field exactly once in ABI field order or according to an equivalent checked coverage map.
- Aggregate reserve: a disjoint declared output subregion may be split from the parent ledger to create `freshInit(storage=output)`. While split, the parent records the subregion as unavailable rather than coexisting as an overlapping usable capability. Sealing the child rejoins it and marks that aggregate field complete.
- Completion: true only when every required result byte/aggregate field is complete and canonical padding remains zero.
- Reads: none while incomplete; the returned result may be read through a sealed child or after complete output when required by BIR semantics.
- Prohibitions: input/frame alias, duplicate field write, output observation on error, or return while incomplete.

Capability transitions are exactly:

```text
scratchInit --Reserve(frame)--> freshInit(building,frame)
outputWrite --Reserve(output subregion)--> output ledger + freshInit(building,output)
freshInit(building,j) --Initialize(next j)--> freshInit(building,j+1)
freshInit(building,n) --Seal--> freshInit(sealed)
freshInit(sealed,output) --rejoin--> outputWrite(field complete)
freshInit(sealed,frame) --certified death--> scratchInit
outputWrite(all fields complete) --complete--> outputWrite(complete)
```

No other transition exists. `immutableRead`/`imm`, `inR`, `fresh`, `scratch`, and `outW` are forbidden serialized names.

## 5. Validation

Validation checks finite structures and certificates only and terminates structurally. It checks:

1. module version, one entry, unique functions/blocks/register definitions, injective status map, exact interface identity, and admissible memory layout;
2. complete reachable block map, one terminator, consistent block arity/types, and total targets;
3. typed SSA definitions, dominance on every path, exact primitive/function/product/projection signatures, and result types;
4. direct calls only, callee rank strictly lower than caller rank, exact actual/result/error continuations, and caller-owned aggregate output;
5. capability existence, linearity, interval/alignment, operation permission, initialized-before-read, unique prefix writes, immutable-after-seal, output coverage, and no forbidden overlap;
6. all natural additions/multiplications/alignment/endpoints below or equal to `2^32` as appropriate, before bit encoding;
7. frame slots dominating capacity extents and pairwise disjoint for simultaneously live objects;
8. root dataflow certificates and death/reuse annotations before injection removal;
9. loop certificates with entry `k=0`, invariant `0<=k<=n<=N`, unique back edge `k'=k+1` under `k<n`, and exit only at `k=n`;
10. builder certificates with `j=k`, exactly one initialize on each successful body path, none on error paths, and one seal at exit;
11. no forbidden BIR construct, hidden memory operation, generic opcode, or unclassified failure edge.

Malformed but evaluable input is not “fixed” by validation. It receives a stable `BIRxxx` diagnostic and is rejected before lowering.

## 6. Ordered dynamic faults

At every attempted step, all failed premises are classified in this precedence; the first class wins:

1. `badControl`: missing/current frame, invalid fetch position, block/label/continuation shape or arity, invalid direct target/rank, or malformed control state.
2. `badType`: absent/ill-typed operand or destination, primitive/function arity/signature, product/projection, branch argument, condition, load/store width, or return type.
3. `badCap`: missing/wrong capability, forbidden permission, illegal overlap/alias, forged pointer/capability pair, or input/sealed write.
4. `uninit`: read of uninitialized bytes/prefix, non-next prefix write, premature/second seal, duplicate output field, or incomplete successful output.
5. `oob`: invalid natural offset/width/count region/alignment/sum/endpoint or access outside the named interval/linear memory.
6. `stackExhaust`: required validated callee activation cannot fit configured activation space.

Declared arithmetic errors take their declared error edge and are not faults. Validation intends to make the first five faults unreachable and configured resources intend to exclude the sixth; PO-NOFAULT remains open.

## 7. Small-step transitions

If no higher-precedence premise fails, the current instruction transitions as follows:

| Form | Successful action |
|---|---|
| `Const` | Bind exact typed literal to fresh destination; advance. |
| `Primitive` | Read operands left-to-right. On success bind the numeric result and advance. On `DivZero`/`DivOverflow`, enter its exact error block without binding a result. |
| `Product` | Bind ordered product of operands; advance. |
| `Project` | Bind one-based declared field; advance. |
| `Address` | Check natural offset/width/alignment in capability, bind paired pointer, advance. No wrapped address exists. |
| `Load` | Check read permission/initialization, decode exact bytes by type, bind, advance. |
| `Reserve` | Check count and backing subrange, consume/split backing capability, create zero-prefix `freshInit` and paired pointer, advance. It emits no runtime allocation instruction; storage was preassigned. |
| `Initialize` | Write one exact representation at next prefix element, advance capability prefix and instruction. |
| `Seal` | Require complete prefix, write length/capacity header, change to sealed; if output-backed, update parent ledger; advance. |
| `StoreScratch` | Write exact bytes, add initialized coverage, advance. |
| `StoreOutput` | Write exact result bytes, mark field coverage, advance. |
| `Branch` | Simultaneously bind target block parameters from arguments; set block and instruction zero. |
| `Conditional` | Require Boolean word and enter exactly one target with simultaneous arguments. |
| `Fail` | Unwind callers without reading output; at entry write/return the injective declared status and terminate. |
| `Call` | Check rank/types/frame/output, push caller continuation, create callee frame with actuals and caller destination, enter callee entry. |
| `Return` | Require result type and complete destination. If caller exists, pop, bind result representation, and enter success continuation; otherwise finish status zero. |

At an instruction/block boundary, checked metadata is applied in order: update source point/roots, remove certified dead mappings/capabilities, recover scratch intervals, then add any fresh mapping required by the next instruction. Removing and adding an interval in the reverse order is invalid.

## 8. Configurations and terminal states

```text
Running(stack,current,memory)
FinalSuccess(status=0,memory,typed_result_descriptor)
FinalDeclared(status in {1,2,3},memory_ignored)
Fault(kind in {badControl,badType,badCap,uninit,oob,stackExhaust},snapshot)
```

`InvalidABI` is a pre-BIR boundary result. A declared error ignores output payload. A successful terminal state requires a complete output record and status zero. There is no transition out of a terminal state.

Observable BIR result is the exact declared error or the ABI-decoded successful value under the numeric NaN quotient. Capabilities, frame addresses, dead bytes, unused capacity bytes, and object identifiers are unobservable.

## 9. Interpreter obligations

An implementation called a BIR interpreter must:

- cover every instruction and terminator above explicitly, with no default “matching instruction” behavior;
- implement the total fault precedence even for states violating multiple premises;
- retain step number, block/instruction, source point, valuation changes, capability changes, memory byte ranges, root/death annotations, and terminal category in its trace;
- use arbitrary-precision/natural validation arithmetic and reject before narrowing;
- be deterministic for fixed module/state and serialize traces deterministically modulo fresh object identifiers;
- execute BIR directly rather than dispatch through emitted Wasm/native code;
- distinguish validation rejection from dynamic fault and declared source error;
- round-trip through the canonical BIR serialization before differential evidence is accepted.

Two independent implementations agree when they produce the same terminal category/value and the same stepwise instruction, capability-state, and written-byte sequence modulo fresh identifiers and irrelevant dead memory.

## 10. Lowering constraints

- Array literal: one reserve, finite acyclic chain of distinct element fragments, one initialize after each success, no loop/counter test, one seal after all successes.
- Fold: preheader/header/body/latch/exit with exact `(N,n,k)` certificate and accumulator block parameter.
- Builder: count before reserve; `(N,n,j)` prefix; repeated single body; one next-element initialize; error bypass; seal only at exit.
- Index: runtime length read and signed bounds decision precede address; removing a check requires a dominating certificate.
- Calls: actuals left-to-right; caller output destination; exact status edge; no callee-frame result escape.
- Normalization/structuring cannot duplicate, remove, reorder, merge, hoist, sink, or reassociate a source action.

