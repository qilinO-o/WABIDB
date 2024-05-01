## Command Structure
The commands are of the form:
```shell
wabidb-inspect INFILE [-options [option-value]]
```

You can display help for each commands by `--help` or `-h` flag.

## Getting started
Let's try to debug a WebAssembly binary.

`wabidb-inspect` loads the given binary  and print it out.

```shell
$ wabidb-inspect fib.wasm "-cmd=wasmtime --invoke fib 1.wasm 8"
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
 > locals(l) | globals(g)
 > l
```

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