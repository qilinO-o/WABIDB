#include "instr-utils.hpp"
#include <parser/wat-parser.h>
#include <pass.h>
#include <passes/passes.h>

namespace wasm_instrument {

std::ostream& _out_stackir_module(std::ostream &o, wasm::Module *module) {
    bool is_cout = (o.rdbuf() == std::cout.rdbuf());
    std::streambuf* old_streambuf = nullptr;
    if (!is_cout) {
        old_streambuf = std::cout.rdbuf();
        std::cout.rdbuf(o.rdbuf());
    }
    wasm::PassRunner runner(module);
    runner.add(std::unique_ptr<wasm::Pass>(wasm::createPrintStackIRPass()));
    runner.run();
    if (!is_cout) {
        std::cout.rdbuf(old_streambuf);
    }
    return o;
}

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

bool _isControlFlowStructure(wasm::Expression::Id id) {
    return (id == wasm::Expression::Id::BlockId) || (id == wasm::Expression::Id::IfId) 
        || (id == wasm::Expression::Id::LoopId) 
        || (id == wasm::Expression::Id::TryId) || (id == wasm::Expression::Id::TryTableId);
}

bool _exp_match_target(const wasm::StackInst* exp, const InstrumentOperation::ExpName &target) {
    bool id_match = (exp->origin->_id == target.id);
    if (!id_match) return false;
    
    bool op_match = false;
    if (target.exp_op.has_value()) {
        if (target.id == wasm::Expression::Id::UnaryId) {
            wasm::Unary* unary_exp = static_cast<wasm::Unary*>(exp->origin);
            if (unary_exp->op == target.exp_op->uop) op_match = true;
        } else if (target.id == wasm::Expression::Id::BinaryId) {
            wasm::Binary* binary_exp = static_cast<wasm::Binary*>(exp->origin);
            if (binary_exp->op == target.exp_op->bop) op_match = true;
        } else if (_isControlFlowStructure(target.id)) {
            if (exp->op == target.exp_op->cop) op_match = true;
        }
    } else {
        op_match = true;
    }
    if (!op_match) return false;
    
    bool type_match = false;
    if (target.exp_type.has_value()) {
        if (exp->origin->type == wasm::Type(target.exp_type.value())) type_match = true;
    } else {
        type_match = true;
    }
    if (!type_match) return false;
    return true;
}

bool _exp_match_targets(const wasm::StackInst* exp, const std::vector<InstrumentOperation::ExpName> &targets) {
    for (const auto &target : targets) {
        if (_exp_match_target(exp, target)) return true;
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

wasm::StackIR _stack_ir_list2vec(const std::list<wasm::StackInst*> &stack_ir) {
    wasm::StackIR stack_ir_vec(stack_ir.begin(), stack_ir.end());
    return stack_ir_vec;
}

wasm::StackInst* _make_stack_inst(wasm::StackInst::Op op, wasm::Expression* origin, wasm::Module* m) {
    auto* ret = m->allocator.alloc<wasm::StackInst>();
    ret->op = op;
    ret->origin = origin;
    auto stackType = origin->type;
    if (_isControlFlowStructure(origin->_id)) {
        if (stackType == wasm::Type::unreachable) {
            stackType = wasm::Type::none;
        } else if (op != wasm::StackInst::BlockEnd && op != wasm::StackInst::IfEnd &&
                op != wasm::StackInst::LoopEnd && op != wasm::StackInst::TryEnd &&
                op != wasm::StackInst::TryTableEnd) {
            stackType = wasm::Type::none;
        }
    }
    ret->type = stackType;
    return ret;
}

}