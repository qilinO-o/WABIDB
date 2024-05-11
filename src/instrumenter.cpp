#include "instrumenter.hpp"
#include "operation-builder.hpp"
#include <wasm-io.h>
#include <support/colors.h>

namespace wasm_instrument {

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
    this->module_->features.enable(this->config_.feature);

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
        writer.write(*(this->module_), this->config_.targetname);
    } catch(wasm::ParseException &p) {
        p.dump(std::cerr);
        std::cerr << '\n';
        return InstrumentResult::generation_error;
    }
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::setConfig(const InstrumentConfig &config) noexcept {
    if (this->state_ != InstrumentState::idle) {
        std::cerr << "Instrumenter: wrong state for setConfig()!" << std::endl;
        return InstrumentResult::invalid_state;
    }
    this->config_.filename = config.filename;
    this->config_.targetname = config.targetname;
    if (this->config_.filename.empty() || this->config_.targetname.empty()) {
        std::cerr << "Instrumenter: setConfig() empty file name!" << std::endl;
        return InstrumentResult::config_error;
    }

    // read file to the instrumenter
    InstrumentResult state_result = _read_file();
    if (state_result != InstrumentResult::success) {
        std::cerr << "Instrumenter: setConfig() error when read file!" << std::endl;
        return state_result;
    }

    // do stack ir pass on mallocator
    wasm::PassRunner runner(this->module_);
    runner.add("generate-stack-ir");
    runner.add("optimize-stack-ir");
    runner.run();

    // add functions of the original binary to function_scope
    for (const auto &f : this->module_->functions) {
        if (!f.get()->imported()) {
            this->function_scope_.emplace(f.get()->name.toString());
        }
    }

    this->state_ = InstrumentState::valid;
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::instrument(const std::vector<InstrumentOperation> &operations) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for instrument()!" << std::endl;
        return InstrumentResult::invalid_state;
    }

    // parse operations
    OperationBuilder builder;
    auto added_instructions = builder.makeOperations(this->module_, operations);
    if (!added_instructions) {
        std::cerr << "Instrumenter: instrumentFunction() parse operations error!" << std::endl;
        return InstrumentResult::instrument_error;
    }

    // do specific instrument operations in config
    // iter through functions in the module
    auto func_visitor = [this, &operations, &added_instructions](wasm::Function* func){
        if (!this->scopeContains(func->name.toString())) return;
        // std::cout << "in function: " << func->name << " type: " << func->type.toString() << std::endl;
    
        // stack ir check
        assert(func->stackIR != nullptr);

        // iter through the body in the current function (with Stack IR)
        // transform the vector of stack ir to a list for better modification
        std::list<wasm::StackInst*> stack_ir_list = _stack_ir_vec2list(*(func->stackIR.get()));
        
        for (auto i = stack_ir_list.begin(); i != stack_ir_list.end(); i++) {
            auto cur_stack_inst = *i;

            // perform each operation on the current expression
            // targets of all operations should be *Orthogonal* !
            int op_num = 0;
            for (auto &operation : operations) {
                if (!_exp_match_targets(cur_stack_inst, operation.targets)) {
                    op_num++;
                    continue;
                } 
                stack_ir_list.splice(i, _stack_ir_vec2list(
                    (*added_instructions)[op_num].pre_instructions));
                std::advance(i, 1);
                stack_ir_list.splice(i, _stack_ir_vec2list(
                    (*added_instructions)[op_num].post_instructions));
                std::advance(i, -1);
                break;
            }
        }
    
        // write back the modified stack ir list to the func
        auto new_stack_ir_vec = _stack_ir_list2vec(stack_ir_list);
        func->stackIR = std::make_unique<wasm::StackIR>(new_stack_ir_vec);
    };
    try {
        iterDefinedFunctions(this->module_, func_visitor);
    } catch(...) {
        std::cerr << "Instrumenter: instrument() error while iterating functions!" << std::endl;
        delete added_instructions;
        return InstrumentResult::instrument_error;
    }
    delete added_instructions;

    // validate the module after modification
    if (!BinaryenModuleValidate(this->module_)) {
        std::cerr << "Instrumenter: instrument() error when validate!" << std::endl;
        return InstrumentResult::validate_error;
    }
    
    return InstrumentResult::success;
}

InstrumentResult Instrumenter::writeBinary() noexcept {
    InstrumentResult state_result = _write_file();
    if (state_result != InstrumentResult::success) {
        std::cerr << "Instrumenter: writeBinary() error when write file!" << std::endl;
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

wasm::Memory* Instrumenter::getMemory(const char* name) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for getMemory()!" << std::endl;
        return nullptr;
    }
    // get the default memory
    if (name == nullptr) {
        if (this->module_->memories.size() != 0) {
            return this->module_->memories[0].get();
        }
        return nullptr;
    }
    return this->module_->getMemoryOrNull(name);
}

wasm::DataSegment* Instrumenter::getDateSegment(const char* name) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for getDataSegment()!" << std::endl;
        return nullptr;
    }
    return this->module_->getDataSegmentOrNull(name);
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
    if (t == nullptr) return nullptr;
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
    auto init = BinaryenConst(this->module_, value);
    ret = BinaryenAddGlobal(this->module_, name, type, if_mutable, init);
    return ret;
}

bool Instrumenter::addFunctions(const std::vector<std::string> &names, const std::vector<std::string> &func_bodies) noexcept {
    assert(names.size() == func_bodies.size());
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addFunction()!" << std::endl;
        return false;
    }
    for (auto i = 0; i < names.size(); i++) {
        auto cur = this->getFunction(names[i].c_str());
        if (cur != nullptr) {
            std::cerr << "Instrumenter: function name: "<< names[i] << " already exists!" << std::endl;
            return false;
        }
    }

    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    Colors::setEnabled(false);
    _out_stackir_module(mstream, this->module_);
    std::string module_str = mstream.str();
    std::string backup_str = module_str;
    while (module_str.back() != ')') 
        module_str.pop_back();
    assert(module_str.back() == ')');
    module_str.pop_back();
    for (auto i = 0; i < names.size(); i++) {
        module_str += func_bodies[i];
        module_str += "\n";
    }
    module_str += ")";
    auto feature = this->module_->features;
    delete this->module_;
    this->module_ = new wasm::Module();
    this->module_->features.set(feature);
    if (!_readTextData(module_str, *(this->module_))) {
        std::cerr << "Instrumenter: addFunctions() read text error!" << std::endl;
        Colors::setEnabled(is_color);
        delete this->module_;
        this->module_ = new wasm::Module();
        if (!_readTextData(backup_str, *(this->module_))) {
            std::cerr << "Instrumenter: addFunctions() cannot recover module! Further operations should end!" << std::endl;
        }
        return false;
    }
    Colors::setEnabled(is_color);

    // do stack ir pass on the module
    wasm::PassRunner pass_runner(this->module_);
    pass_runner.add("generate-stack-ir");
    pass_runner.add("optimize-stack-ir");
    pass_runner.run();
    return true;
}

wasm::Memory* Instrumenter::addMemory(const char* name, bool if_shared, int init_pages, int max_pages) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addMemory()!" << std::endl;
        return nullptr;
    }
    if ((!this->config_.feature.hasMultiMemory()) && (this->module_->memories.size() != 0)) {
        std::cerr << "Instrumenter: cannot have multiple memories" << std::endl;
        return nullptr;
    }
    auto ret = this->getMemory(name);
    if (ret != nullptr) {
        std::cerr << "Instrumenter: memory name: "<< name << " already exists!" << std::endl;
        return nullptr;
    }
    
    auto memory = std::make_unique<wasm::Memory>();
    memory->name = name;
    memory->shared = if_shared;
    memory->initial = std::max(0, init_pages);
    memory->max = std::min(max_pages, static_cast<int>(wasm::Memory::kMaxSize32));
    ret = this->module_->addMemory(std::move(memory));
    return ret;
}

wasm::DataSegment* Instrumenter::addPassiveDateSegment(const char* name, const char* data, size_t len) noexcept {
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addPassiveDataSegment()!" << std::endl;
        return nullptr;
    }
    BinaryenAddDataSegment(this->module_, name, nullptr, true, nullptr, data, len);
    return this->module_->getDataSegmentOrNull(name);
}

bool Instrumenter::addImportFunction(const char* internal_name,
                                    const char* external_module_name,
                                    const char* external_base_name,
                                    BinaryenType params,
                                    BinaryenType results) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addImportFunction()!" << std::endl;
        return false;
    }
    BinaryenAddFunctionImport(this->module_, internal_name, external_module_name, external_base_name, params, results);
    return true;
}

bool Instrumenter::addImportGlobal(const char* internal_name,
                                const char* external_module_name,
                                const char* external_base_name,
                                BinaryenType type,
                                bool if_mutable) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addImportGlobal()!" << std::endl;
        return false;
    }
    BinaryenAddGlobalImport(this->module_, internal_name, external_module_name, external_base_name, type, if_mutable);
    return true;
}

bool Instrumenter::addImportMemory(const char* internal_name,
                                const char* external_module_name,
                                const char* external_base_name,
                                bool if_shared) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for addImportGlobal()!" << std::endl;
        return false;
    }
    if ((!this->config_.feature.hasMultiMemory()) && (this->module_->getMemoryOrNull(internal_name) == nullptr)) {
        std::cerr << "Instrumenter: cannot have multiple memories" << std::endl;
        return false;
    }
    BinaryenAddMemoryImport(this->module_, internal_name, external_module_name, external_base_name, if_shared);
    return true;
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
            return ret;
        case wasm::ModuleItemKind::Global:
            ret = BinaryenAddGlobalExport(this->module_, internal_name, external_name);
            return ret;
        case wasm::ModuleItemKind::Memory:
            ret = BinaryenAddMemoryExport(this->module_, internal_name, external_name);
            return ret;
        default:
            return nullptr;
    }
}

InstrumentResult Instrumenter::instrumentFunction(const InstrumentOperation &operation,
                                                const char* name,
                                                size_t pos) noexcept
{
    if (this->state_ != InstrumentState::valid) {
        std::cerr << "Instrumenter: wrong state for instrumentFunction()!" << std::endl;
        return InstrumentResult::invalid_state;
    }

    // parse operation
    OperationBuilder builder;
    auto added_instructions = builder.makeOperations(this->module_, {operation});
    if (!added_instructions) {
        std::cerr << "Instrumenter: instrumentFunction() parse operation error!" << std::endl;
        return InstrumentResult::instrument_error;
    }

    auto func = this->module_->getFunctionOrNull(name);
    if (func == nullptr) {
        std::cerr << "Instrumenter: function name: "<< name << " does not exists!" << std::endl;
        delete added_instructions;
        return InstrumentResult::instrument_error;
    }
    if (func->imported()) {
        std::cerr << "Instrumenter: function name: "<< name << " is a import!" << std::endl;
        delete added_instructions;
        return InstrumentResult::instrument_error;
    }

    // std::cout << "in function: " << func->name << " type: " << func->type.toString() << std::endl;
    // stack ir check
    assert(func->stackIR != nullptr);
    if ((pos < 0) || (pos > func->stackIR->size())) {
        std::cerr << "Instrumenter: instrumentFunction() pos invalid!" << std::endl;
        delete added_instructions;
        return InstrumentResult::instrument_error;
    }

    // transform the vector of stack ir to a list for better modification
    std::list<wasm::StackInst*> stack_ir_list = _stack_ir_vec2list(*(func->stackIR));
    
    // perform operation on the pos
    auto iter = stack_ir_list.begin();
    std::advance(iter, pos);
    stack_ir_list.splice(iter, _stack_ir_vec2list((*added_instructions)[0].post_instructions));

    // write back the modified stack ir list to the func
    auto new_stack_ir_vec = _stack_ir_list2vec(stack_ir_list);
    func->stackIR = std::make_unique<wasm::StackIR>(new_stack_ir_vec);

    delete added_instructions;

    // validate the module after modification
    if (!BinaryenModuleValidate(this->module_)) {
        std::cerr << "Instrumenter: instrumentFunction() error when validate!" << std::endl;
        return InstrumentResult::validate_error;
    }
    
    return InstrumentResult::success;
}

}