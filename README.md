# WABIDB

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
Run all tests registered in [./test/CMakeLists.txt](./test/CMakeLists.txt):
```
make test
```

## Usage Tutorial
### Instrumentation
`WABIDB` basically provides the ability to modify a wasm binary, which is also potentially useful for individual usage in other projects. You only need to import [`instrumenter.hpp`](./src/instrumenter.hpp) for basic [C++ APIs](#api).

Note that these APIs **MUST** be called in a certain sequence described in section [Calling Sequence](#calling-sequence).

The `routine()` method below covers basic usage of the APIs:
```cpp
void routine() {
    InstrumentConfig config;
    config.filename = "./test.wasm";
    config.targetname = "./test_instr.wasm";
    // example: insert an i32.const 0 and a drop before every call
    InstrumentOperation op1;
    op1.targets.push_back(InstrumentOperation::ExpName{
      wasm::Expression::Id::CallId, 
      std::nullopt, std::nullopt
    });
    op1.pre_instructions = {
        "i32.const 0",
        "drop"
    };
    op1.post_instructions = {};
    Instrumenter instrumenter;
    instrumenter.setConfig(config);
    instrumenter.instrument({op1,});
    instrumenter.writeBinary();
}
```
More examples can be found in [test](./test/) directory.

### Debug
⚠️ Still under construction!

## API
### General Instrumentation
The config of general instrumentation is designed for a match-and-insert semantics. It finds expressions in functions that match any target of a target set, and insert certain instructions before and after the specific expressions.

Note that these targets **MUST** be orthogonal(Or we say the target in front is matched first). 

Inserted instructions should be carefully designed to maintain a still balanced stack after insertions.

```cpp
InstrumentResult instrument(const vector<InstrumentOperation> &operations);

struct InstrumentOperation {
    struct ExpName {
        Expression::Id id;
        union ExpOp {
            UnaryOp uop;
            BinaryOp bop;
            // control flow op mark its begin or end
            StackInst::Op cop;
        };
        // nullopt to ignore Op check
        optional<ExpOp> exp_op;
        // nullopt to ignore type check
        optional<BinaryenType> exp_type;
    };
    vector<ExpName> targets;
    vector<string> pre_instructions;
    vector<string> post_instructions;
};
```
> See Op and Type definitions in [`wasm.h`](https://github.com/WebAssembly/binaryen/blob/main/src/wasm.h) and [`binaryen-c.h`](https://github.com/WebAssembly/binaryen/blob/main/src/binaryen-c.h) of [`Binaryen`](https://github.com/WebAssembly/binaryen).

### Add Declaration
```cpp
Global*      addGlobal(const char* name,
                       BinaryenType type,
                       bool if_mutable,
                       BinaryenLiteral value);
void         addFunctions(vector<string> &names,
                          vector<string> &func_bodies);
Memory*      addMemory(const char* name,
                       bool if_shared,
                       int init_pages,
                       int max_pages);
DataSegment* addPassiveDateSegment(const char* name,
                                   const char* data,
                                   size_t len);
```

### Add Import
Mostly similar to APIs of [`Binaryen`](https://github.com/WebAssembly/binaryen):
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
Global*      getGlobal(const char* name);
Function*    getFunction(const char* name);
Memory*      getMemory(const char* name);
DataSegment* getDateSegment(const char* name);
Export*      getExport(const char* external_name);
Importable*  getImport(ModuleItemKind kind, const char* base_name);
Function*    getStartFunction();
```

### Print
```cpp
void print(bool if_stack_ir = false);
```

### Scope
`instrument()` will be performed on functions in the scope. The scope contains all defined(unimport) functions of the original binary by default. You can modify the scope using following APIs.
```cpp
bool scopeAdd(const std::string& name);
bool scopeRemove(const std::string& name);
bool scopeContains(const std::string& name);
void scopeClear();
```

## Calling Sequence
When validating the instrumented instructions, we should guarentee that newly defined `global`s, `import`s and `function`s can be found. Thus, a calling sequence should be obeyed as follow:
| Phase | State       | Call                         |
| :---: | :---------: | :--------------------------: |
| 1     | read module | `setConfig()`                |
| 2     | declaration | `addImport[*]()`             |
|       |             | `addGlobal()`                |
|       |             | `addMemory()`                |
|       |             | `addPassiveDataSegment()`    |
|       |             | `addFunctions()`             |
|       |             | `addExport()`                |
| 3     | instrument  | `instrument()`               |
| 4     | write back  | `writeBinary()`              |

All `get[*]()` can be called in phase 2 & 3. Scope APIs should be called before phase 3.

## Support Information
Default support the core spec and all [proposals](https://github.com/WebAssembly/spec/tree/main/proposals) in the spec, can be easily set for further features. Using `Binaryen` enables `WABIDB` to support most up-to-date proposals.

Now use an experimental WAT-parser of `Binaryen`, which may be switched when [`issue#6208`](https://github.com/WebAssembly/binaryen/issues/6208) is fully resolved.

## Misc
I guess "WABIDB" should be pronounced as "Wabi Debugger", anyway.