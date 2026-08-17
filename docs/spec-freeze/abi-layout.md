# Source and WebAssembly ABI

Normative version: `boundfin-abi-0.1.0`

All layout arithmetic is performed in mathematical natural numbers and checked before conversion to a 32-bit address. All multibyte values are little-endian. The linear address limit is `U=2^32`, used as an exclusive endpoint.

## 1. Scalar encodings

| Type | Width | Alignment | Encoding |
|---|---:|---:|---|
| `bool` | 4 | 4 | Unsigned word `0` for false, `1` for true; every other word is invalid ABI. |
| `i32` | 4 | 4 | Exact 32-bit source word. |
| `i64` | 8 | 8 | Exact 64-bit source word. |
| `f64` | 8 | 8 | Exact binary64 word; any NaN payload is accepted. |

Integer arithmetic semantics are not inferred from the encoding; they are defined in `numeric-semantics.md`. Input decoding does not canonicalize NaNs. Target output may contain any Wasm-permitted NaN and is compared through the observation quotient.

## 2. Inline type representation

Let `align(x,a)=a*ceil(x/a)` for a positive power-of-two `a`. Define inline alignment `A(tau)`, inline width `W(tau)`, and product field offsets recursively:

```text
A(bool)=A(i32)=4                    W(bool)=W(i32)=4
A(i64)=A(f64)=8                    W(i64)=W(f64)=8
A(arr<tau,N>)=4                    W(arr<tau,N>)=4

A(prod<t1,...,tk>) = max_i A(ti)
off_1 = 0
off_i = align(off_(i-1) + W(t_(i-1)), A(t_i))
W(prod<t1,...,tk>) = align(off_k + W(t_k), A(prod<t1,...,tk>))
```

Products are inline, ordered field structures. Nested products are laid out recursively and introduce no pointer or source allocation. An array occurrence is represented inline by one unsigned 32-bit absolute offset to an array object. Product and array-element padding is part of the ABI and must be zero.

Products have at least two fields. There is no variant, nullable reference, pointer arithmetic value, or platform-native struct layout.

## 3. Array objects and immediate source regions

An `arr<tau,N>` object at base `b` has:

```text
b + 0 : u32 logical_length
b + 4 : u32 declared_capacity
e     = align(b + 8, A(tau))
e + i*stride(tau) : inline element i, for 0 <= i < N
stride(tau) = align(W(tau), A(tau))
region(tau,N) = align((e-b) + N*stride(tau), max(4,A(tau)))
```

The capacity header must equal the capacity in the static type. The logical length must satisfy `0 <= length <= N`. Only elements below logical length are values. Each logical array-typed element contains a nonzero offset to its owned nested object. Unused capacity elements and their padding are zero and are never decoded as values.

`region(tau,N)` is the immediate array-object amount used by source reservation, cumulative bytes, and source peak. Descendant array objects are distinct source objects and are charged separately when locally constructed.

## 4. Canonical nested capacity tree

The ABI reserves enough space for every statically possible descendant array occurrence so that input/output extents do not depend on runtime lengths. Because differently aligned descendant objects may require intermediate padding, the extent is defined by an address transformer rather than an unaligned size sum:

```text
PlaceDesc(scalar,p) = p
PlaceDesc(prod<t1,...,tk>,p) =
    fold fields in order: p_(i+1) = PlaceDesc(t_i,p_i)
PlaceDesc(arr<tau,N>,p) =
    let q_0 = align(p,max(4,A(tau))) + region(tau,N)
    in fold potential indices 0..N-1:
         q_(i+1) = PlaceDesc(tau,q_i)

TreeEnd(tau,b) = PlaceDesc(tau,b+W(tau))
TreeBytes(tau,b) = align(TreeEnd(tau,b),8) - b
```

This formula is a capacity extent, not a source-object footprint equation. Concrete placement is deterministic preorder:

1. lay out the root inline slot/fields;
2. visit product fields in declaration order;
3. when an array occurrence is visited, allocate its immediate object at the next address aligned to `max(4,A(element_type))`;
4. visit potential element indices `0..N-1`, recursively visiting array occurrences in each element type;
5. write offsets only for logical values; leave unused offset slots and unused preallocated descendant bytes zero.

Every logical aggregate occurrence owns one distinct object. Cycles, back-references, offset aliasing, shared descendant objects, and offsets outside the containing ABI region are invalid. Source-local values may alias through products, but the external ABI deliberately has a tree, not a graph.

## 5. Interface identity

The canonical interface text is ASCII:

```text
boundfin-abi-0.1.0\n
export boundfin_entry(<parameter types in order>)-><result type>\n
```

Types use the grammar spellings without whitespace. `interface_id` is SHA-256 of these exact bytes. It is compiled into the module/native artifact and stored in input/output records and manifests.

## 6. Input record

The input region begins at `input_offset` and has an exact statically calculated extent `ABIIn`. Its 48-byte header is:

| Relative bytes | Field |
|---:|---|
| `0..3` | ASCII magic `BFI1`. |
| `4..7` | ABI major/minor packed u32 value `0x00010000`. |
| `8..11` | Exact total input-region bytes, u32. |
| `12..15` | Parameter count, u32. |
| `16..47` | 32-byte `interface_id`. |

The root parameter record begins at `align(input_offset+48,8)`. Parameters are placed like product fields in declaration order, including alignment and zero padding. After all inline parameter roots, descendant objects are placed in parameter order by applying `PlaceDesc(parameter_type,p)` from the common inline-record end. `ABIIn` is the final aligned end minus `input_offset` and must fit u32.

Nullary entries have parameter count zero and no root fields. Every nested header, logical scalar, offset, padding byte, unused capacity byte, and endpoint is validated before source entry.

## 7. Output record and aggregate-result convention

The output region begins at `output_offset`, is disjoint from input/frames/runtime reserve, and has exact extent `ABIOut`. Its 48-byte header is:

| Relative bytes | Field |
|---:|---|
| `0..3` | ASCII magic `BFO1`. |
| `4..7` | ABI packed version `0x00010000`. |
| `8..11` | Exact total output-region bytes, u32. |
| `12..15` | Status word. |
| `16..47` | 32-byte `interface_id`. |

The single result root begins at `align(output_offset+48,8)` and its end is computed by `TreeEnd(result_type,result_root_base)`, rounded to 8. Before invocation the host zeros the entire output region, then writes its magic, version, extent, initial non-success status `4`, and interface identity. Successful execution deep-serializes the complete result tree into caller-owned output storage, leaves all padding/unused capacity bytes zero, writes status zero last, and returns zero.

If a source function returns an input array or a local alias, serialization deep-copies its logical value into the canonical output tree. No output pointer can refer into input, a callee frame, or runtime reserve. A local aggregate constructed directly in output is charged to output placement, not again to the callee frame.

For a declared source error, result payload bytes remain ignored/zero, the output status is written to the declared value, and the same value is returned. For invalid input with a valid writable output region, status `4` is written and returned. If the output offset/extent itself is invalid, the module writes nothing and returns `4`.

## 8. Export contract and status

The scalar module exports exactly:

```text
memory : non-growing WebAssembly 32-bit linear memory
boundfin_entry : (param i32 input_offset, i32 output_offset) (result i32 status)
```

The module has no imports or start function. Fixed status words are:

| Word | Meaning |
|---:|---|
| `0` | Success; output complete. |
| `1` | `Bounds`. |
| `2` | `DivZero`. |
| `3` | `DivOverflow`. |
| `4` | `InvalidABI`, before source entry. |

No other return value is valid. BIR faults, Wasm traps, target exhaustion, host/runtime failure, backend failure, OS termination, and hardware failure are separate out-of-band categories and must never be converted to source status.

## 9. Wasm32 memory partition

For an artifact-specific maximum alignment `Amax=8`, the validated manifest chooses bases and proves:

```text
bI = input_base
eI = align(bI + ABIIn, Amax)
bO = eI
eO = align(bO + ABIOut, Amax)
bF = eO
eF = align(bF + Stack(entry), Amax)
bR = eF
MemReq = align(bR + runtimeReserve, 65536)
```

For every half-open interval `[b,b+z)`, require `b < U`, `b+z <= U`, no natural-number overflow, and the declared disjointness. `MemReq <= U` and configured memory pages must equal `MemReq/65536`; memory growth is absent. The runtime manifest freezes `bI` and `bO`; call arguments must equal them. This preserves a stable invocation boundary while retaining explicit offset arguments.

Frame alignment gaps, ABI internal padding, runtime reserve, and page rounding are included in target memory accounting. Engine operand/call-stack storage outside linear memory is separately reported. Source cumulative reservation and source peak are never substituted for these quantities.

## 10. Validation order

Host validation occurs before the timed call as part of the symmetric invocation boundary. The exported wrapper also checks the two base arguments and record identity defensively. Validation checks, in order:

1. configured memory size and exact input/output bases;
2. region endpoints and input/output separation;
3. magic, ABI version, exact extents, parameter count, and interface identity;
4. root field alignment and scalar encodings;
5. every logical array header and exact declared capacity;
6. every logical nested offset, ownership, alignment, endpoint, and nonalias relation;
7. zero padding, reserved, unused capacity, and unused preallocated descendant bytes.

The first failed ABI check reports its stable `ABIxxx` diagnostic to host evidence and yields status `4` if invocation is attempted. ABI validation emits no source event and source active depth is not established.

## 11. Aliasing and host obligations

- Input and output regions never overlap each other, frames, or runtime reserve.
- Distinct input parameters and logical aggregate occurrences do not alias.
- All absolute offsets point to the canonical owned object inside the same input or output region.
- The host must validate the artifact/configuration/interface hashes before writing memory.
- The host must initialize every required input byte and zero every required canonical padding/unused byte.
- The host must zero/initialize output as specified, call the exact typed export once per primary observation, read status before decoding payload, and consume results symmetrically across Wasm/native.
- The host must not mutate input or output concurrently, retain an escaped pointer, call through an import, grow memory, or decode output on nonzero status.
- The native baseline must expose the same logical `(input_offset,output_offset)->status` boundary over an equivalent byte buffer, even though its process address is wider.
- Raw bytes, normalized observation hashes, interface identity, region extents, and status must be retained in correctness evidence.

## 12. Trap policy

ABI rejection and declared errors are ordinary statuses. Bounds checks precede address formation. Signed division/remainder guards precede trap-prone target operations. Accepted region arithmetic is proved in naturals. Any Wasm trap is therefore an unexpected target failure and blocks G2; standard validation alone does not establish its absence.
