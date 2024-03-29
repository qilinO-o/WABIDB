#include <iostream>
#include "instrumenter.hpp"

int main(int argc, char **argv) {
    std::printf("Now using wasm_instrumenter.\n");

    wasm_instrument::InstrumentConfig config;
    config.filename = "/home/hdh/wasm_code/wasm_instrumenter/playground/manual_hello.wasm";
    config.targetname = "/home/hdh/wasm_code/wasm_instrumenter/playground/add_instr.wasm";
    wasm_instrument::InstrumentOperation op1;
    op1.location = wasm_instrument::InstrumentOperation::Loaction::after;
    config.operations.push_back(op1);
    
    wasm_instrument::Instrumenter instrumenter(config);
    wasm_instrument::InstrumentResult result = instrumenter.instrument();
    
    std::printf("End instrument with result code: %d\n", static_cast<int>(result));
    return 0;
}