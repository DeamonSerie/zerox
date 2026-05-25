# Zero Compile-Time Features

Zero supports compile-time evaluation through constants and static parameters. These features enable generic programming, fixed-size type definitions, and compile-time computation.

## Constants

Constants are named values computed at compile time:

```zero
const base i32 40
const answer i32 + base 2
```

### Uses

- Named magic numbers
- Reusable configuration values
- Compile-time arithmetic

### Restrictions

Compile-time evaluation runs in a sandboxed environment:
- No file system access
- No network access
- No arbitrary code execution (only compile-time expressions)
- Time-bounded execution

## Static Parameters

Static parameters are compile-time known values used in generic type and function definitions. They enable type-level programming with concrete values.

### Type-Level Static Parameters

```zero
type FixedVec<T: Type, static N: usize>
  len usize
  items [N]T
```

Here `N` is known at compile time, which allows the array `[N]T` to be sized correctly.

### Function-Level Static Parameters

```zero
fn first<T: Type, static N: usize> T vec ref<FixedVec<T,N>>
  ret vec.items[0]
```

### Calling with Static Parameters

```zero
let first first<u8, 4> (&vec)     # explicit static args
```

### Static Value Expressions

Static parameters can be initialized from compile-time expressions:

```zero
const default_cap usize + 2 2

fn first<T: Type, static N: usize> T vec ref<FixedVec<T,N>>
  ret vec.items[0]

pub fn main Void world World !
  let vec FixedVec<u8,default_cap> FixedVec . len default_cap items ([1, 2, 3, 4])
  if == (first<u8, default_cap> (&vec)) 1
    check world.out.write "static value params ok\n"
```

## Generic Specialization

The compiler specializes generic functions and types at compile time. Each unique combination of type and static parameters generates separate code.

### Monomorphization

When you use `Vec<i32>` and `Vec<String>`, the compiler generates two separate type definitions with the appropriate field sizes.

### Interface Dispatch

Interfaces with `ref<Self>` parameters enable compile-time dispatch without runtime overhead:

```zero
interface Readable<T: Type>
  fn read i32 self ref<T>

fn readValue<T: Readable<T>> i32 value ref<T>
  ret T.read value         # resolved at compile time
```

## Compile-Time Sandbox

The compile-time evaluator runs in a restricted environment:

| Operation | Allowed |
|-----------|---------|
| Integer arithmetic | Yes |
| Comparison | Yes |
| Logical operations | Yes |
| Array access | Yes |
| Function calls (same module) | Yes |
| Generic specialization | Yes |
| File I/O | No |
| Network calls | No |
| External process calls | No |
| System calls | No |
| Timeouts | Hard limit enforced |

### Sandbox Violations

If compile-time evaluation attempts a disallowed operation, the compiler reports a diagnostic with the operation and location.

## Size-Level Programming

Static parameters enable size-level programming patterns:

```zero
# Fixed-size buffer type
const BUF_SIZE usize 256

type FixedBuf<T: Type>
  data [BUF_SIZE]T
  len usize

# Compile-time sized operations
fn fill<T: Type, static N: usize> Void buf mutref<[N]T> value T
  for i in 0..N
    set buf[i] value

# Sized byte views
type Header
  magic [4]u8
  version u32
  flags u16
```

## Compile-Time Facts

The compiler exposes compile-time facts through `--json` output:

```bash
zero check --json examples/const-arithmetic.0
```

The output includes:
- Whether compile-time code was executed
- What operations were performed
- Any sandbox violations
- Evaluation timing
