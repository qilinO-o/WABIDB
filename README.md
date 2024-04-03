# WABIDB

`WABIDB` is an instrumentation and debug framework for WebAssembly Binary. The repository uses `Binaryen` as a parse backend in order to be up-to-date with the latest WASM standards and proposals.

## Building
`WABIDB` uses git submodules, so before you build you will have to initialize the submodules:
```shell
git submodule init
git submodule update
```
After that you can build with CMake:
```shell
mkdir build && cd build
cmake .. && make
```

## Usage Instructions

`WABIDB` uses `class instrumenter` internally, which is also potentially useful for individual usage in other projects. You only need to import `instrumenter.hpp` for basic functionality. It provides a general wasm binary instrumentation tool that is easy to use and powerful as well.

The `routine()` method below covers basic usage of the APIs:
```cpp
void routine() {
    InstrumentConfig config;
    config.filename = "./test.wasm";
    config.targetname = "./test_instr.wasm";
    // example: insert an i32.const(0) and a drop before and after every call
    InstrumentOperation op1;
    op1.targets.push_back(InstrumentOperation::ExpName{
      wasm::Expression::Id::CallId, 
      InstrumentOperation::ExpName::ExpOp{.no_op=-1}
      });
    op1.pre_instructions = {
        "i32.const 0",
        "drop"
    };
    op1.post_instructions = {
        "i32.const 0",
        "drop"
    };
    config.operations.push_back(op1);
    Instrumenter instrumenter;
    instrumenter.setConfig(config);
    InstrumentResult result = instrumenter.instrument();
}
```

## API
### General Instrumentation
The config of general instrumentation is designed for a match-and-insert semantics. It finds expressions in functions that match vec<target> one by one, and insert certain instructions before and after the specific expression.
```cpp
op.targets.push_back(InstrumentOperation::ExpName{
  wasm::Expression::Id::[XXXId], 
  InstrumentOperation::ExpName::ExpOp{[XXXOp]}
  });
op.[POSITION]_instructions = {
  "[instruction 1]",
  "[instruction 2]",
  ...
};
```
General instrumentation can be done by `setConfig()` for an `Instrumenter` and then call the `instrument()`.

### Add Import
```cpp

```

### Add Export
```cpp

```

### Find Operations
```cpp

```

### Instruction Info
Default support WebAssembly version 1 (MVP), can be easily set for further features. Using `Binaryen` enables `WABIDB` to support most up-to-date proposals. 

Now use an experimental WAT-parser of `Binaryen`, which may be switched when [`issue#6208`](https://github.com/WebAssembly/binaryen/issues/6208) is fully resolved.
