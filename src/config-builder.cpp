#include "config-builder.hpp"
#include <parser/wat-parser.h>
#include <pass.h>

namespace wasm_instrument {

// binaryen has experimental text parser for wabt output(aka stack style)
// while it is disabled by flag useNewWATParser = false
// reimplement it here
static bool _readTextData(const std::string& input, wasm::Module& wasm) {
    std::string_view in(input.c_str());
    if (auto parsed = wasm::WATParser::parseModule(wasm, in); auto err = parsed.getErr()) {
        std::cerr << err->msg;
        return false;
    }
    return true;
}

// transform two lists to a well-formed func string like .wat
std::string _makeIRString(const std::vector<std::string>& pre_list, 
        const std::vector<std::string>& post_list, int func_num) {
    std::string module_str = "(func $" + std::to_string(func_num) + "_1\n";
    for (const auto& instr_str : pre_list) {
        module_str += instr_str;
        module_str += "\n";
    }
    module_str += "unreachable)\n(func $" + std::to_string(func_num) + "_2\n";
    for (const auto& instr_str : post_list) {
        module_str += instr_str;
        module_str += "\n";
    }
    module_str += "unreachable)\n";
    return module_str;
}

// transform all operations to a well-formed module string like .wat
std::string _makeModuleString(const std::vector<InstrumentOperation>& operations) {
    std::string module_str = "(module\n";
    int op_num = 1;
    for (const auto& operation : operations) {
        module_str += _makeIRString(operation.pre_instructions, operation.post_instructions, op_num);
        op_num++;
    }
    module_str += ")";
    return module_str;
}

// input config and output the data structure of a vector of both pre_list and post_list
// which can be used directly for class Instrumenter to do instrument()
// this function is called by class Instrumenter when dealing with config
// return nullptr aka InstrumentResult::config_error
AddedInstructions* ConfigBuilder::makeConfig(wasm::Module &mallocator, const InstrumentConfig &config) noexcept {
    AddedInstructions* added_instructions = new AddedInstructions;
    added_instructions->vec.resize(config.operations.size());
    auto module_str = _makeModuleString(config.operations);
    //std::cout << module_str << std::endl;
    if (!_readTextData(module_str, mallocator)) {
        delete added_instructions;
        return nullptr;
    }

    // do stack ir pass on the module
    auto pass_runner = new wasm::PassRunner(&mallocator);
    pass_runner->add("generate-stack-ir");
    pass_runner->add("optimize-stack-ir");
    pass_runner->run();
    delete pass_runner;

    int op_num = 1;
    for (const auto& operation : config.operations) {
        std::string op_num_str = std::to_string(op_num);
        auto func1 = BinaryenGetFunction(&mallocator, (op_num_str + "_1").c_str());
        auto func2 = BinaryenGetFunction(&mallocator, (op_num_str + "_2").c_str());
        assert(func1 != nullptr);
        assert(func2 != nullptr);
        assert(func1->stackIR.get() != nullptr);
        assert(func2->stackIR.get() != nullptr);
        for (auto i = 0; i < func1->stackIR.get()->size() - 1; i++) {
            added_instructions->vec[op_num - 1].pre_instructions.push_back((*(func1->stackIR.get()))[i]);
        }
        for (auto i = 0; i < func2->stackIR.get()->size() - 1; i++) {
            added_instructions->vec[op_num - 1].post_instructions.push_back((*(func2->stackIR.get()))[i]);
        }
        BinaryenRemoveFunction(&mallocator, func1->name.toString().c_str());
        BinaryenRemoveFunction(&mallocator, func2->name.toString().c_str());
        op_num++;
    }

    return added_instructions;
}

}