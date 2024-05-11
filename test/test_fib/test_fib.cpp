#include "instrumenter.hpp"

using namespace wasm_instrument;

/*
* test_fib doc:
* 1. read in a side module of a simple fibonacci function
* 2. add WASI function fd_write()
* 3. add global to record the number of calls being executed
* 4. add memory section
* 5. add function as _start for running the test in standalone runtimes
*    the _start function calls fib(x) then print its result(or maybe the number of calls)
* 6. add function to transfer int(i32) to char*(i32) which is compiled from cpp
*/
int main() {
    std::string relative_path = "../test/test_fib/";
    
    InstrumentConfig config;
    // the wasm file is compiled from fib.cpp
    // compile parameters: em++ fib.cpp -Og -s SIDE_MODULE=1 -o fib.wasm
    config.filename = relative_path + "fib.wasm";
    config.targetname = relative_path + "fib_instr.wasm";

    // set the input / output file name and open module
    Instrumenter instrumenter;
    InstrumentResult result = instrumenter.setConfig(config);
    assert(result == InstrumentResult::success);
    
    // Create a function type for (i32, i32, i32, i32) -> i32
    // import WASI function fd_write()
    BinaryenType iiii[4] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32()};
    BinaryenType fdwrite_params = BinaryenTypeCreate(iiii, 4);
    BinaryenType fdwrite_results = BinaryenTypeInt32();
    instrumenter.addImportFunction("fd_write", "wasi_snapshot_preview1", "fd_write", fdwrite_params, fdwrite_results);
    
    // add new global to record the number of call instructions being executed
    auto ret1 = instrumenter.addGlobal("call_num", BinaryenTypeInt32(), true, BinaryenLiteralInt32(0));
    assert(ret1 != nullptr);

    // add memory for further functions
    auto ret2 = instrumenter.addMemory("mem", false, 256, 256);
    assert(ret2 != nullptr);
    
    // add a function to increment call number after every call
    // add a function which calls function fib(x) and do print job
    // add a function to parse i32 to char*
    auto export_func_fib = instrumenter.getExport("fib");
    auto func_fib = instrumenter.getFunction(export_func_fib->value.toString().c_str());
    auto internal_func_fib_name = func_fib->name.toString();
    instrumenter.addFunctions({"add_call", "do_test", "itoa",}, 
    {
        "(func $add_call\n"
            "i32.const 1\n"
            "global.get $call_num\n"
            "i32.add\n"
            "global.set $call_num\n)",
        "(func $do_test\n"
            "i32.const 8\n"
            "call $" + internal_func_fib_name + "\n"
            //"drop\n"
            //"global.get $call_num\n"
            "i32.const 1024\n"
            "call $itoa\n"
            "i32.const 512\n"
            "i32.const 1024\n"
            "i32.store\n"
            "i32.const 516\n"
            "i32.const 8\n"
            "i32.store\n"
            "i32.const 1\n"
            "i32.const 512\n"
            "i32.const 1\n"
            "i32.const 24\n"
            "call $fd_write\n"
            "drop\n)",
        R"((func $itoa (param i32 i32)
    (local i32 i32 i32 i32)
    local.get 0
    if  ;; label = @1
      i32.const 12800
      local.set 4
      loop  ;; label = @2
        local.get 4
        i32.const 6
        i32.add
        local.get 2
        i32.add
        local.get 0
        local.get 0
        i32.const 10
        i32.div_s
        local.tee 3
        i32.const 10
        i32.mul
        i32.sub
        i32.const 48
        i32.add
        i32.store8
        local.get 2
        i32.const 1
        i32.add
        local.set 2
        local.get 0
        i32.const 9
        i32.add
        local.get 3
        local.set 0
        i32.const 19
        i32.ge_u
        br_if 0 (;@2;)
      end
      loop  ;; label = @2
        local.get 1
        local.get 2
        i32.const 1
        i32.sub
        local.tee 0
        local.get 4
        i32.const 6
        i32.add
        i32.add
        i32.load8_u
        i32.store8
        local.get 1
        i32.const 1
        i32.add
        local.set 1
        local.get 2
        i32.const 1
        i32.gt_s
        local.get 0
        local.set 2
        br_if 0 (;@2;)
      end
    end))",
    });
    
    // set function do_test() as _start
    auto ret3 = instrumenter.addExport(wasm::ModuleItemKind::Function, "do_test", "_start");
    assert(ret3 != nullptr);
    
    // a module with WASI apis must export memory by default
    ret3 = instrumenter.addExport(wasm::ModuleItemKind::Memory, "mem", "memory");
    assert(ret3 != nullptr);
    
    // add code segment before every call
    InstrumentOperation op1;
    op1.targets.push_back(InstrumentOperation::ExpName{
    wasm::Expression::Id::CallId, std::nullopt, std::nullopt});
    op1.pre_instructions.instructions = {
        "call $add_call",
    };
    op1.post_instructions = {};
    result = instrumenter.instrument({op1,});
    assert(result == InstrumentResult::success);
    
    // write back the instrumented module
    result = instrumenter.writeBinary();
    assert(result == InstrumentResult::success);
    
    return 0;
}