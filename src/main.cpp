#include <iostream>
#include "instrumenter.hpp"

using namespace wasm_instrument;

int main(int argc, char **argv) {
    std::printf("Now using wasm_instrumenter.\n");

    InstrumentConfig config;
    config.filename = "../playground/fd_write.wasm";
    config.targetname = "../playground/instr_result/9.wasm";
    
    Instrumenter instrumenter;
    InstrumentResult result = instrumenter.setConfig(config);
    assert(result == InstrumentResult::success);

    instrumenter.addGlobal("g", BinaryenTypeInt64(), false, BinaryenLiteralInt64(100000));
    assert(instrumenter.getGlobal("g") != nullptr);
    std::printf("## 1 ##\n");
    instrumenter.addImportGlobal("impg", "om", "omg", BinaryenTypeFloat32(), false);
    assert(instrumenter.getGlobal("impg") != nullptr);
    std::printf("## 2 ##\n");
    instrumenter.addImportFunction("impf", "om", "omf", BinaryenTypeNone(), BinaryenTypeNone());
    assert(instrumenter.getFunction("impf") != nullptr);
    std::printf("## 3 ##\n");
    instrumenter.addExport(wasm::ModuleItemKind::Global, "g", "expg");
    assert(instrumenter.getExport("expg") != nullptr);
    std::printf("## 4 ##\n");
    assert(instrumenter.getImport(wasm::ModuleItemKind::Function, "fd_write") != nullptr);
    std::printf("## 5 ##\n");
    std::vector<std::string> names = {"donothing", "donothing2"};
    std::vector<std::string> func_bodies = {" (func $donothing (param i32) (result i32)\n"
                                                "  local.get 0\n  i32.const 100000\n  i32.add\n )",
                                            " (func $donothing2 (param i32) (result i32)\n"
                                                "  local.get 0\n  global.get $g\n  i32.wrap_i64\n  i32.add\n )"};
    instrumenter.addFunctions(names, func_bodies);
    for (auto name: names) {
        assert(instrumenter.getFunction(name.c_str()) != nullptr);
    }
    std::printf("## 6 ##\n");
    auto start_func = instrumenter.getStartFunction();

    // example: insert an i32.const(2333) and a drop before and after every call
    InstrumentOperation op1;
    op1.targets.push_back(InstrumentOperation::ExpName{
    wasm::Expression::Id::CallId, std::nullopt, std::nullopt});
    op1.pre_instructions = {
        "i32.const 20001",
        "drop"
    };
    op1.post_instructions = {
        "i32.const 20002",
        "drop"
    };
    //result = instrumenter.instrument({op1,});
    result = instrumenter.instrumentFunction(op1, "donothing", 0);
    assert(result == InstrumentResult::success);
    std::printf("## 7 ##\n");
    result = instrumenter.writeBinary();
    std::printf("## 8 ##\n");
    std::printf("End instrument with result: %s\n", InstrumentResult2str(result).c_str());
    return 0;
}