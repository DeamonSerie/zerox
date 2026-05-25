# Zero Functions

## Function Definitions

Functions are defined with `fn`, followed by the return type, name, parameters, and body.

```zero
fn add i32 x i32 y i32
  ret + x y

fn greet Void name String
  ret
```

### Syntax

```
fn <return_type> <name> <param1_type> <param1_name> <param2_type> <param2_name> ...
  <body>
```

The return type comes first, followed by the function name, then alternating parameter types and names.

## Return Values

Use `ret` to return a value:

```zero
fn answer i32
  ret 42
```

For `Void` functions, `ret` with no value is used:

```zero
fn log Void message String
  ret
```

## Fallible Functions

Functions that can fail declare error types with `!` followed by a choice/error type in brackets:

```zero
fn validate i32 ok Bool ![InvalidInput]
  if == ok false
    raise InvalidInput
  ret 42
```

Calling a fallible function requires `check`:

```zero
fn run Void ![IOError]
  check validate true
```

## Public Functions

Use `pub fn` to export a function from a module:

```zero
pub fn main Void world World !
  check world.out.write "hello from zero\n"
```

The `main` function signature is special: it takes a `World` parameter for capability-based I/O and uses `!` for fallibility.

## Function Calls

```zero
answer()                            # no-argument call
add 3 4                             # multi-argument call
std.mem.span data                   # qualified call
T.read value                        # interface dispatch
Counter.add (&counter) 2            # static method call
```

## Static Methods

Functions can be defined inside a type block as static methods:

```zero
type Counter
  value i32

  fn add i32 self ref<Self> amount i32
    ret + self.value amount
```

Called as:

```zero
Counter.add (&counter) 2
```

The `Self` keyword refers to the enclosing type.

## Interface Methods

Interfaces define method signatures that types can implement:

```zero
interface Readable<T: Type>
  fn read i32 self ref<T>
```

Implementation:

```zero
type Counter
  value i32

  fn read i32 self ref<Self>
    ret self.value
```

Interface dispatch:

```zero
fn readValue<T: Readable<T>> i32 value ref<T>
  ret T.read value
```

## Drop (Destructor)

Types can define a `drop` method that runs automatically when an `owned` value goes out of scope:

```zero
type CleanupProbe
  marker MutSpan<u8>

  fn drop Void self mutref<Self>
    set self.marker[0] 1
```

## Generic Functions

Functions can have type and static parameters:

```zero
fn makePair<T: Type, U: Type> Pair<T,U> left T right U
  ret Pair . left left right right

fn first<T: Type, static N: usize> T vec ref<FixedVec<T,N>>
  ret vec.items[0]
```

## Defer

Deferred calls run when the current scope exits:

```zero
if true
  defer cleanup()
  # ... body ...
```

## Extern Functions

Zero can call C functions via `extern` declarations:

```zero
extern c "stdint.h" as cstdint

extern type CConfig
  port i32
  workers i32
```
