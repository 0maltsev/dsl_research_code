# Numeric semantics

Normative version: `boundfin-numeric-0.1.0`

## Values and literals

`bool` has exactly the values false and true. `i32` and `i64` are respectively 32-bit and 64-bit words; signed interpretation is two's-complement only where a rule says signed. `f64` is a 64-bit IEEE 754 binary64 word interpreted by the scalar WebAssembly operation named below.

Bit literals denote their words exactly:

- `i32bits(0xHHHHHHHH)` is one `i32` word.
- `i64bits(0xHHHHHHHHHHHHHHHH)` is one `i64` word.
- `f64bits(0xHHHHHHHHHHHHHHHH)` is one `f64` word.

Hexadecimal digit case has no semantic effect. There are no decimal integer value literals, decimal floating literals, implicit conversions, or source-level casts. Decimal numerals occur only as capacities and projection indices.

All operand expressions are evaluated left-to-right. A failed operand stops evaluation before later operands and before the primitive event.

## Integer operations

For width `w` in `{32,64}`, let `word_w(n) = n mod 2^w`, and let `signed_w(x)` be the unique mathematical integer in `[-2^(w-1), 2^(w-1)-1]` represented by word `x`.

| Surface form | Operand/result type | Function |
|---|---|---|
| `-x` | `iw -> iw` | `word_w(-signed_w(x))`; never errors. |
| `x + y` | `iw,iw -> iw` | `word_w(signed_w(x)+signed_w(y))`; never errors. |
| `x - y` | `iw,iw -> iw` | `word_w(signed_w(x)-signed_w(y))`; never errors. |
| `x * y` | `iw,iw -> iw` | `word_w(signed_w(x)*signed_w(y))`; never errors. |
| `x / y` | `iw,iw -> iw or error` | `DivZero` if `signed_w(y)=0`; `DivOverflow` if `signed_w(x)=-2^(w-1)` and `signed_w(y)=-1`; otherwise quotient truncated toward zero, encoded with `word_w`. |
| `x % y` | `iw,iw -> iw or error` | The same two error cases as division; otherwise `x - trunc(x/y)*y`, encoded with `word_w`. |
| `x == y`, `x != y` | `iw,iw -> bool` | Word equality or its negation. |
| `<`, `<=`, `>`, `>=` | `iw,iw -> bool` | Mathematical comparison of `signed_w` values. |

Here `iw` is either `i32` or `i64`; operands must have the same type. Modular add/subtract/multiply/negation are source behavior, not undefined behavior. A C++ oracle must implement them with unsigned fixed-width arithmetic and explicit bit reinterpretation. It must guard division and remainder before invoking an operation that could be undefined or trap.

The count fragment is narrower than `i32`. A count result is interpreted by `signed_32` and accepted only with a checked finite certificate proving it lies in `[0,2^31-1]`. Modular wrap never proves a size fact.

## Boolean operations

| Surface form | Function |
|---|---|
| `!x` | Boolean negation. |
| `x && y` | Strict conjunction after both operands successfully evaluate left-to-right. It is not short-circuiting. |
| `x || y` | Strict disjunction after both operands successfully evaluate left-to-right. It is not short-circuiting. |
| `x == y`, `x != y` | Boolean equality or inequality. |

Branching exists only through `if`. Consequently `&&` and `||` add one primitive event, not a decision event.

## Binary64 operations

The source floating environment is the WebAssembly scalar binary64 environment: round to nearest, ties to even; no excess precision; no contraction; no reassociation; no reciprocal approximation; no relaxed SIMD semantics; and no ambient host rounding-mode dependence.

Let `qNaN = 0x7ff8000000000000`. Let `can(z)` return `qNaN` when word `z` encodes any quiet or signaling NaN and return `z` otherwise. Each arithmetic/unary primitive is the mathematical function:

```text
source_fop(arguments) = can(wasm_scalar_fop(argument_words))
```

Canonicalization occurs after every primitive, before its result can be used by another source expression. Literal and ABI input words are not canonicalized merely by being read.

| Surface form | Wasm scalar operation followed by `can` |
|---|---|
| `-x` | `f64.neg` |
| `abs(x)` | `f64.abs` |
| `x + y` | `f64.add` |
| `x - y` | `f64.sub` |
| `x * y` | `f64.mul` |
| `x / y` | `f64.div` |

Floating division by zero follows WebAssembly/IEEE behavior and is not `DivZero`. Overflow, underflow, infinities, subnormals, and signed zero likewise follow the named operation. No floating primitive raises a declared source error.

Comparisons use the exact scalar Wasm Boolean result and do not canonicalize operands first:

| Surface form | Wasm operation |
|---|---|
| `==`, `!=` | `f64.eq`, `f64.ne` |
| `<`, `<=`, `>`, `>=` | `f64.lt`, `f64.le`, `f64.gt`, `f64.ge` |

Therefore equality is false and inequality true when either operand is NaN; ordered comparisons are false when unordered. `+0.0` and `-0.0` compare equal, while their result words remain observably distinct outside comparison.

The language has no source `sqrt`, min/max, FMA, `pow`, transcendental, conversion, general bitwise, shift, or SIMD operation.

## Primitive identity and events

After operator elaboration, every operator has one typed primitive identifier. A primitive is invoked only after all operands succeed. Success and a declared integer division/remainder error each emit exactly one primitive action. An operand error emits no primitive action for the enclosing operator.

The primitive table version and selected identifier must be stored on every typed operator node and in compiler certificates. Overload resolution requires exact operand types; it never inserts conversions.

## Observable equality

For source values and correctness comparisons:

- `bool`, `i32`, and `i64` words are equal exactly.
- Non-NaN `f64` words are equal bit-for-bit, including the sign of zero.
- Any two NaN encodings are observationally equal.
- Products compare fieldwise in order.
- Arrays compare runtime length and each logical element recursively; capacity and unused bytes are not result values, although capacity must match the declared type.
- Declared errors compare by exact error name.

Reference and native-oracle output is expected to contain canonical primitive-produced NaNs. Scalar Wasm output may contain any permitted NaN encoding and is compared through the quotient. Hash records containing successful floating output must retain both the raw-byte hash and the normalized-observation hash.

## Native and target obligations

- The scalar Wasm lowerer uses only matching fixed-width scalar instructions and explicit guards for signed division/remainder error cases.
- The native oracle avoids signed-overflow undefined behavior and uses operations/flags that preserve the specified binary64 tree.
- Fast-math, unsafe reassociation, contraction, excess precision, and relaxed floating operations are forbidden in every correctness or confirmatory build.
- Toolchain-specific flags are not frozen here; they remain `DEC-004` and require bit-class conformance plus disassembly evidence.

