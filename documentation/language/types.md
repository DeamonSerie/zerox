# Zero Type System

## Primitive Types

| Type | Description | Size | Literal Example |
|------|-------------|------|-----------------|
| `i32` | Signed 32-bit integer | 4 bytes | `42` |
| `i64` | Signed 64-bit integer | 8 bytes | `42_i64` |
| `u8` | Unsigned 8-bit integer | 1 byte | `42_u8` |
| `u16` | Unsigned 16-bit integer | 2 bytes | `42_u16` |
| `u32` | Unsigned 32-bit integer | 4 bytes | `42_u32` |
| `u64` | Unsigned 64-bit integer | 8 bytes | `42_u64` |
| `usize` | Architecture-dependent unsigned | 4/8 bytes | `42_usize` |
| `Bool` | Boolean | 1 byte | `true`, `false` |
| `String` | Immutable UTF-8 string | 16 bytes (ptr + len) | `"hello"` |
| `Void` | Unit type (no value) | 0 bytes | — |

## Compound Types

### Arrays `[N]T`

Fixed-size array of `N` elements of type `T`.

```zero
let items [4]u8 [0, 0, 0, 0]       # fixed array
let first items[0]                  # indexed access
set items[0] 42                     # mutable indexed write
```

### Slices `Span<T>` / `MutSpan<T>`

A `Span<T>` is a read-only view into a contiguous sequence. A `MutSpan<T>` is a mutable view.

```zero
let span std.mem.span data           # Span<u8> from array
let slice Span<u8> data[0..5]        # slice range
```

### References

| Syntax | Description |
|--------|-------------|
| `ref<T>` | Immutable reference |
| `mutref<T>` | Mutable reference |
| `owned<T>` | Owned reference (unique ownership) |

```zero
fn read ref<T> value T               # immutable reference
fn write mutref<T> value T           # mutable reference
let owned owned<CleanupProbe> ...    # owned allocation
```

## Type Constructors

### Shapes (Struct Types)

Named product types with named fields.

```zero
type Point
  x i32
  y i32
```

Construction uses keyword arguments:

```zero
let point Point . x 40 y 2
```

### Enums

Tagless enumeration of variants.

```zero
enum Status
  ready
  failed
```

### Choices (Tagged Unions)

Union types where each variant carries a payload type.

```zero
choice Result
  ok i32
  err String
```

Construction and matching:

```zero
let result Result Result.ok 42
match result
  ok value
    # use value
  err message
    # use message
```

## Generics

### Type Parameters

```zero
type Pair<T: Type, U: Type>
  left T
  right U
```

### Constrained Generics

```zero
interface Readable<T: Type>
  fn read i32 self ref<T>

fn readValue<T: Readable<T>> i32 value ref<T>
  ret T.read value
```

### Static (Compile-Time) Parameters

Parameters known at compile time, used for array sizes and other compile-time constants.

```zero
type FixedVec<T: Type, static N: usize>
  len usize
  items [N]T
```

Generic function with static params:

```zero
fn first<T: Type, static N: usize> T vec ref<FixedVec<T,N>>
  ret vec.items[0]
```

## Type Aliases

```zero
alias BytePair Pair<u8,u8>
```

## Implicit Type Inference

Zero infers types from context in many cases:

```zero
let value 42                    # inferred as i32
let sum + 40 2                  # inferred from expression
```

Explicit type annotations are needed when inference is ambiguous:

```zero
let value 42_u8                 # explicit suffix disambiguates
let pair Pair<i32,u8> ...       # explicit generic args
```

## C Interop Types

Zero supports importing C types via `extern` declarations:

```zero
extern c "stdint.h" as cstdint

extern type CConfig
  port i32
  workers i32
```

## Ownership Types

| Wrapper | Description |
|---------|-------------|
| `ref<T>` | Borrowed reference (non-owning) |
| `mutref<T>` | Mutable borrowed reference |
| `owned<T>` | Unique owning pointer (runs drop on scope exit) |

Values without `ref`/`mutref`/`owned` are passed by value (copied or moved).
