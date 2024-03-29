#include "instrumenter.hpp"
#include <fstream>
#include <assert.h>
#include <wasm-io.h>
#include <ir/module-utils.h>
#include <wasm-stack.h>

using namespace wasm_instrument;

InstrumentResult Instrumenter::_read_file() noexcept {
    // wasm MVP feature
    this->module_.features.enable(wasm::FeatureSet::MVP);

    wasm::ModuleReader reader;
    try {
        reader.read(this->config_.filename, this->module_, "");
    } catch(wasm::ParseException &p) {
        p.dump(std::cerr);
        std::cerr << '\n';
    }

    if (this->module_.functions.empty()) {
        return InstrumentResult::open_module_error;
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

InstrumentResult Instrumenter::instrument() noexcept {
    if (this->config_.filename.empty() || this->config_.targetname.empty()) {
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
    pass_runner->run();
    delete pass_runner;

    // do specific instrument operations in config
    for (InstrumentOperation &operation : this->config_.operations) {
        // once for one operation
        // iter through functions in the module
        wasm::ModuleUtils::iterDefinedFunctions(this->module_,
        [&operation](const wasm::Function* func){
            std::cout << "in function: " << func->name << " type: " << func->type.toString();
      
            // stack ir check
            if (!func->stackIR) {
                std::printf(" with no stack ir exists!\n");
            } else {
                std::printf(" with stack ir exp num: %ld\n", func->stackIR.get()->size());
            }

            // iter through the body in the current function (with Binaryen IR)
            /*
            assert(func->body->_id == wasm::Expression::Id::BlockId);
            wasm::Block* func_body = static_cast<wasm::Block*>(func->body);
            std::printf("; with body exp num: %ld\n", func_body->list.size());

            for(auto i = 0; i != func_body->list.size(); i++) {
                wasm::Expression* cur_exp = func_body->list[i];
                std::printf("iter at exp: %s\n", wasm::getExpressionName(cur_exp));
                if (!_exp_match_targets(cur_exp, operation.targets)) continue;

                // do instructions insertion
                if (operation.location == InstrumentOperation::Loaction::before) {
                    
                } else if (operation.location == InstrumentOperation::Loaction::after) {

                }
            }
            */

            // iter through the body in the current function (with Stack IR)
            auto stack_ir = func->stackIR.get();
            for(auto i = 0; i != stack_ir->size(); i++) {
                auto cur_stack_inst = stack_ir->operator[](i);
                auto cur_exp = cur_stack_inst->origin;
                std::printf("iter at exp: %s\n", wasm::getExpressionName(cur_exp));
                if (!_exp_match_targets(cur_exp, operation.targets)) continue;

                // do instructions insertion
                if (operation.location == InstrumentOperation::Loaction::before) {
                    
                } else if (operation.location == InstrumentOperation::Loaction::after) {

                }
            }
        }
        );
    }
    
    return InstrumentResult::success;
}