# Zero Syntax Overview

Zero uses a whitespace-sensitive syntax inspired by Python and functional languages. Indentation (spaces or tabs) determines block structure.

## File Extension

- `.0` — Zero source files
- `.row` — Row syntax files (an intermediate parseable format)

## Comments

```zero
# This is a single-line comment
```

## Literals

### Integers

```zero
42          # i32 (default)
42_i64      # explicit i64 suffix
0xFF        # hex
0b1010      # binary
0o77        # octal
```

### Unsigned Integers

```zero
42_u8       # u8
42_u16      # u16
42_u32      # u32
42_u64      # u64
42_usize    # usize (architecture-dependent)
```

### Booleans

```zero
true
false
```

### Strings

```zero
"hello from zero\n"   # String literal
```

### Byte Arrays

```zero
[0_u8, 1_u8, 2_u8]         # explicit array
[0_u8; 64]                  # repeat expression (64 zeros)
```

## Expressions

### Arithmetic

```zero
+ 40 2          # prefix addition: 42
- 10 3          # subtraction: 7
* 6 7           # multiplication: 42
/ 10 3          # integer division: 3
% 10 3          # modulo: 1
```

### Comparison

```zero
== 42 42        # equality
!= 5 6          # inequality
< 5 10          # less than
> 10 5          # greater than
<= 5 5          # less or equal
>= 5 5          # greater or equal
```

### Logical

```zero
&& true false   # logical AND
|| true false   # logical OR
! true          # logical NOT (prefix)
```

## Statements

### Let Bindings

```zero
let value 42                              # immutable binding
let message "hello"                       # String binding
let sum + 40 2                            # expression binding
let point Point . x 40 y 2               # shape construction
let pair Pair<i32,u8> makePair 40 2_u8    # generic function call
```

### Mutable Bindings

```zero
mut counter 0                             # mutable binding
set counter + counter 1                   # mutation
set vec.items[0] 42                       # indexed mutation
```

### Function Calls

```zero
answer()                    # no arguments
+ 40 2                      # prefix operator (function call)
add 3 4                     # multiple arguments
std.mem.span message        # qualified function call
```

### If/Else

```zero
if == answer 42
  check world.out.write "yes\n"
else
  check world.out.write "no\n"
```

### Match (Pattern Matching)

```zero
match result
  ok value
    check world.out.write "ok"
  err message
    check world.out.write "err: " message
```

### Defer

```zero
defer cleanup()             # run on scope exit
defer mark_defer marker     # deferred function call
```

### Check (Fallible Context)

```zero
check validate true         # propagate errors
check world.out.write "ok"  # fallible I/O
```

### Return

```zero
ret 42                      # return with value
ret                         # return from Void function
```

## Function Definitions

```zero
fn add i32 x i32 y i32
  ret + x y
```

### Public Functions

```zero
pub fn main Void world World !
  check world.out.write "hello\n"
```

The `main` function takes a `World` parameter (providing capability-based I/O) and uses `!` to indicate fallibility.

### Fallible Functions

```zero
fn validate i32 ok Bool ![InvalidInput]
  if == ok false
    raise InvalidInput
  ret 42
```

## Type Definitions

### Shapes (Struct-like)

```zero
type Point
  x i32
  y i32
```

### Shape Construction

```zero
let point Point . x 40 y 2
```

### Enums

```zero
enum Status
  ready
  failed
```

### Choices (Tagged Unions)

```zero
choice Result
  ok i32
  err String
```

### Type Aliases

```zero
alias BytePair Pair<u8,u8>
```

### Generic Types

```zero
type Pair<T: Type, U: Type>
  left T
  right U
```

### Static Parameters (Compile-Time)

```zero
type FixedVec<T: Type, static N: usize>
  len usize
  items [N]T
```

## Interface and Method Definitions

### Interfaces

```zero
interface Readable<T: Type>
  fn read i32 self ref<T>
```

### Static Methods

```zero
type Counter
  value i32

  fn add i32 self ref<Self> amount i32
    ret + self.value amount
```

### Implementing Interfaces

```zero
type Counter
  value i32

  fn read i32 self ref<Self>
    ret self.value

fn readValue<T: Readable<T>> i32 value ref<T>
  ret T.read value
```

## Drop (Destructor)

```zero
type CleanupProbe
  marker MutSpan<u8>

  fn drop Void self mutref<Self>
    set self.marker[0] 1
```

## Visibility

```zero
pub fn greet Void ...     # public (exported)
fn internal Void ...       # private (module-local)
pub type Config ...        # public type
```
