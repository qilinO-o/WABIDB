#include "config-builder.hpp"
#include <parser/wat-parser.h>
#include <pass.h>
#include <random>

namespace wasm_instrument {

// binaryen has experimental text parser for wabt output(aka stack style)
// while it is disabled by flag useNewWATParser = false
// reimplement it here
bool _readTextData(const std::string& input, wasm::Module& wasm) {
    std::string_view in(input.c_str());
    if (auto parsed = wasm::WATParser::parseModule(wasm, in); auto err = parsed.getErr()) {
        std::cerr << err->msg << std::endl;
        return false;
    }
    return true;
}

static std::string _random_prefix_generator() {
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::random_device rd;
    std::mt19937 generator(rd());
    std::shuffle(str.begin(), str.end(), generator);
    return str.substr(0, 8);
}

// transform two lists to a well-formed func string like .wat
std::string _makeIRString(const std::vector<std::string>& pre_list, 
                        const std::vector<std::string>& post_list,
                        int func_num, 
                        const std::string& random_prefix) 
{
    std::string funcs_str = "(func $" + random_prefix + std::to_string(func_num) + "_1\n";
    for (const auto& instr_str : pre_list) {
        funcs_str += instr_str;
        funcs_str += "\n";
    }
    funcs_str += "unreachable)\n(func $" + random_prefix + std::to_string(func_num) + "_2\n";
    for (const auto& instr_str : post_list) {
        funcs_str += instr_str;
        funcs_str += "\n";
    }
    funcs_str += "unreachable)\n";
    return funcs_str;
}

// transform all operations to a well-formed module string like .wat
void _makeModuleString(std::string& module_str, 
                                const std::vector<InstrumentOperation>& operations, 
                                const std::string& random_prefix)
{
    while (module_str.back() != ')') 
        module_str.pop_back();
    assert(module_str.back() == ')');
    module_str.pop_back();
    int op_num = 1;
    for (const auto& operation : operations) {
        module_str += _makeIRString(operation.pre_instructions, operation.post_instructions, op_num, random_prefix);
        op_num++;
    }
    module_str += ")";
}

// input config and output the data structure of a vector of both pre_list and post_list
// which can be used directly for class Instrumenter to do instrument()
// this function is called by class Instrumenter when dealing with config
// return nullptr aka InstrumentResult::config_error
AddedInstructions* ConfigBuilder::makeConfig(wasm::Module* &mallocator, const InstrumentConfig &config) noexcept {
    AddedInstructions* added_instructions = new AddedInstructions;
    added_instructions->vec.resize(config.operations.size());
    auto random_prefix = _random_prefix_generator();
    
    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    Colors::setEnabled(false);
    // also do stack ir pass on the module
    mallocator->typeNames.clear();
    wasm::printStackIR(mstream, mallocator, true);
    std::string module_str = mstream.str();
    std::string backup_str = module_str;

    _makeModuleString(module_str, config.operations, random_prefix);

    auto feature = mallocator->features;
    BinaryenModuleDispose(mallocator);
    mallocator = BinaryenModuleCreate();
    mallocator->features.set(feature);
    
    if (!_readTextData(module_str, *(mallocator))) {
        std::cerr << "ConfigBuilder: makeConfig() read text error!" << std::endl;
        delete added_instructions;
        Colors::setEnabled(is_color);
        BinaryenModuleDispose(mallocator);
        mallocator = BinaryenModuleCreate();
        if (!_readTextData(backup_str, *(mallocator))) {
            std::cerr << "ConfigBuilder: makeConfig() cannot recover module! Further operations should end!" << std::endl;
        }
        return nullptr;
    }
    Colors::setEnabled(is_color);

    // do stack ir pass on the module
    wasm::PassRunner pass_runner(mallocator);
    pass_runner.add("generate-stack-ir");
    pass_runner.add("optimize-stack-ir");
    pass_runner.run();

    int op_num = 1;
    for (const auto& operation : config.operations) {
        std::string op_num_str = std::to_string(op_num);
        auto func1 = BinaryenGetFunction(mallocator, (random_prefix + op_num_str + "_1").c_str());
        auto func2 = BinaryenGetFunction(mallocator, (random_prefix + op_num_str + "_2").c_str());
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
        BinaryenRemoveFunction(mallocator, func1->name.toString().c_str());
        BinaryenRemoveFunction(mallocator, func2->name.toString().c_str());
        op_num++;
    }

    return added_instructions;
}

}