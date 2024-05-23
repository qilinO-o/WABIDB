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

## Instrumentation
`WABIDB` basically provides the ability to modify a wasm binary, which is also potentially useful for individual usage in other projects. You only need to import [`instrumenter.hpp`](./src/instrumenter.hpp) for basic [C++ APIs](#api).

Note that these APIs **MUST** be called in a certain sequence described in section [Calling Sequence](#calling-sequence).

The `routine()` method below gives a basic usage of the APIs:
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
    op1.pre_instructions.instructions = {
        "i32.const 0",
        "drop"
    };
    Instrumenter instrumenter;
    instrumenter.setConfig(config);
    instrumenter.instrument({op1,});
    instrumenter.writeBinary();
}
```
More examples can be found in [test](./test/) directory.

## Debug
### wabidb-inspect
`wabidb-inspect` is an interactive debugger for WebAssembly binaries. It can be used for WebAssembly code and WASI applications as well. The tool is runtime-independent and relies on instrumentation technique.

Current features include inspect locals, globals and backtrace after a specific position of code.

Basic usage:
```shell
$ wabidb-inspect example.wasm -cmd=wasmtime
```

Full [tutorial](./docs/wabidb-inspect.md) here.


## API
### Define the fragment to be inserted
**Important:** All inserted instructions should be carefully designed to maintain a still balanced stack after insertions.

The data structure below helps you to maintian the stack.
```cpp
struct InstrumentFragment {
    // a list of instructions that will be executed one by one
    vector<string> instructions;
    // declare number and types of locals(index) used in upon vector of instructions for validation
    // must all be basic type (i32 i64 f32 f64 v128)
    vector<Type> local_types = {};
    // stack context that the fragment will use
    // the deduction rule must be followed C|- fragment : [stack_context] -> [stack_context]
    vector<Type> stack_context = {};
};
```

### Match-and-Insert Instrumentation
The instrumentation is designed for a match-and-insert semantics. It finds expressions in functions that match any target of a target set, and insert certain instructions before and after the matching point.

Note that these targets **MUST** be orthogonal(Or we say the target in front is matched first). 

```cpp
InstrumentResult instrument(const vector<InstrumentOperation> &operations);

struct InstrumentOperation {
    struct ExpName {
        Expression::Id id;
        union ExpOp {
            UnaryOp uop; // identify which unary or binary op it is
            BinaryOp bop;
            StackInst::Op cop; // control flow op mark its begin or end
        };
        optional<ExpOp> exp_op;
        optional<BinaryenType> exp_type;
    };
    vector<ExpName> targets;
    InstrumentFragment pre_instructions;
    InstrumentFragment post_instructions;
};
```
> See Op and Type definitions in [`wasm.h`](https://github.com/WebAssembly/binaryen/blob/main/src/wasm.h), [`binaryen-c.h`](https://github.com/WebAssembly/binaryen/blob/main/src/binaryen-c.h) and [`wasm-stack.h`](https://github.com/WebAssembly/binaryen/blob/main/src/wasm-stack.h) of [`Binaryen`](https://github.com/WebAssembly/binaryen).

### Position-Insert Instrumentation
Insert operation.post_instructions after the line of pos of the named function. Instructions are indexed from 1 and pos = 0 equals to insert at the beginning of the function.
```cpp
InstrumentResult instrumentFunction(const InstrumentOperation &operation,
                                    const char* name,
                                    size_t pos);
```

### General Iteration
Above apis may not cover all instrumentation scenarios, so `WABIDB` provides function-level and instruction-level iteration template. Any customized instrumentation can be implemented upon them.

These apis are defined in [`instr-utils.hpp`](/src/instr-utils.hpp).
```cpp
// The visitor provided should have signature void(Function*)
template<typename T> inline void iterDefinedFunctions(Module* m, T visitor);
// The visitor provided should have signature void(std::list<wasm::StackInst *>, std::list<wasm::StackInst *>::iterator&)
template<typename T> inline void iterInstructions(Function* func, T visitor);
```

### Add Declaration
`nullptr` or `false` to indicate failure.
```cpp
Global*      addGlobal(const char* name,
                       BinaryenType type,
                       bool if_mutable,
                       BinaryenLiteral value);
bool         addFunctions(vector<string> &names,
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
`false` to indicate failure.
Mostly similar to APIs of [`Binaryen`](https://github.com/WebAssembly/binaryen):
```cpp
bool addImportFunction(const char* internal_name,
                       const char* external_module_name,
                       const char* external_base_name,
                       ...[specific attributes]);
bool addImportGlobal(...);
bool addImportMemory(...);
```

### Add Export
`nullptr` to indicate failure.
```cpp
Export* addExport(ModuleItemKind kind, 
                  const char* internal_name, 
                  const char* external_name);
```

### Find Operations
`nullptr` to indicate failure.
```cpp
Global*      getGlobal(const char* name);
Function*    getFunction(const char* name);
Memory*      getMemory(const char* name = nullptr); // omit param for default memory
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
When validating the instrumented instructions, we should guarentee that newly defined `global`s, `import`s, `function`s and etc. can be found. Thus, a calling sequence should be obeyed as follow:
| Phase | State       | Call                         |
| :---: | :---------: | :--------------------------: |
| 1     | read module | `setConfig()`                |
| 2     | declaration | `addImport[*]()`             |
|       |             | `addGlobal()`                |
|       |             | `addMemory()`                |
|       |             | `addPassiveDataSegment()`    |
|       |             | `addFunctions()`             |
|       |             | `addExport()`                |
| 3     | instrument  | `instrument()` & ...         |
| 4     | write back  | `writeBinary()`              |

All `get[*]()` can be called in phase 2 & 3. Scope APIs should be called before phase 3.

## Support Information
Default support the core spec and all [proposals](https://github.com/WebAssembly/spec/tree/main/proposals) in the spec, can be easily set for further features. Using `Binaryen` enables `WABIDB` to support most up-to-date proposals.

Now use an experimental WAT-parser of `Binaryen`, which may be switched when [`issue#6208`](https://github.com/WebAssembly/binaryen/issues/6208) is fully resolved.

## Misc
I guess "WABIDB" should be pronounced as "Wabi Debugger", anyway.

Get the idea of `wabidb-inspect` from [`wasminspect`](https://github.com/kateinoigakukun/wasminspect), unfortunately it is not updated for a long time.