# ZeroX Math Library — User Guide

## Overview

The `std.math` library provides a comprehensive set of mathematical operations for ZeroX programs. It covers algebraic operations, calculus, matrix manipulations, numerical analysis, and debugging utilities — all written in pure ZeroX `.0` source code with `f64` (double-precision floating-point) as the primary numeric type.

The library is organized into five sub-modules:

| Module         | File                | Purpose                                      |
|----------------|---------------------|----------------------------------------------|
| `std.math`     | `math.0`            | Constants, comparisons, basic utilities      |
| `std.math.algebra` | `algebra.0`     | Polynomials, trig, exp/log, roots, GCD/LCM   |
| `std.math.calculus`| `calculus.0`    | Numerical differentiation and integration    |
| `std.math.matrix`  | `matrix.0`      | Matrix ops, determinant, inverse, LU solve   |
| `std.math.numerical`| `numerical.0`  | Root-finding, interpolation, regression, special functions |
| `std.math.debug`   | `debug.0`       | Computation tracing and debugging            |

---

## Getting Started

Import the desired sub-module at the top of your `.0` file:

```
use std.math
use std.math.algebra
use std.math.calculus
use std.math.matrix
use std.math.numerical
use std.math.debug
```

All public functions are accessed via their fully-qualified path:

```
let x f64 std.math.algebra.sin std.math.PI_2
```

---

## Core Module (`std.math`)

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `PI`     | 3.14159265358979323846 | π |
| `TAU`    | 6.28318530717958647692 | 2π |
| `E`      | 2.71828182845904523536 | e |
| `LN2`    | 0.69314718055994530942 | ln(2) |
| `LN10`   | 2.30258509299404568402 | ln(10) |
| `SQRT2`  | 1.41421356237309504880 | √2 |
| `PHI`    | 1.61803398874989484820 | φ (golden ratio) |
| `PI_2`   | 1.57079632679489661923 | π/2 |
| `PI_4`   | 0.78539816339744830962 | π/4 |
| `DEFAULT_TOLERANCE` | 1e-12 | Default ε for comparisons |
| `LOOSE_TOLERANCE` | 1e-6 | Loose tolerance |
| `EPSILON` | 2.22e-16 | Machine epsilon for f64 |

### Comparison Functions

- `approxEq(a, b, tolerance)` — Check approximate equality within tolerance
- `approxEqDefault(a, b)` — Check with DEFAULT_TOLERANCE
- `isZero(value)` — Check if ≈ 0
- `isZeroTol(value, tolerance)` — Check if ≈ 0 with custom tolerance

### Utility Functions

- `clamp(value, min, max)` — Constrain to [min, max]
- `sign(value)` — Return -1, 0, or 1
- `min(a, b)`, `max(a, b)`, `min3(a, b, c)`, `max3(a, b, c)`
- `lerp(a, b, t)` — Linear interpolation
- `mapRange(value, inMin, inMax, outMin, outMax)`
- `degToRad(degrees)`, `radToDeg(radians)`
- `step(x)`, `smoothStep(x)` — Step functions
- `factorial(n)` — n! as f64
- `abs(x)`, `sq(x)`, `cube(x)`
- `powi(base, exp)` — Integer exponent (exponentiation by squaring)
- `arraySum(values, start, count)` — Sum over Span<f64>

---

## Algebra Module (`std.math.algebra`)

### Polynomial Operations

- `polyEval(coeffs, degree, x)` — Horner's method evaluation
- `polyEvalWithDerivative(coeffs, degree, x)` — Returns `PolyValue { f, df }`
- `polyVal(coeffs, numCoeffs, x)` — Convenience wrapper

### Equation Solving

- `solveQuadratic(a, b, c, roots)` — Real roots of ax² + bx + c = 0
- `solveQuadraticRobust(a, b, c, roots)` — Handles degenerate cases (a=0)
- `solveDepressedCubic(p, q, roots)` — Real roots of x³ + px + q = 0

### Elementary Functions

- `sqrt(x)` — Square root via Newton's method
- `cbrt(x)` — Cube root via Newton's method
- `sin(x)`, `cos(x)`, `tan(x)` — Trigonometric (Taylor series)
- `asin(x)`, `acos(x)`, `atan(x)`, `atan2(y, x)` — Inverse trig
- `exp(x)` — eˣ via Taylor series
- `ln(x)` — Natural log via AGM method
- `log10(x)`, `log2(x)` — Base-10 and base-2 logs
- `pow(a, b)` — Real exponent aᵇ

### Number Theory

- `gcd(a, b)` — Greatest common divisor (i64)
- `lcm(a, b)` — Least common multiple (i64)
- `binomial(n, k)` — Binomial coefficient C(n, k) as f64

### Other

- `agm(a, b)` — Arithmetic-geometric mean

---

## Calculus Module (`std.math.calculus`)

All operations are numerical approximations requiring a function `(fn f64) f64`.

### Differentiation

- `derivative(fn, x, h)` — First derivative (central difference)
- `derivative5(fn, x, h)` — First derivative (5-point stencil)
- `secondDerivative(fn, x, h)` — Second derivative
- `secondDerivative5(fn, x, h)` — Second derivative (5-point stencil)
- `thirdDerivative(fn, x, h)` — Third derivative
- `diffData(y, count, h, out)` — Derivative from sampled data

### Integration

- `integrateTrapezoid(fn, a, b, n)` — Trapezoidal rule
- `integrateSimpson(fn, a, b, n)` — Simpson's rule (n must be even)
- `integrateSimpson38(fn, a, b, n)` — Simpson's 3/8 rule
- `integrateAdaptive(fn, a, b, tolerance, maxDepth)` — Adaptive Simpson
- `integrateRomberg(fn, a, b, order)` — Romberg integration (returns `IntegrateResult`)
- `riemannLeft(fn, a, b, n)`, `riemannRight(fn, a, b, n)`, `riemannMidpoint(fn, a, b, n)`

### Types

- `IntegrateResult` — Contains `value f64` and `errorEstimate f64`

---

## Matrix Module (`std.math.matrix`)

Matrices are stored in row-major order as flat `[M*N]f64` arrays and accessed via `Matrix`/`MatrixMut` view types.

### Types

- `Matrix` — `{ rows u32, cols u32, data Span<f64>, cap usize }`
- `MatrixMut` — `{ rows u32, cols u32, data MutSpan<f64>, cap usize }`
- `LUResult` — `{ ok Bool, pivot u32 }`

### Construction

- `matrixView(rows, cols, data)` — Create Matrix from Span
- `matrixMutView(rows, cols, data)` — Create MatrixMut from MutSpan

### Element Access

- `get(mat, row, col)` — Get element
- `set(mat, row, col, value)` — Set element

### Basic Operations

- `add(a, b, out)` — Element-wise addition
- `sub(a, b, out)` — Element-wise subtraction
- `scale(a, scalar, out)` — Scalar multiplication
- `mul(a, b, out)` — Matrix multiplication
- `transpose(a, out)` — Matrix transpose

### Norms

- `normFrobenius(mat)` — Frobenius norm (√Σaᵢⱼ²)
- `normInf(mat)` — Infinity norm (max row sum)

### Determinant & Inverse

- `det2(a)` — 2×2 determinant
- `det3(a)` — 3×3 determinant
- `inv2(a, out)` — 2×2 inverse
- `inv3(a, out)` — 3×3 inverse

### Linear Systems

- `identity(n, out)` — Fill n×n identity matrix
- `trace(mat)` — Sum of diagonal elements
- `lu(a, lufactor, perm, permCount)` — PA = LU decomposition
- `forwardSub(lufactor, b, y)` — Forward substitution (Ly = b)
- `backSub(lufactor, y, x)` — Back substitution (Ux = y)
- `solve(a, b, x, lufactor, perm, y)` — Solve Ax = b

---

## Numerical Analysis Module (`std.math.numerical`)

### Root Finding

- `bisect(fn, a, b, tol)` — Bisection method
- `newton(fn, dfn, guess, tol, maxIter)` — Newton's method
- `secant(fn, x0, x1, tol, maxIter)` — Secant method

### Interpolation

- `interpolateLinear(xData, yData, count, x)` — Piecewise linear
- `interpolateLagrange(xData, yData, count, x)` — Lagrange polynomial

### Curve Fitting

- `linearRegression(xData, yData, count)` — Returns `LinearFit { a, b, r2 }`
- `polyFit2(xData, yData, count, coeffs)` — Quadratic polynomial fit

### Types

- `LinearFit` — `{ a f64, b f64, r2 f64 }`

### Finite Differences

- `forwardDiff(fn, x, h)` — Forward difference
- `backwardDiff(fn, x, h)` — Backward difference
- `fdCoeffs1(stencil, h, coeffs)` — Finite difference coefficients
- `trapz(y, count, h)` — Trapezoidal integration of discrete data

### Special Functions

- `sinc(x)` — sin(πx)/(πx), with sinc(0)=1
- `erf(x)` — Error function (Abramowitz & Stegun approximation)
- `erfc(x)` — Complementary error function
- `gamma(x)` — Gamma function (Lanczos approximation)
- `beta(x, y)` — Beta function

---

## Debug Module (`std.math.debug`)

The debug module enables step-by-step tracing of mathematical computations without modifying the computation logic.

### Types

- `MathContext` — Debug context with trace log
- `TraceEntry` — `{ label f64, input f64, output f64, residual f64 }`
- `TraceLog` — `{ entries TraceEntry, capacity usize }`

### Functions

- `new(capacity)` — Create a new debug context
- `trace(ctx, label, input, output)` — Record step, returns output
- `traceExplicit(ctx, label, input, output, residual)` — Record with custom residual
- `stepCount(ctx)` — Number of recorded steps
- `hasConverged(ctx, tolerance)` — Check if latest step converged
- `reset(ctx)` — Clear all trace entries
- `debugEval(ctx, label, x, fn)` — Trace function evaluation
- `iterate(ctx, initial, maxIter, tolerance, stepFn)` — Trace iterative computation
- `printTrace(ctx, world)` — Print trace entries to stdout

### Debugging Pattern

The key design principle is that trace functions return their `output` argument unchanged. This allows insertion into expressions without affecting results:

```
# Without debug:
let x f64 / (+ x (/ a x)) 2.0_f64

# With debug (same result, traced):
let x f64 std.math.debug.trace (&mut ctx) (as f64 iter) x (/ (+ x (/ a x)) 2.0_f64)
```

---

## File Format (.0)

All math library modules are written in ZeroX `.0` format following the standard conventions:

- Comments begin with `#`
- Constants use `const name type value`
- Types use `type Name ...`
- Functions use `pub fn name returnType params...`
- Inline tests use `test "description" ... expect ...`
- Mutable parameters use `mutref<Type>`

---

## Examples

The following example files demonstrate each module:

| File | Description |
|------|-------------|
| `examples/math-algebra-demo.0` | Polynomials, trig, roots, logs |
| `examples/math-calculus-demo.0` | Derivatives, integrals, sampled data |
| `examples/math-matrix-demo.0` | Add, multiply, determinant, inverse |
| `examples/math-numerical-demo.0` | Root-finding, interpolation, regression, special functions |
| `examples/math-debug-demo.0` | Tracing Newton iteration, convergence monitoring |

Run with:

```sh
bin/zerox check examples/math-algebra-demo.0
bin/zerox test examples/math-algebra-demo.0
```

---

## Performance Considerations

- All math functions are pure ZeroX implementations — no native runtime helpers required.
- Elementary functions use Taylor series expansions with 15–20 terms for accuracy.
- Matrix multiplication uses the standard O(n³) algorithm with a sparse-entry optimization (skips zero elements in the left matrix).
- LU decomposition uses partial pivoting for numerical stability.
- The debug module stores trace entries in flat arrays with bounded capacity — no runtime allocation on the tracing path.
- For performance-critical code, the trace calls can be conditionally compiled or removed during optimization.

---

## Integration with ZeroX

The library integrates seamlessly with ZeroX:

1. **No external dependencies** — Pure ZeroX code, no C extensions required
2. **Standard import mechanism** — Uses `std.math.*` namespace
3. **Type-safe API** — All functions use ZeroX's static type system
4. **Testable** — Inline `test` blocks verify correctness
5. **Debuggable** — The `debug` module enables step-by-step inspection
6. **Composable** — Functions from different modules can be combined freely
7. **.0 format compliant** — All source files follow the `.0` specification

---

## API Stability

The `std.math` library follows the same API stability conventions as the rest of the ZeroX standard library:
- All public symbols are considered "bootstrap-stable"
- Breaking changes will be documented in the changelog
- The library is designed to evolve as ZeroX's type system and capabilities grow
