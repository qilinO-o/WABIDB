#include <instrumenter.hpp>
using namespace wasm_instrument;

void instruction_mix() {
    Instrumenter instrumenter;
    instrumenter.addGlobal("__count_base", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    instrumenter.addFunctions({"__incInstr", "__prepare"},
        {"(func $__incInstr (param i32) (local i32)\nlocal.get 0\ni32.const 4\ni32.mul\nglobal.get $__count_base\ni32.add\nlocal.tee 1\nlocal.get 1\ni32.load\ni32.const 1\ni32.add\ni32.store\n)",
        "(func $__prepare\ni32.const 1\nmemory.grow\ni32.const 65536\ni32.mul\nglobal.set $__count_base\n)"});
    std::vector<InstrumentOperation> ops(23);
    for (int i = 1; i <= 23; i++) {
        ops[i-1].targets.push_back(InstrumentOperation::ExpName{wasm::Expression::Id(i), std::nullopt, std::nullopt});
        ops[i-1].pre_instructions.instructions = {"i32.const " + std::to_string(i), "call $incInstr",};
    }
    instrumenter.instrument(ops);
    InstrumentOperation op;
    op.post_instructions.instructions = {"call $__prepare"};
    instrumenter.instrumentFunction(op, instrumenter.getStartFunction()->name.toString().c_str(), 0);
}

void cryptominer_detection() {
    Instrumenter instrumenter;
    instrumenter.addGlobal("__count_base", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    instrumenter.addFunctions({"__incInstr", "__prepare"},
        {"(func $__incInstr (param i32) (local i32)\nlocal.get 0\ni32.const 4\ni32.mul\nglobal.get $__count_base\ni32.add\nlocal.tee 1\nlocal.get 1\ni32.load\ni32.const 1\ni32.add\ni32.store\n)",
        "(func $__prepare\ni32.const 1\nmemory.grow\ni32.const 65536\ni32.mul\nglobal.set $__count_base\n)"});
    std::vector<InstrumentOperation> ops(4);
    std::vector<wasm::BinaryOp> signature {wasm::BinaryOp::AddInt32, wasm::BinaryOp::AndInt32, wasm::BinaryOp::ShlInt32, wasm::BinaryOp::ShrUInt32, wasm::BinaryOp::XorInt32};
    for (int i = 1; i <= 4; i++) {
        InstrumentOperation::ExpName t {wasm::Expression::Id::BinaryId, std::nullopt, std::nullopt};
        t.exp_op->bop = signature[i-1];
        ops[i-1].targets.push_back(t);
        ops[i-1].pre_instructions.instructions = {"i32.const " + std::to_string(i), "call $incInstr",};
    }
    instrumenter.instrument(ops);
    InstrumentOperation op;
    op.post_instructions.instructions = {"call $__prepare"};
    instrumenter.instrumentFunction(op, instrumenter.getStartFunction()->name.toString().c_str(), 0);
}

void memory_access_tracing() {
    Instrumenter instrumenter;
    instrumenter.addGlobal("__count_base", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    instrumenter.addFunctions({"__accessload", "__accessstore", "__prepare"},
        {"(func $__accessload (param i32) (result i32) (local i32)\nlocal.get 0\nlocal.set 1\nglobal.get $__count_base\nlocal.get 1\ni32.store\ni32.const 4\nglobal.get $__count_base\ni32.add\ni32.const 0\ni32.store\ni32.const 8\nglobal.get $__count_base\ni32.add\nglobal.set $__count_base\nlocal.get 1\n)",
        "(func $__accessstore (param i32 i32) (result i32 i32) (local i32 i32)\nlocal.get 0\nlocal.set 2\nlocal.get 1\nlocal.set 3\nglobal.get $__count_base\nlocal.get 2\ni32.store\ni32.const 4\nglobal.get $__count_base\ni32.add\ni32.const 1\ni32.store\ni32.const 8\nglobal.get $__count_base\ni32.add\nglobal.set $__count_base\nlocal.get 2\nlocal.get 3\n)",
        "(func $__prepare\ni32.const 1\nmemory.grow\ni32.const 65536\ni32.mul\nglobal.set $__count_base\n)"});
    std::vector<InstrumentOperation> ops(2);
    ops[0].targets.push_back(InstrumentOperation::ExpName{wasm::Expression::Id::LoadId, std::nullopt, std::nullopt});
    ops[0].pre_instructions.instructions = {"call $__accessload",};
    ops[1].targets.push_back(InstrumentOperation::ExpName{wasm::Expression::Id::StoreId, std::nullopt, std::nullopt});
    ops[1].pre_instructions.instructions = {"call $__accessstore",};
    instrumenter.instrument(ops);
    InstrumentOperation op;
    op.post_instructions.instructions = {"call $__prepare"};
    instrumenter.instrumentFunction(op, instrumenter.getStartFunction()->name.toString().c_str(), 0);
}

int main() {
    
    return 0;
}