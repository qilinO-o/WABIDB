#include "instrumenter.hpp"
#include "config-builder.hpp"
#include <fstream>
#include <assert.h>
#include <list>
#include <wasm-io.h>
#include <ir/module-utils.h>
#include <support/colors.h>

namespace wasm_instrument{
std::string InstrumentResult2str(InstrumentResult result) {
    std::string result_map[] = {
        "success",
        "config_error",
        "open_module_error",
        "instrument_error",
        "validate_error",
        "generation_error",
        "invalid_state"
    };
    return result_map[int(result)];
}

InstrumentResult Instrumenter::_read_file() noexcept {
    // wasm MVP feature
    const auto FEATURE_USED = wasm::FeatureSet::MVP;
    this->module_->features.enable(FEATURE_USED);
    this->mallocator_->features.enable(FEATURE_USED);

    wasm::ModuleReader reader;
    try {
        reader.read(this->config_.filename, *(this->module_), "");
    } catch(wasm::ParseException &p) {
        p.dump(std::cerr);
        std::cerr << '\n';
        return InstrumentResult::open_module_error;
    }

    if (this->module_->functions.empty()) {
        return InstrumentResult::open_module_error;
    }
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::_write_file() noexcept {
    wasm::ModuleWriter writer;
    try {
        writer.write(*(this->mallocator_), this->config_.targetname);
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

bool _isControlFlowStructure(wasm::Expression::Id id) {
    return (id == wasm::Expression::Id::BlockId) || (id == wasm::Expression::Id::IfId) 
        || (id == wasm::Expression::Id::LoopId);
}

bool _exp_match_targets(wasm::StackInst* exp, std::vector<InstrumentOperation::ExpName> &targets) {
    for (const auto &target : targets) {
        if (exp->origin->_id == target.id) {
            if (target.exp_op.no_op == -1) return true;
            else if (target.id == wasm::Expression::Id::UnaryId) {
                wasm::Unary* unary_exp = static_cast<wasm::Unary*>(exp->origin);
                if (unary_exp->op == target.exp_op.uop) return true;
            } else if (target.id == wasm::Expression::Id::BinaryId) {
                wasm::Binary* binary_exp = static_cast<wasm::Binary*>(exp->origin);
                if (binary_exp->op == target.exp_op.bop) return true;
            } else if (_isControlFlowStructure(target.id)) {
                if (exp->op == target.exp_op.cop) return true;
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

InstrumentResult Instrumenter::setConfig(InstrumentConfig &config) noexcept {
    if (this->state_ != InstrumentState::idle) {
        std::cerr << "Instrumenter: wrong state for setConfig()!" << std::endl;
        return InstrumentResult::invalid_state;
    }
    this->config_.filename = config.filename;
    this->config_.targetname = config.targetname;
    this->config_.operations.assign(config.operations.begin(), config.operations.end());
    if (this->config_.filename.empty() || this->config_.targetname.empty()) {
        return InstrumentResult::config_error;
    }

    // read file to the instrumenter
    InstrumentResult state_result = _read_file();
    if (state_result != InstrumentResult::success) {
        return state_result;
    }

    // transfer dependencies to mallocator
    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    Colors::setEnabled(false);
    // also do stack ir pass on the module
    wasm::printStackIR(mstream, (wasm::Module*)(this->module_), true);
    if (!_readTextData(mstream.str(), *(this->mallocator_))) {
        std::cerr << "Instrumenter: setConfig() read text error!" << std::endl;
        Colors::setEnabled(is_color);
        return InstrumentResult::config_error;
    }
    Colors::setEnabled(is_color);
    // do stack ir pass on mallocator
    wasm::PassRunner runner(this->mallocator_);
    runner.add("generate-stack-ir");
    runner.add("optimize-stack-ir");
    runner.run();

    this->state_ = InstrumentState::valid;
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::instrument() noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for instrument()!" << std::endl;
        return InstrumentResult::invalid_state;
    }

    ConfigBuilder builder;
    this->added_instructions_ = builder.makeConfig(*(this->mallocator_), this->config_);
    if (!this->added_instructions_) {
        return InstrumentResult::config_error;
    }

    // do specific instrument operations in config
    // iter through functions in the module
    auto func_visitor = [this](wasm::Function* func){
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
                if (!_exp_match_targets(cur_stack_inst, operation.targets)) {
                    op_num++;
                    continue;
                } 
                stack_ir_list.splice(i, _stack_ir_vec2list(
                    this->added_instructions_->vec[op_num].pre_instructions));
                std::advance(i, 1);
                stack_ir_list.splice(i, _stack_ir_vec2list(
                    this->added_instructions_->vec[op_num].post_instructions));
                std::advance(i, -1);
                break;
            }
        }
    
        // write back the modified stack ir list to the func
        auto new_stack_ir_vec = _stack_ir_list2vec(stack_ir_list);
        func->stackIR = std::make_unique<wasm::StackIR>(new_stack_ir_vec);
    };
    try {
        wasm::ModuleUtils::iterDefinedFunctions(*(this->module_), func_visitor);
    } catch(...) {
        return InstrumentResult::instrument_error;
    }

    // validate the module after modification
    if (!BinaryenModuleValidate(this->module_)) {
        return InstrumentResult::validate_error;
    }
    
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::writeBinary() noexcept {
    InstrumentResult state_result = _write_file();
    if (state_result != InstrumentResult::success) {
        return state_result;
    }
    this->state_ = InstrumentState::written;
    return InstrumentResult::success;
}

wasm::Global* Instrumenter::getGlobal(const char* name) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for getGlobal()!" << std::endl;
        return nullptr;
    }
    return BinaryenGetGlobal(this->module_, name);
}

wasm::Function* Instrumenter::getFunction(const char* name) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for getFunction()!" << std::endl;
        return nullptr;
    }
    return BinaryenGetFunction(this->module_, name);
}

wasm::Export* Instrumenter::getExport(const char* external_name) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for getExport()!" << std::endl;
        return nullptr;
    }
    return BinaryenGetExport(this->module_, external_name);
}

wasm::Function* Instrumenter::getStartFunction() noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for getStartFunction()!" << std::endl;
        return nullptr;
    }
    auto t = this->getExport("_start");
    return this->module_->getFunctionOrNull(t->value);
}

wasm::Importable* Instrumenter::getImport(wasm::ModuleItemKind kind, const char* base_name) noexcept {
    switch (kind) {
        case wasm::ModuleItemKind::Function:
            for (const auto& f : this->module_->functions) {
                if (f.get()->imported() && f.get()->base == base_name) {
                    return f.get();
                }
            }
        case wasm::ModuleItemKind::Table:
            for (const auto& f : this->module_->tables) {
                if (f.get()->imported() && f.get()->base == base_name) {
                    return f.get();
                }
            }
        case wasm::ModuleItemKind::Memory:
            for (const auto& f : this->module_->memories) {
                if (f.get()->imported() && f.get()->base == base_name) {
                    return f.get();
                }
            }
        case wasm::ModuleItemKind::Global:
            for (const auto& f : this->module_->globals) {
                if (f.get()->imported() && f.get()->base == base_name) {
                    return f.get();
                }
            }
        case wasm::ModuleItemKind::Tag:
        case wasm::ModuleItemKind::DataSegment:
        case wasm::ModuleItemKind::ElementSegment:
        case wasm::ModuleItemKind::Invalid:
        default:
            return nullptr;
    }
}

wasm::Global* Instrumenter::addGlobal(const char* name, 
                            BinaryenType type, 
                            bool if_mutable, 
                            BinaryenLiteral value) noexcept 
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addGlobal()!" << std::endl;
        return nullptr;
    }
    auto ret = this->getGlobal(name);
    if (ret != nullptr) {
        std::cerr << "Instrumenter: global name: "<< name << " already exists!" << std::endl;
        return nullptr;
    }
    auto init = BinaryenConst(this->mallocator_, value);
    ret = BinaryenAddGlobal(this->module_, name, type, if_mutable, init);
    // maintain dependency in mallocator
    BinaryenAddGlobal(this->mallocator_, name, type, if_mutable, init);
    return ret;
}

void Instrumenter::addFunctions(std::vector<std::string> &names, std::vector<std::string> &func_bodies) noexcept {
    assert(names.size() == func_bodies.size());
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addFunction()!" << std::endl;
        return;
    }
    for (auto i = 0; i < names.size(); i++) {
        auto cur = this->getFunction(names[i].c_str());
        if (cur != nullptr) {
            std::cerr << "Instrumenter: function name: "<< names[i] << " already exists!" << std::endl;
            return;
        }
    }

    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    Colors::setEnabled(false);
    // also do stack ir pass on the module
    wasm::printStackIR(mstream, (wasm::Module*)(this->module_), true);
    std::string module_str = mstream.str();
    while (module_str.back() != ')') 
        module_str.pop_back();
    assert(module_str.back() == ')');
    module_str.pop_back();
    for (auto i = 0; i < names.size(); i++) {
        module_str += func_bodies[i];
        module_str += "\n";
    }
    module_str += ")";
    BinaryenModuleDispose(this->mallocator_);
    this->mallocator_ = BinaryenModuleCreate();
    this->mallocator_->features.set(this->module_->features);
    if (!_readTextData(module_str, *(this->mallocator_))) {
        std::cerr << "Instrumenter: addFunctions() read text error!" << std::endl;
        Colors::setEnabled(is_color);
        return;
    }
    Colors::setEnabled(is_color);

    // do stack ir pass on the module
    wasm::PassRunner pass_runner(this->mallocator_);
    pass_runner.add("generate-stack-ir");
    pass_runner.add("optimize-stack-ir");
    pass_runner.run();
    
    for (auto i = 0; i < names.size(); i++) {
        auto cur = this->mallocator_->getFunctionOrNull(names[i]);
        if (cur == nullptr) {
            std::cerr << "Instrumenter: add function: "<< i << " name does not match its body" << std::endl;
            continue;
        }
        this->module_->addFunction(cur);
    }
}

void Instrumenter::addImportFunction(const char* internal_name,
                                    const char* external_module_name,
                                    const char* external_base_name,
                                    BinaryenType params,
                                    BinaryenType results) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addImportFunction()!" << std::endl;
        return;
    }
    BinaryenAddFunctionImport(this->module_, internal_name, external_module_name, external_base_name, params, results);
    // maintain dependency in mallocator
    BinaryenAddFunctionImport(this->mallocator_, internal_name, external_module_name, external_base_name, params, results);
}

void Instrumenter::addImportGlobal(const char* internal_name,
                                const char* external_module_name,
                                const char* external_base_name,
                                BinaryenType type,
                                bool if_mutable) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addImportGlobal()!" << std::endl;
        return;
    }
    BinaryenAddGlobalImport(this->module_, internal_name, external_module_name, external_base_name, type, if_mutable);
    // maintain dependency in mallocator
    BinaryenAddGlobalImport(this->mallocator_, internal_name, external_module_name, external_base_name, type, if_mutable);
}

void Instrumenter::addImportMemory(const char* internal_name,
                                const char* external_module_name,
                                const char* external_base_name,
                                bool if_shared) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addImportGlobal()!" << std::endl;
        return;
    }
    BinaryenAddMemoryImport(this->module_, internal_name, external_module_name, external_base_name, if_shared);
    // maintain dependency in mallocator
    BinaryenAddMemoryImport(this->mallocator_, internal_name, external_module_name, external_base_name, if_shared);
}

wasm::Export* Instrumenter::addExport(wasm::ModuleItemKind kind, const char* internal_name,
                                    const char* external_name) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addExport()!" << std::endl;
        return nullptr;
    }
    wasm::Export* ret = nullptr;
    switch (kind) {
        case wasm::ModuleItemKind::Function:
            ret = BinaryenAddFunctionExport(this->module_, internal_name, external_name);
            // maintain dependency in mallocator
            BinaryenAddFunctionExport(this->mallocator_, internal_name, external_name);
            return ret;
        case wasm::ModuleItemKind::Global:
            ret = BinaryenAddGlobalExport(this->module_, internal_name, external_name);
            // maintain dependency in mallocator
            BinaryenAddGlobalExport(this->mallocator_, internal_name, external_name);
            return ret;
        case wasm::ModuleItemKind::Memory:
            ret = BinaryenAddMemoryExport(this->module_, internal_name, external_name);
            // maintain dependency in mallocator
            BinaryenAddMemoryExport(this->mallocator_, internal_name, external_name);
            return ret;
        default:
            return nullptr;
    }
}

InstrumentResult Instrumenter::instrumentFunction(wasm::Function* func, 
                                                int line_num, 
                                                std::vector<std::string>& added) noexcept
{
    std::printf("## 0 ##\n");
    auto func_size = func->stackIR.get()->size();
    if (line_num < 0 || line_num > func_size) {
        std::cerr << "Instrumenter: invalid line num!" << std::endl;
        return InstrumentResult::instrument_error;
    }
    std::printf("## 1 ##\n");
    std::string module_str = "(module\n(func $temp\n";
    for (const auto& instr_str : added) {
        module_str += instr_str;
        module_str += "\n";
    }
    module_str += "unreachable)\n)";
    std::printf("## 2 ##\n");
    BinaryenModulePrint(this->mallocator_);
    if (!_readTextData(module_str, *(this->mallocator_))) {
        std::cerr << "Instrumenter: instrumentFunction() read text error!" << std::endl;
        return InstrumentResult::instrument_error;
    }
    std::printf("## 3 ##\n");
    BinaryenModulePrint(this->mallocator_);
    // do stack ir pass on the module
    auto pass_runner = new wasm::PassRunner(this->mallocator_);
    pass_runner->add("generate-stack-ir");
    pass_runner->add("optimize-stack-ir");
    pass_runner->runOnFunction(this->mallocator_->getFunctionOrNull("temp"));
    delete pass_runner;
    std::printf("## 4 ##\n");
    auto temp_func = this->mallocator_->getFunctionOrNull("temp");
    assert(temp_func != nullptr);
    std::printf("## 5 ##\n");
    std::list<wasm::StackInst*> stack_ir_list = _stack_ir_vec2list(*(func->stackIR.get()));
    std::list<wasm::StackInst*> temp_ir_list = _stack_ir_vec2list(*(temp_func->stackIR.get()));
    temp_ir_list.pop_back();
    std::printf("## 6 ##\n");
    auto i = stack_ir_list.begin();
    std::advance(i, line_num);
    std::printf("insert at exp: %d\n", (*i)->origin->_id);
    for (auto i : stack_ir_list) {
        std::printf("add at exp: %d\n", i->origin->_id);
    }
    stack_ir_list.splice(i, temp_ir_list);
    std::printf("## 7 ##\n");
    for (auto i : stack_ir_list) {
        std::printf("add at exp: %s\n", wasm::getExpressionName(i->origin));
    }
    // write back the modified stack ir list to the func
    auto new_stack_ir_vec = _stack_ir_list2vec(stack_ir_list);
    func->stackIR = std::make_unique<wasm::StackIR>(new_stack_ir_vec);
    
    return InstrumentResult::success;
}

}