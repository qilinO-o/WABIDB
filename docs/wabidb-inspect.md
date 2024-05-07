## Command Structure
The commands are of the form:
```shell
wabidb-inspect INFILE [-options [option-value]]
```

You can display help for each commands by `--help` or `-h` flag.

## Getting started
Let's try to debug a WebAssembly binary.

`wabidb-inspect` loads the given binary and print it out.

```shell
$ wabidb-inspect fib.wasm "-cmd=wasmtime --invoke fib fib.wasm 8"
(wabidb-inspect) Listing
   (module
    (export "fib" (func $0))
    (func $0 (param $0 i32) (result i32)
     (local $1 i32)
     (local $2 i32)
 1   i32.const 1
 2   local.set $1
     ...
    )
    ...
   )
```
The example is a simple fibonacci wasm without start function. `wabidb-inspect` allows you to fully use standalone runtime functionalities by option flag `--command` or `-cmd`. Such as `wasmtime`'s `--invoke` option.

### Choosing an inspection
You need to specify the location and type of inspection.

```shell
(wabidb-inspect) Enter inspect position
 > func: 0
 > line: 2
(wabidb-inspect) Enter inspect command
 > locals(l) | globals(g) | backtrace(bt)
 > l
```
Note that backtrace is a rather experimental function. Wasm `table` is dynamically load at runtime, so our backtrace does not support `call_indirect` as it relies on static instrumentation.

### Examining inspection result
`wabidb-inspect` instruments the binary based on previews choice and runs your `-cmd` argument. If the binary is interactive, just interact with it as you like. It will stop at the inspection point. Then inspection result shows up.

```shell
(wabidb-inspect) Instrumenting ...
(wabidb-inspect) Write instrumented file to: fib-inspect.wasm
(wabidb-inspect) Executing with: "wasmtime --dir=. --invoke fib fib-inspect.wasm 8" ...
(wabidb-inspect) Locals:
 0: param name: $0 = i32(8)
 1: var   name: $1 = i32(1)
 2: var   name: $2 = i32(0)
```

### Continuing
You can continue inspecting or quit the tool. Note that the binary is **NOT** continuously executed! Instead, it is instrumented and executed again.

```shell
(wabidb-inspect) continue(c) | quit(q)
 > c
(wabidb-inspect) Enter inspect position
 > func:
 ...
(wabidb-inspect) Enter inspect command
 > locals(l) | globals(g) | backtrace(bt)
 > g
 ...
(wabidb-inspect) Globals:
 0: name: $global$0 = i32(1000)
 1: name: $global$1 = i64(2000)
 2: name: $global$2 = f32(0.233300)
 3: name: $global$3 = f64(0.466600)
 4: name: $global$4 = v128(0 0 0 0 0 0 0 0 0x43 0 0 0 0x21 0 0 0)
(wabidb-inspect) continue(c) | quit(q)
 > c
(wabidb-inspect) Enter inspect position
 > func:
 ...
(wabidb-inspect) Enter inspect command
 > locals(l) | globals(g) | backtrace(bt)
 > bt
 ...
(wabidb-inspect) Backtrace:
 0: $1
 1: $1
 2: $1
 3: $1
 4: $1
 5: $_start (or what runtime directly call)
```