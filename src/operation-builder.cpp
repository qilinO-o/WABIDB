#include "operation-builder.hpp"
#include <random>

namespace wasm_instrument {

static std::string _random_prefix_generator() {
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::random_device rd;
    std::mt19937 generator(rd());
    std::shuffle(str.begin(), str.end(), generator);
    return str.substr(0, 8);
}

static std::string _make_func_param(const std::vector<wasm::Type> &local_types) {
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
    return params_str;
}

static std::string _make_func_result(const std::vector<wasm::Type> &local_types) {
    std::string params_str = "";
    if (local_types.size() > 0) {
        params_str = " (result";
        for (const auto t : local_types) {
            assert(t.isBasic());
            params_str += " ";
            params_str += t.toString();
        }
        params_str += ")";
    }
    return params_str;
}

static std::string _make_func_str(const InstrumentFragment& fragment,
                                int func_num, 
                                const std::string &random_prefix,
                                const std::string &suffix) {
    const std::string const_exprs[5] = {
        "i32.const 0\n",
        "i64.const 0\n",
        "f32.const 0\n",
        "f64.const 0\n",
        "v128.const i32x4 0x00 0x00 0x00 0x00\n"
    };
    std::string params_str = _make_func_param(fragment.local_types);
    std::string result_str = _make_func_result(fragment.stack_context);
    std::string func_str = "(func $" + random_prefix + std::to_string(func_num) + suffix + params_str + result_str + "\n";
    for (const auto& t : fragment.stack_context) {
        assert(t.isBasic());
        func_str += const_exprs[t.getID() - 2];
    }
    for (const auto& instr_str : fragment.instructions) {
        func_str += instr_str;
        func_str += "\n";
    }
    func_str += ")\n";
    return func_str;
}

// transform two lists to a well-formed func string like .wat
static std::string _makeFuncsString(const InstrumentFragment& pre_list, 
                        const InstrumentFragment& post_list,
                        int func_num, 
                        const std::string& random_prefix) 
{
    std::string pre_func_str = _make_func_str(pre_list, func_num, random_prefix, "_1");
    std::string post_func_str = _make_func_str(post_list, func_num, random_prefix, "_2");
    return pre_func_str + post_func_str;
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
        module_str += _makeFuncsString(operation.pre_instructions, operation.post_instructions,
                                    op_num, random_prefix);
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
    added_instructions->resize(operations.size());
    auto random_prefix = _random_prefix_generator();
    
    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    Colors::setEnabled(false);
    // also do stack ir pass on the module
    mallocator->typeNames.clear();
    _out_stackir_module(mstream, mallocator);
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

    for (int op_num = 0; op_num < operations.size(); op_num++) {
        std::string op_num_str = std::to_string(op_num + 1);
        auto func1 = BinaryenGetFunction(mallocator, (random_prefix + op_num_str + "_1").c_str());
        auto func2 = BinaryenGetFunction(mallocator, (random_prefix + op_num_str + "_2").c_str());
        assert(func1 != nullptr);
        assert(func2 != nullptr);
        assert(func1->stackIR.get() != nullptr);
        assert(func2->stackIR.get() != nullptr);
        for (auto i = operations[op_num].pre_instructions.stack_context.size(); i < func1->stackIR.get()->size(); i++) {
            (*added_instructions)[op_num].pre_instructions.push_back((*(func1->stackIR.get()))[i]);
        }
        for (auto i = operations[op_num].post_instructions.stack_context.size(); i < func2->stackIR.get()->size(); i++) {
            (*added_instructions)[op_num].post_instructions.push_back((*(func2->stackIR.get()))[i]);
        }
        BinaryenRemoveFunction(mallocator, func1->name.toString().c_str());
        BinaryenRemoveFunction(mallocator, func2->name.toString().c_str());
    }

    return added_instructions;
}

}