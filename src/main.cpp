#include <iostream>
#include "instrumenter.hpp"

using namespace wasm_instrument;

int main(int argc, char **argv) {
    std::printf("Now using wasm_instrumenter.\n");

    InstrumentConfig config;
    config.filename = "../playground/fd_write.wasm";
    config.targetname = "../playground/instr_result/2.wasm";
    
    Instrumenter instrumenter(config);
    InstrumentResult result = instrumenter.instrument();
    
    std::printf("End instrument with result: %s\n", InstrumentResult2str(result).c_str());
    return 0;
}