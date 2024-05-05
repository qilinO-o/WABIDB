#include "operation-builder.hpp"
#include <cassert>
#include <pass.h>
#include <random>

namespace wasm_instrument {

static std::string _random_prefix_generator() {
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::random_device rd;
    std::mt19937 generator(rd());
    std::shuffle(str.begin(), str.end(), generator);
    return str.substr(0, 8);
}

// transform two lists to a well-formed func string like .wat
static std::string _makeIRString(const std::vector<std::string>& pre_list, 
                        const std::vector<std::string>& post_list,
                        int func_num, 
                        const std::string& random_prefix,
                        const std::vector<wasm::Type> &local_types) 
{
    std::string params_str = "";
    if (local_types.size() > 0) {
        params_str = " (param";
        for (const auto t : local_types) {
            assert(t.isBasic());
            params_str += " ";
            params_str += t.toString();
        }
        params_str += ")";
    }
    std::string funcs_str = "(func $" + random_prefix + std::to_string(func_num) + "_1" + params_str + "\n";
    for (const auto& instr_str : pre_list) {
        funcs_str += instr_str;
        funcs_str += "\n";
    }
    funcs_str += "unreachable)\n(func $" + random_prefix + std::to_string(func_num) + "_2" + params_str + "\n";
    for (const auto& instr_str : post_list) {
        funcs_str += instr_str;
        funcs_str += "\n";
    }
    funcs_str += "unreachable)\n";
    return funcs_str;
}

// transform all operations to a well-formed module string like .wat
static void _makeModuleString(std::string& module_str, 
                                const std::vector<InstrumentOperation>& operations, 
                                const std::string& random_prefix)
{
    while (module_str.back() != ')') 
        module_str.pop_back();
    assert(module_str.back() == ')');
    module_str.pop_back();
    int op_num = 1;
    for (const auto& operation : operations) {
        module_str += _makeIRString(operation.pre_instructions, operation.post_instructions,
                                    op_num, random_prefix, operation.local_types);
        op_num++;
    }
    module_str += ")";
}

// input operations and output the data structure of a vector of both pre_list and post_list
// which can be used directly for class Instrumenter to do instrument()
// this function is called by class Instrumenter when dealing with operations
// return nullptr aka InstrumentResult::instrument_error
AddedInstructions* OperationBuilder::makeOperations(wasm::Module* &mallocator, const std::vector<InstrumentOperation> &operations) noexcept {
    AddedInstructions* added_instructions = new AddedInstructions;
    added_instructions->vec.resize(operations.size());
    auto random_prefix = _random_prefix_generator();
    
    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    Colors::setEnabled(false);
    // also do stack ir pass on the module
    mallocator->typeNames.clear();
    wasm::printStackIR(mstream, mallocator, true);
    std::string module_str = mstream.str();
    std::string backup_str = module_str;

    _makeModuleString(module_str, operations, random_prefix);

    auto feature = mallocator->features;
    delete mallocator;
    mallocator = new wasm::Module();
    mallocator->features.set(feature);
    
    if (!_readTextData(module_str, *(mallocator))) {
        std::cerr << "OperationBuilder: makeOperations() read text error!" << std::endl;
        delete added_instructions;
        Colors::setEnabled(is_color);
        delete mallocator;
        mallocator = new wasm::Module();
        if (!_readTextData(backup_str, *(mallocator))) {
            std::cerr << "OperationBuilder: makeOperations() cannot recover module! Further operations should end!" << std::endl;
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
    for (const auto& _ : operations) {
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