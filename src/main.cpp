#include <iostream>
#include "instrumenter.hpp"

using namespace wasm_instrument;

int main(int argc, char **argv) {
    std::printf("Now using wasm_instrumenter.\n");

    InstrumentConfig config;
    config.filename = "../playground/fd_write.wasm";
    config.targetname = "../playground/instr_result/3.wasm";
    // example: insert an i32.const(2333) and a drop before and after every call
    InstrumentOperation op1;
    op1.targets.push_back(InstrumentOperation::ExpName{
    wasm::Expression::Id::CallId, InstrumentOperation::ExpName::ExpOp{.no_op=-1}});
    op1.pre_instructions = {
        "i32.const 2333",
        "drop"
    };
    op1.post_instructions = {
        "i32.const 2333",
        "drop"
    };
    config.operations.push_back(op1);
    
    Instrumenter instrumenter;
    InstrumentResult result = instrumenter.setConfig(config);
    assert(result == InstrumentResult::success);
    result = instrumenter.instrument();
    assert(result == InstrumentResult::success);
    result = instrumenter.writeBinary();
    
    std::printf("End instrument with result: %s\n", InstrumentResult2str(result).c_str());
    return 0;
}