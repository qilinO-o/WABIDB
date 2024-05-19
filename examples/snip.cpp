#include "operation-builder.hpp"
#include "instrumenter.hpp"
using namespace wasm_instrument;
// usage: snip [infile name] [function name] [outfile name]
int main(int argc, const char* argv[]) {
    if (argc <= 3) return 1;
    std::string func_name = argv[2];
    InstrumentConfig config;
    config.filename = argv[1];
    config.targetname = argv[3];
    Instrumenter instrumenter;
    instrumenter.setConfig(config);
    std::map<std::string, int> in_degree;
    for (const auto &name : instrumenter.getScope()) in_degree[name] = 0;
    std::string cur_func;
    auto inst_vistor = [&instrumenter, &in_degree, &cur_func](std::list<wasm::StackInst*> &l, std::list<wasm::StackInst*>::iterator &iter) {
        auto inst = *iter;
        if (inst->origin->_id != wasm::Expression::Id::CallId) return;
        auto call = inst->origin->dynCast<wasm::Call>();
        if (call->target.toString() != cur_func)
            in_degree[call->target.toString()]++;
    };
    auto func_visitor = [&inst_vistor, &instrumenter, &func_name, &cur_func](wasm::Function* func) {
        cur_func = func->name.toString();
        if (cur_func == func_name) return;
        iterInstructions(func, inst_vistor);
    };
    iterDefinedFunctions(instrumenter.getModule(), func_visitor);
    instrumenter.scopeClear();
    for (const auto &[k, v] : in_degree)
        if (v == 0) instrumenter.scopeAdd(k);
    InstrumentOperation op;
    op.pre_instructions.instructions = {
        "unreachable",
    };
    auto added_instructions = OperationBuilder().makeOperations(instrumenter.getModule(), {op});
    auto insts = (*added_instructions)[0].pre_instructions;
    auto func_snipper = [&instrumenter, &insts](wasm::Function* func) {
        if (!instrumenter.scopeContains(func->name.toString())) return;
        func->stackIR = std::make_unique<wasm::StackIR>(insts);
    };
    iterDefinedFunctions(instrumenter.getModule(), func_snipper);
    instrumenter.writeBinary();
    return 0;
}