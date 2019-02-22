# Conversion to the LLVM IR Dialect

Conversion to the [LLVM IR Dialect](Dialects/LLVM.md) can be performed by the
specialized dialect conversion pass by running

```sh
mlir-opt -convert-to-llvmir <filename.mlir>
```

It performs type and operation conversions for a subset of operations from
standard, built-in and super-vector dialects as described in this document. We
use the terminology defined by the
[LLVM IR Dialect description](Dialects/LLVM.md) throughout this document.

[TOC]

## Type Conversion

### Scalar Types

Scalar types are converted to their LLVM counterparts if they exist. The
following conversions are currently implemented.

-   `i*` converts to `!llvm.type<"i*">`
-   `f16` converts to `!llvm.type<"half">`
-   `f32` converts to `!llvm.type<"float">`
-   `f64` converts to `!llvm.type<"double">`

Note: `bf16` type is not supported by LLVM IR and cannot be converted.

### Index Type

Index type is converted to a wrapped LLVM IR integer with bitwidth equal to the
bitwidth of the pointer size as specified by the
[data layout](https://llvm.org/docs/LangRef.html#data-layout) of the LLVM module
[contained](Dialects/LLVM.md#context-and-module-association) in the LLVM Dialect
object. For example, on x86-64 CPUs it converts to `!llvm.type<"i64">`.

### Vector Types

LLVM IR only supports *one-dimensional* vectors, unlike MLIR where vectors can
be multi-dimensional. MLIR vectors are converted to LLVM IR vectors of the same
size with element type converted using these conversion rules. Vector types
cannot be nested in either IR.

For example, `vector<4 x f32>` converts to `!llvm.type<"<4 x float>">`.

### Memref Types

Memref types in MLIR have both static and dynamic information associated with
them. The dynamic information comprises the buffer pointer as well as sizes of
any dynamically sized dimensions. They are converted into LLVM IR structure
types. The first element of the structure is a pointer to the memref's element
type converted using these rules. Follow as many elements as memref has dynamic
sizes, all of the MLIR `index` type converted using these rules.

Examples:

```mlir {.mlir}
// All of the following are converted to a single-element structure because
// of fully static sizes.
memref<f32>
memref<1 x f32>
memref<10x42x42x43x123 x f32>
// resulting type
!llvm.type<"{float*}">

// All of the following are converted to a three-element structure
memref<?x? x f32>
memref<42x?x10x35x1x? x f32>
// resulting type assuming 64-bit pointers
!llvm.type<"{float*, i64, i64}">

// Memref types can have vectors as element types
memref<1x? x vector<4xf32>>
// which get converted as well
!llvm.type<"{<4 x float>*, i64}">
```

### Function Types {#function-types}

Function types get converted to LLVM function types. The arguments are converted
individually according to these rules. The result types need to accommodate the
fact that LLVM IR functions always have a return type, which may be a Void type.
The converted function always has a single result type. If the original function
type had no results, the converted function will have one result of the wrapped
`void` type. If the original function type had one result, the converted
function will have one result converted using these rules. Otherwise, the result
type will be a wrapped LLVM IR structure type where each element of the
structure corresponds to one of the results of the original function, converted
using these rules. In high-order functions, function-typed arguments and results
are converted to a wrapped LLVM IR function pointer type (since LLVM IR does not
allow passing functions to functions without indirection) with the pointee type
converted using these rules.

Examples:

```mlir {.mlir}
// zero-ary function type with no results.
() -> ()
// is converted to a zero-ary function with `void` result
!llvm.type<"void ()">

// unary function with one result
(i32) -> (i64)
// has its argument and result type converted, before creating the LLVM IR function type
!llvm.type<"i64 (i32)">

// binary function with one result
(i32, f32) -> (i64)
// has its arguments handled separately
!llvm.type<"i64 (i32, float)">

// binary function with two results
(i32, f32) -> (i64, f64)
// has its result aggregated into a structure type
!llvm.type<"{i64, double} (i32, f32)">

// function-typed arguments or results in higher-order functions
(() -> ()) -> (() -> ())
// are converted into pointers to functions
!llvm.type<"void ()* (void ()*)">
```

## Calling Convention

### Function Signature Conversion

MLIR function type is built into the representation, even the functions in
dialects including a first-class function type must have the built-in MLIR
function type. During the conversion to LLVM IR, function signatures are
converted as follows:

-   the outer type remains the built-in MLIR function;
-   function arguments are converted individually following these rules;
-   function results:
    -   zero-result functions remain zero-result;
    -   single-result functions have their result type converted according to
        these rules;
    -   multi-result functions have a single result type of the wrapped LLVM IR
        structure type with elements corresponding to the converted original
        results.

Rationale: function definitions remain analyzable within MLIR without having to
abstract away the function type. In order to remain consistent with the regular
MLIR functions, we do not introduce a `void` result type since we cannot create
a value of `void` type that MLIR passes might expect to be returned from a
function.

Examples:

```mlir {.mlir}
// zero-ary function type with no results.
func @foo() -> ()
// remains as is
func @foo() -> ()

// unary function with one result
func @bar(i32) -> (i64)
// has its argument and result type converted
func @bar(!llvm.type<"i32">) -> !llvm.type<"i64">

// binary function with one result
func @baz(i32, f32) -> (i64)
// has its arguments handled separately
func @baz(!llvm.type<"i32">, !llvm.type<"float">) -> !llvm.type<"i64">

// binary function with two results
func @qux(i32, f32) -> (i64, f64)
// has its result aggregated into a structure type
func @qux(!llvm.type<"i32">, !llvm.type<"float">) -> !llvm.type<"{i64, double}">

// function-typed arguments or results in higher-order functions
func @quux(() -> ()) -> (() -> ())
// are converted into pointers to functions
func @quux(!llvm.type<"void ()*">) -> !llvm.type<"void ()*">
// the call flow is handled by the LLVM dialect `call` instruction supporting both
// direct and indirect calls
```

### Result Packing

In case of multi-result functions, the returned values are inserted into a
structure-typed value before being returned and extracted from it at the call
site. This transformation is a part of the conversion and is transparent to the
defines and uses of the values being returned.

Example:

```mlir {.mlir}
func @foo(%arg0: i32, %arg1: i64) -> (i32, i64) {
  return %arg0, %arg1 : i32, i64
}
func @bar() {
  %0 = constant 42 : i32
  %1 = constant 17 : i64
  %2 = call @foo(%0, %1) : (i32, i64) -> (i32, i64)
  "use_i32"(%2#0) : (i32) -> ()
  "use_i64"(%2#1) : (i64) -> ()
}

// is transformed into

func @foo(%arg0: !llvm.type<"i32">, %arg1: !llvm.type<"i64">) -> !llvm.type<"{i32, i64}"> {
  // insert the vales into a structure
  %0 = "llvm.undef"() : () -> !llvm.type<"{i32, i64}">
  %1 = "llvm.insertvalue"(%arg0, %0) {position: [0]} : (!llvm.type<"i32">, !llvm.type<"{i32, i64}">) -> !llvm.type<"{i32, i64}">
  %2 = "llvm.insertvalue"(%arg1, %1) {position: [1]} : (!llvm.type<"i64">, !llvm.type<"{i32, i64}">) -> !llvm.type<"{i32, i64}">

  // return the structure value
  "llvm.return"(%2) : !llvm.type<"{i32, i64}"> -> ()
}
func @bar() {
  %0 = "llvm.constant" {value: 42} : !llvm.type<"i32">
  %1 = "llvm.constant" {value: 17} : !llvm.type<"i64">

  // call and extract the values from the structure
  %2 = "llvm.call"(%0, %1) {callee: @bar} : (%arg0: !llvm.type<"i32">, %arg1: !llvm.type<"i64">) -> !llvm.type<"{i32, i64}">
  %3 = "llvm.extractvalue"(%2) {position: [0]} :  (!llvm.type<"{i32, i64}"> -> !llvm.type<"i32">
  %4 = "llvm.extractvalue"(%2) {position: [1]} :  (!llvm.type<"{i32, i64}"> -> !llvm.type<"i64">

  // use as before
  "use_i32"(%3) : (!llvm.type<"i32">) -> ()
  "use_i64"(%4) : (!llvm.type<"i64">) -> ()
}
```

## Memref Model

### Memref Descriptor

Within a converted function, a `memref`-typed value is represented by a memref
_descriptor_, the type of which is the structure type obtained by converting
from the memref type. This descriptor holds a pointer to a linear buffer storing
the data, and dynamic sizes of the memref value. It is created by the allocation
operation and is updated by the conversion operations that may change static
dimensions into dynamic and vice versa.

Note: LLVM IR conversion does not support `memref`s in non-default memory spaces
or `memref`s with non-identity layouts.

### Index Linearization

Accesses to a memref element are transformed into an access to an element of the
buffer pointed to by the descriptor. The position of the element in the buffer
is calculated by linearizing memref indices in row-major order (lexically first
index is the slowest varying, similar to C). The computation of the linear
address is emitted as arithmetic instructions in the LLVM IR dialect. Static
sizes are introduced as constants. Dynamic sizes are extracted from the memref
descriptor.

Examples:

```mlir {.mlir}
%0 = load %m[1,2,3,4] : memref<10x?x13x?xf32>
```

is transformed into the equivalent of the following code:

```mlir {.mlir}
// obtain the buffer pointer
%b = "llvm.extractvalue"(%m) {position: [0]} : (!llvm.type<"{float*, i64, i64}">) -> !llvm.type<"float*">

// obtain the components for the index
%sub1 = "llvm.constant" {value: 1} : () -> !llvm.type<"i64">  // first subscript
%sz2 = "llvm.extractvalue"(%m) {position: [1]}
    : (!llvm.type<"{float*, i64, i64}">) -> !llvm.type<"float*"> // second size (dynamic, second descriptor element)
%sub2 = "llvm.constant" {value: 2} : () -> !llvm.type<"i64">  // second subscript
%sz3 = "llvm.constant" {value: 13} : () -> !llvm.type<"i64">  // third size (static)
%sub3 = "llvm.constant" {value: 3} : () -> !llvm.type<"i64">  // third subscript
%sz4 = "llvm.extractvalue"(%m) {position: [1]}
    : (!llvm.type<"{float*, i64, i64}">) -> !llvm.type<"float*"> // fourth size (dynamic, third descriptor element)
%sub4 = "llvm.constant" {value: 4} : () -> !llvm.type<"i64">  // fourth subscript

// compute the linearized index
// %sub4 + %sub3 * %sz4 + %sub2 * (%sz3 * %sz4) + %sub1 * (%sz2 * %sz3 * %sz4) =
// = ((%sub1 * %sz2 + %sub2) * %sz3 + %sub3) * %sz4 + %sub4
%idx0 = "llvm.mul"(%sub1, %sz2) : (!llvm.type<"i64">, !llvm.type<"i64">) -> !llvm.type<"i64">
%idx1 = "llvm.add"(%idx0, %sub2) : (!llvm.type<"i64">, !llvm.type<"i64">) -> !llvm.type<"i64">
%idx2 = "llvm.mul"(%idx1, %sz3) : (!llvm.type<"i64">, !llvm.type<"i64">) -> !llvm.type<"i64">
%idx3 = "llvm.add"(%idx2, %sub3) : (!llvm.type<"i64">, !llvm.type<"i64">) -> !llvm.type<"i64">
%idx4 = "llvm.mul"(%idx3, %sz4) : (!llvm.type<"i64">, !llvm.type<"i64">) -> !llvm.type<"i64">
%idx5 = "llvm.add"(%idx4, %sub4) : (!llvm.type<"i64">, !llvm.type<"i64">) -> !llvm.type<"i64">

// obtain the element address
%a = "llvm.getelementptr"(%b, %idx5) : (!llvm.type<"float*">, !llvm.type<"i64">) -> !llvm.type<"float*">

// perform the actual load
%0 = "llvm.load"(%a) : (!llvm.type<"float*">) -> !llvm.type<"float">
```

In practice, the subscript and size extraction will be interleaved with the
linear index computation. For stores, the address computation code is identical
and only the actual store operation is different.

Note: the conversion does not perform any sort of common subexpression
elimination when emitting memref accesses.