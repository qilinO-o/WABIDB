# [WIP]WABIDB

`WABIDB` is an instrumentation and debug framework for WebAssembly Binary. This repository uses [`Binaryen`](https://github.com/WebAssembly/binaryen) as a parse backend in order to be up-to-date with the latest WASM standards and proposals.

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

`WABIDB` uses `class Instrumenter` internally, which is also potentially useful for individual usage in other projects. You only need to import `instrumenter.hpp` for basic C++ APIs. It provides a general wasm binary instrumentation tool that is easy to use and powerful as well.

Note that these APIs **MUST** be called in a certain sequence described in section [Calling Sequence](#calling-sequence).

The `routine()` method below covers basic usage of the APIs:
```cpp
void routine() {
    InstrumentConfig config;
    config.filename = "./test.wasm";
    config.targetname = "./test_instr.wasm";
    // example: insert an i32.const 0 and a drop before and after every call
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
    Instrumenter instrumenter;
    instrumenter.setConfig(config);
    instrumenter.instrument({op1,});
    instrumenter.writeBinary();
}
```

## API
### General Instrumentation
The config of general instrumentation is designed for a match-and-insert semantics. It finds expressions in functions that match vec\<target\> one by one, and insert certain instructions before and after the specific expression.<br/>
Note that these targets **MUST** be orthogonal.
```cpp
InstrumentResult instrument(const vector<InstrumentOperation> &operations);

operation.targets.push_back(
  InstrumentOperation::ExpName{
    wasm::Expression::Id::[XXXId], 
    InstrumentOperation::ExpName::ExpOp{[XXXOp]}
  }
);
operation.[POSITION]_instructions = {
  "[instruction 1]",
  "[instruction 2]",
  ...
};
```
>`XXXId` in `wasm::Expression::Id`.<br/> `XXXOp` now only support unary and binary instructions in `wasm::UnaryOP` and `wasm::BinaryOP`, `-1` to ignore Op check. See the definitions in `wasm.h` of `Binaryen`.<br/> `POSITION` can be `pre` or `post`.

### Add Declaration
```cpp
Global* addGlobal(const char* name, 
                  BinaryenType type, 
                  bool if_mutable, 
                  BinaryenLiteral value);
void addFunctions(vector<string> &names,
                  vector<string> &func_bodies);
Memory* addMemory(const char* name, bool if_shared);
```

### Add Import
Mostly similar to API of `Binaryen`:
```cpp
void addImportFunction(const char* internal_name,
                       const char* external_module_name,
                       const char* external_base_name,
                       ...[specific attributes]);
void addImportGlobal(...);
void addImportMemory(...);
```

### Add Export
```cpp
Export* addExport(ModuleItemKind kind, 
                  const char* internal_name, 
                  const char* external_name);
```

### Find Operations
```cpp
Global*   getGlobal(const char* name);
Function* getFunction(const char* name);
Memory* getMemory(const char* name);
Export*   getExport(const char* external_name);
Importable* getImport(ModuleItemKind kind, const char* base_name);
Function* getStartFunction();
```

### Print
```cpp
void print(bool if_stack_ir = false);
```

### Scope
`instrument()` will be performed on functions in the scope. The scope contains all defined(unimport) functions of the original binary by default. You can modify the scope using following apis.
```cpp
bool scope_add(const std::string& name);
bool scope_remove(const std::string& name);
bool scope_contains(const std::string& name);
void scope_clear();
```

## Calling Sequence
When validating the instrumented instructions, we should guarentee that newly defined `global`s, `import`s and `function`s can be found. Thus, a calling sequence should be obeyed as follow:
| Phase | State       | Call             |
| :---: | :---------: | :--------------: |
| 1     | read module | `setConfig()`    |
| 2     | declaration | `addImport[*]()` |
|       |             | `addGlobal()`    |
|       |             | `addMemory()`    |
|       |             | `addFunctions()` |
|       |             | `addExport()`    |
| 3     | instrument  | `instrument()`   |
| 4     | write back  | `writeBinary()`  |

All `get[*]()` can be called in phase 2 & 3. Scope apis should be called before phase 3.

## Support Information
Default support WebAssembly version 1 (MVP), can be easily set for further features. Using `Binaryen` enables `WABIDB` to support most up-to-date proposals.

Now use an experimental WAT-parser of `Binaryen`, which may be switched when [`issue#6208`](https://github.com/WebAssembly/binaryen/issues/6208) is fully resolved.
