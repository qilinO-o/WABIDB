#ifndef instr_utils_h
#define instr_utils_h

#include <list>
#include <wasm.h>
#include <wasm-stack.h>
#include "binaryen-c.h"

namespace wasm_instrument {

const wasm::FeatureSet FEATURE_SPEC = wasm::FeatureSet::BulkMemory |
                                    wasm::FeatureSet::Multivalue |
                                    wasm::FeatureSet::SignExt |
                                    wasm::FeatureSet::MutableGlobals |
                                    wasm::FeatureSet::SIMD |
                                    wasm::FeatureSet::ReferenceTypes |
                                    wasm::FeatureSet::TruncSat;

struct InstrumentFragment {
    std::vector<std::string> instructions;
    // declare number and types of locals(index) used in upon vectors for validation
    // must all be basic type (i32 i64 f32 f64 v128)
    std::vector<wasm::Type> local_types = {};
    // stack context that the fragment will use
    // the deduction rule must be followed C|- fragment : [stack_context] -> [stack_context]
    // now only support basic type (i32 i64 f32 f64 v128) (need reftype)
    std::vector<wasm::Type> stack_context = {};
};

// struct to define a certain operation
// add /add_instructions/ at /location/ of the current expression that matches /targets/
struct InstrumentOperation final {
    struct ExpName {
        wasm::Expression::Id id;
        union ExpOp {
            wasm::UnaryOp uop; // identify which unary or binary op it is
            wasm::BinaryOp bop;
            wasm::StackInst::Op cop; // control flow op mark its begin or end
            // to be added more op id
        };
        // nullopt to ignore Op check
        std::optional<ExpOp> exp_op;
        // nullopt to ignore type check
        std::optional<BinaryenType> exp_type;
    };
    // targets of all operations should be *Orthogonal* !
    std::vector<ExpName> targets;
    InstrumentFragment pre_instructions;
    InstrumentFragment post_instructions;
};

// 1 to 1 related to config.operations
// data structure for transformed instruction string to stack ir
struct AddedInstruction {
    std::vector<wasm::StackInst*> pre_instructions;
    std::vector<wasm::StackInst*> post_instructions;
};
using AddedInstructions = std::vector<AddedInstruction>;


std::ostream& _out_stackir_module(std::ostream &o, wasm::Module *module);

bool _readTextData(const std::string& input, wasm::Module& wasm);

bool _isControlFlowStructure(wasm::Expression::Id id);

bool _exp_match_target(const wasm::StackInst* exp, const InstrumentOperation::ExpName &target);

bool _exp_match_targets(const wasm::StackInst* exp, const std::vector<InstrumentOperation::ExpName> &targets);

std::list<wasm::StackInst*> _stack_ir_vec2list(const wasm::StackIR &stack_ir);

wasm::StackIR _stack_ir_list2vec(const std::list<wasm::StackInst*> &stack_ir);

// general iteration methods
// no state check and validation check, so be careful!
// The visitor provided should have signature void(Function*)
template<typename T>
inline void iterDefinedFunctions(wasm::Module* m, T visitor) {
    for (auto &func : m->functions) {
        if (!func->imported()) {
            visitor(func.get());
        }
    }
}

// The visitor provided should have signature void(std::list<wasm::StackInst *>, std::list<wasm::StackInst *>::iterator)
template<typename T>
inline void iterInstructions(wasm::Function* func, T visitor) {
    assert(func->stackIR != nullptr);
    std::list<wasm::StackInst*> stack_ir_list = _stack_ir_vec2list(*(func->stackIR.get()));
    for (auto iter = stack_ir_list.begin(); iter != stack_ir_list.end(); iter++)  {
        visitor(stack_ir_list, iter);
    }
    auto new_stack_ir_vec = _stack_ir_list2vec(stack_ir_list);
    func->stackIR = std::make_unique<wasm::StackIR>(new_stack_ir_vec);
}

wasm::StackInst* _make_stack_inst(wasm::StackInst::Op op, wasm::Expression* origin, wasm::Module* m);

}

#endif