# Zero Memory Model

Zero uses an ownership-based memory model with borrowing, deferred cleanup, and controlled mutability.

## Ownership

Every value has a single owner at any time. When the owner goes out of scope, the value is dropped (cleaned up).

### Owned Values

The `owned<T>` wrapper creates an owned reference:

```zero
let probe owned<CleanupProbe> CleanupProbe . marker marker
```

When `probe` goes out of scope, `CleanupProbe.drop` is called automatically.

### Value Semantics (Default)

Most values are passed by value (copied or moved):

```zero
fn add i32 x i32 y i32       # x and y are copied in
  ret + x y
```

## Borrowing

Borrowing allows temporary access without transferring ownership.

### Immutable References (`ref<T>`)

```zero
fn read<T> T value ref<T>
  # can read but not modify
```

### Mutable References (`mutref<T>`)

```zero
fn reset Void value mutref<Self>
  set value.marker[0] 1
```

## Mutability

Variables must be explicitly declared mutable:

```zero
mut counter 0                    # mutable binding
set counter + counter 1          # update with set
set items[0] 42                  # indexed mutation
```

## Defer

The `defer` statement schedules a call to run when the current scope exits:

```zero
fn run Void
  if true
    defer world.out.write "cleanup\n"
    # ... body ...
```

Deferred calls run in reverse order of declaration (LIFO), even if the scope exits early.

## Drop (Destructors)

Types can define cleanup logic with a `drop` method:

```zero
type FileHandle
  fd i32

  fn drop Void self mutref<Self>
    std.fs.close self.fd
```

Drop is called automatically when an `owned` value goes out of scope.

## Arrays and Spans

### Fixed Arrays

```zero
let buf [256]u8 [0_u8; 256]     # 256-element zero-initialized array
```

### Spans (Slices)

`Span<T>` provides a read-only view:

```zero
let data Span<u8> buf            # create span from array
let slice Span<u8> buf[0..10]    # sub-slice
```

`MutSpan<T>` provides a mutable view:

```zero
let mutable MutSpan<u8> buf      # mutable view
set mutable[0] 42                # write through span
```

## Allocation

### Stack Allocation (Default)

All values are stack-allocated by default. No implicit heap allocation.

### Fixed Allocators

The standard library provides fixed-size allocators:

```zero
let alloc std.mem.fixedAllocator<256>()
```

### Bump Allocators

```zero
let alloc std.mem.bumpAllocator<1024>()
```

## Memory Safety Guarantees

1. **Single ownership** — each value has exactly one owner
2. **No dangling references** — borrows cannot outlive their source
3. **Deterministic cleanup** — `drop` runs at scope exit via ownership
4. **Bounds checking** — array and span access is bounds-checked
5. **No null pointers** — use `Maybe<T>` for optional values

## `Maybe<T>` Type

Used for operations that may fail without raising an error:

```zero
let maybe std.mem.get items 0
if maybe.has
  # use maybe.value
else
  # handle missing
```
