#include "operation-builder.hpp"
#include "instrumenter.hpp"
#include <random>
using namespace wasm_instrument;
// usage: stack_canary [infile name] [outfile name]
int main(int argc, const char* argv[]) {
    if (argc <= 2) return 1;
    InstrumentConfig config;
    config.filename = argv[1];
    config.targetname = argv[2];
    Instrumenter instrumenter;
    instrumenter.setConfig(config);
    std::random_device rd;
    int64_t canary = rd();
    InstrumentOperation op_validate;
    op_validate.targets.push_back(InstrumentOperation::ExpName{wasm::Expression::Id::CallId, std::nullopt, std::nullopt});
    op_validate.targets.push_back(InstrumentOperation::ExpName{wasm::Expression::Id::CallIndirectId, std::nullopt, std::nullopt});
    op_validate.post_instructions.instructions = {
        "global.get $__stack_pointer",
        "i64.load",
        "i64.const " + std::to_string(canary),
        "i64.ne",
        "if",
        "unreachable",
        "end",
        "global.get $__stack_pointer",
        "i32.const 16",
        "i32.add",
        "global.set $__stack_pointer"
    };
    instrumenter.instrument({op_validate});
    InstrumentOperation op_inject;
    op_inject.post_instructions.instructions = {
        "global.get $__stack_pointer",
        "i32.const 16",
        "i32.sub",
        "global.set $__stack_pointer",
        "global.get $__stack_pointer",
        "i64.const " + std::to_string(canary),
        "i64.store"
    };
    for (const auto &name : instrumenter.getScope()) {
        instrumenter.instrumentFunction(op_inject, name.c_str(), 0);
    }
    instrumenter.writeBinary();
    return 0;
}