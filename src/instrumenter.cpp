#include "instrumenter.hpp"
#include "config-builder.hpp"
#include <fstream>
#include <assert.h>
#include <list>
#include <wasm-io.h>
#include <ir/module-utils.h>

namespace wasm_instrument{
std::string InstrumentResult2str(InstrumentResult result) {
    std::string result_map[] = {
        "success",
        "config_error",
        "open_module_error",
        "instrument_error",
        "validate_error",
        "generation_error"
    };
    return result_map[int(result)];
}

InstrumentResult Instrumenter::_read_file() noexcept {
    // wasm MVP feature
    this->module_.features.enable(wasm::FeatureSet::MVP);

    wasm::ModuleReader reader;
    try {
        reader.read(this->config_.filename, this->module_, "");
    } catch(wasm::ParseException &p) {
        p.dump(std::cerr);
        std::cerr << '\n';
        return InstrumentResult::open_module_error;
    }

    if (this->module_.functions.empty()) {
        return InstrumentResult::open_module_error;
    }
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::_write_file() noexcept {
    wasm::ModuleWriter writer;
    try {
        writer.write(this->module_, this->config_.targetname);
    } catch(wasm::ParseException &p) {
        p.dump(std::cerr);
        std::cerr << '\n';
        return InstrumentResult::generation_error;
    }
    return InstrumentResult::success;
}

bool _exp_match_targets(wasm::Expression* exp, std::vector<InstrumentOperation::ExpName> &targets) {
    for (const auto &target : targets) {
        if (exp->_id == target.id) {
            if (target.id == wasm::Expression::Id::UnaryId) {
                wasm::Unary* unary_exp = static_cast<wasm::Unary*>(exp);
                if (unary_exp->op == target.exp_op.uop) return true;
            } else if (target.id == wasm::Expression::Id::BinaryId) {
                wasm::Binary* binary_exp = static_cast<wasm::Binary*>(exp);
                if (binary_exp->op == target.exp_op.bop) return true;
            } else { // exp and target instr are of no op instr
                if (target.exp_op.no_op == -1) return true;
            }
        }
    }
    return false;
}

std::list<wasm::StackInst*> _stack_ir_vec2list(const wasm::StackIR &stack_ir) {
    std::list<wasm::StackInst*> stack_ir_list;
    for (const auto &stack_instr : stack_ir) {
        if (stack_instr == nullptr) continue;
        stack_ir_list.push_back(stack_instr);
    }
    return stack_ir_list;
}

void _stack_ir_list2vec(const std::list<wasm::StackInst*> &stack_ir, wasm::StackIR &stack_ir_vec) {
    stack_ir_vec.assign(stack_ir.begin(), stack_ir.end());
}

wasm::StackIR _stack_ir_list2vec(const std::list<wasm::StackInst*> &stack_ir) {
    wasm::StackIR stack_ir_vec(stack_ir.begin(), stack_ir.end());
    return stack_ir_vec;
}

void _print_stack_ir(const std::list<wasm::StackInst*> stack_ir_list, bool verbose = false) {
    auto stack_ir_op2str = [](wasm::StackInst::Op op){
        std::string op_map[] = {
            "Basic",
            "BlockBegin",
            "BlockEnd",
            "IfBegin",
            "IfElse",
            "IfEnd",
            "LoopBegin",
            "LoopEnd",
            "TryBegin",
            "Catch",
            "CatchAll",
            "Delegate",
            "TryEnd",
            "TryTableBegin",
            "TryTableEnd"
        };
        return op_map[int(op)];
    };
    for (auto i = stack_ir_list.begin(); i != stack_ir_list.end(); i++) {
        auto cur_stack_inst = *i;
        auto cur_exp = cur_stack_inst->origin;
        if (!verbose) {
            std::printf("op: %s, exp: %s\n", stack_ir_op2str(cur_stack_inst->op).c_str(), wasm::getExpressionName(cur_exp));
        } else {
            std::printf("op: %s, exp: ", stack_ir_op2str(cur_stack_inst->op).c_str());
            cur_exp->dump();
        }
    }
}

InstrumentResult Instrumenter::instrument() noexcept {
    if (!this->is_set_ || this->config_.filename.empty() || this->config_.targetname.empty()) {
        return InstrumentResult::config_error;
    }
    ConfigBuilder builder;
    auto added_instructions = builder.makeConfig(this->config_);
    if (!added_instructions) {
        return InstrumentResult::config_error;
    }

    // read file to the instrumenter
    InstrumentResult state_result = _read_file();
    if (state_result != InstrumentResult::success) {
        return state_result;
    }

    // do stack ir pass on the module
    auto pass_runner = new wasm::PassRunner(&(this->module_));
    pass_runner->add("generate-stack-ir");
    pass_runner->add("optimize-stack-ir");
    pass_runner->run();
    delete pass_runner;

    // do specific instrument operations in config
    // iter through functions in the module
    auto func_visitor = [this, &added_instructions](wasm::Function* func){
        std::cout << "in function: " << func->name << " type: " << func->type.toString() << std::endl;
    
        // stack ir check
        assert(func->stackIR.get() != nullptr);

        // iter through the body in the current function (with Stack IR)
        // transform the vector of stack ir to a list for better modification
        std::list<wasm::StackInst*> stack_ir_list = _stack_ir_vec2list(*(func->stackIR.get()));
        // _print_stack_ir(stack_ir_list);
        
        for (auto i = stack_ir_list.begin(); i != stack_ir_list.end(); i++) {
            auto cur_stack_inst = *i;
            auto cur_exp = cur_stack_inst->origin;
            std::printf("iter at exp: %s\n", wasm::getExpressionName(cur_exp));

            // perform each operation on the current expression
            // targets of all operations should be *Orthogonal* !
            int op_num = 0;
            for (auto &operation : this->config_.operations) {
                if (!_exp_match_targets(cur_exp, operation.targets)) {
                    op_num++;
                    continue;
                } 
                stack_ir_list.splice(i, _stack_ir_vec2list(added_instructions->vec[op_num].pre_instructions));
                std::advance(i, 1);
                stack_ir_list.splice(i, _stack_ir_vec2list(added_instructions->vec[op_num].post_instructions));
                std::advance(i, -1);
                break;
            }
        }
    
        // write back the modified stack ir list to the func
        auto new_stack_ir_vec = _stack_ir_list2vec(stack_ir_list);
        func->stackIR = std::make_unique<wasm::StackIR>(new_stack_ir_vec);
    };
    try {
        wasm::ModuleUtils::iterDefinedFunctions(this->module_, func_visitor);
    } catch(...) {
        return InstrumentResult::instrument_error;
    }

    // validate the module after modification
    if (!BinaryenModuleValidate(&(this->module_))) {
        return InstrumentResult::validate_error;
    }

    // write the module to binary file with name config.targetname
    state_result = _write_file();
    if (state_result != InstrumentResult::success) {
        return state_result;
    }
    
    return InstrumentResult::success;
}

}