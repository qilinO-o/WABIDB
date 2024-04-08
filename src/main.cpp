#include <iostream>
#include "instrumenter.hpp"

using namespace wasm_instrument;

int main(int argc, char **argv) {
    std::printf("Now using wasm_instrumenter.\n");

    InstrumentConfig config;
    config.filename = "../playground/fd_write.wasm";
    config.targetname = "../playground/instr_result/6.wasm";
    // example: insert an i32.const(2333) and a drop before and after every call
    InstrumentOperation op1;
    op1.targets.push_back(InstrumentOperation::ExpName{
    wasm::Expression::Id::CallId, InstrumentOperation::ExpName::ExpOp{.no_op=-1}});
    op1.pre_instructions = {
        "i32.const 23331",
        "drop"
    };
    op1.post_instructions = {
        "i32.const 23332",
        "drop"
    };
    config.operations.push_back(op1);
    
    Instrumenter instrumenter;
    InstrumentResult result = instrumenter.setConfig(config);
    assert(result == InstrumentResult::success);

    instrumenter.addGlobal("g", BinaryenTypeInt64(), false, BinaryenLiteralInt64(100000));
    instrumenter.addGlobal("g2", BinaryenTypeInt64(), false, BinaryenLiteralInt64(100001));
    assert(instrumenter.getGlobal("g") != nullptr);
    assert(instrumenter.getGlobal("g2") != nullptr);
    std::printf("## 1 ##\n");
    std::vector<std::string> names = {"donothing", "donothing2"};
    std::vector<std::string> func_bodies = {"(func $donothing (param i32) (result i32)\n"
                                                "local.get 0\ni32.const 100000\ni32.add)",
                                            "(func $donothing2 (param i32) (result i32)\n"
                                                "local.get 0\nglobal.get $g\ni32.wrap_i64\ni32.add)"};
    instrumenter.addFunctions(names, func_bodies);
    for (auto name: names) {
        assert(instrumenter.getFunction(name.c_str()) != nullptr);
    }
    std::printf("## 2 ##\n");
    auto start_func = instrumenter.getStartFunction();
    result = instrumenter.instrument();
    assert(result == InstrumentResult::success);
    std::printf("## 3 ##\n");
    result = instrumenter.writeBinary();
    std::printf("## 4 ##\n");
    std::printf("End instrument with result: %s\n", InstrumentResult2str(result).c_str());
    return 0;
}