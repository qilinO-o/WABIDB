#ifndef instrumenter_h
#define instrumenter_h
#include "instr-utils.hpp"

namespace wasm_instrument {

// config for the instrumentation task
// do several /operations/ on module from /filename/ and write to /targetname/
struct InstrumentConfig final {
    std::string filename;
    std::string targetname;
    wasm::FeatureSet feature = FEATURE_SPEC;
};

enum InstrumentResult {
    success = 0,
    config_error,
    open_module_error,
    instrument_error,
    validate_error,
    generation_error,
    invalid_state
};

enum InstrumentState {
    // before set config,
    idle = 0,
    // after set config, module is read and stack ir is emitted
    // all further instrumentations can be done at this state
    valid,
    // end instrumentation, module is written out
    written
};

// new Instrumenter with config and run with instrument()
// also provide other useful utilities including create wasm classes(globals, imports, expressions) etc.
class Instrumenter final {
public:
    Instrumenter() noexcept {
        this->module_ = new wasm::Module();
    }
    Instrumenter(const Instrumenter &a) = delete;
    Instrumenter(Instrumenter &&a) = delete;
    Instrumenter &operator=(const Instrumenter &) = delete;
    Instrumenter &operator=(Instrumenter &&) = delete;
    ~Instrumenter() noexcept {
        delete this->module_;
    }

    // set config, read module and make stack ir emitted
    // prepare for further instrumentations
    InstrumentResult setConfig(const InstrumentConfig &config) noexcept;
    // do the general instrumentations with match-and-insert semantics
    // and validate the modified module
    // make sure that the stack is balanced after insertion to pass the validation
    InstrumentResult instrument(const std::vector<InstrumentOperation> &operations) noexcept;
    // write the module to binary file with name config.targetname
    InstrumentResult writeBinary() noexcept;

    // instrumenter can be re-used after call clear()
    void clear() {
        this->state_ = InstrumentState::idle;
        delete this->module_;
        this->module_ = new wasm::Module();
        this->scopeClear();
    }

    // below: return nullptr denotes add or get failed
    wasm::Global* addGlobal(const char* name, 
                            BinaryenType type, 
                            bool if_mutable, 
                            BinaryenLiteral value) noexcept;
    // add functions at one time to avoid cannot find call target problems
    bool addFunctions(const std::vector<std::string> &names,
                    const std::vector<std::string> &func_bodies) noexcept;
    // Multiple memories proposal needed!
    wasm::Memory* addMemory(const char* name, bool if_shared, int init_pages, int max_pages) noexcept;
    wasm::DataSegment* addPassiveDateSegment(const char* name, const char* data, size_t len) noexcept;

    // below: if internal_name already exists, just turn the element to import by set external name
    bool addImportFunction(const char* internal_name,
                            const char* external_module_name,
                            const char* external_base_name,
                            BinaryenType params,
                            BinaryenType results) noexcept;
    bool addImportGlobal(const char* internal_name,
                        const char* external_module_name,
                        const char* external_base_name,
                        BinaryenType type,
                        bool if_mutable) noexcept;
    // Multiple memories proposal needed!
    bool addImportMemory(const char* internal_name,
                        const char* external_module_name,
                        const char* external_base_name,
                        bool if_shared) noexcept;
    wasm::Export* addExport(wasm::ModuleItemKind kind, const char* internal_name,
                            const char* external_name) noexcept;

    wasm::Global* getGlobal(const char* name) noexcept;
    wasm::Function* getFunction(const char* name) noexcept;
    wasm::Memory* getMemory(const char* name = nullptr) noexcept;
    wasm::DataSegment* getDateSegment(const char* name) noexcept;
    wasm::Export* getExport(const char* external_name) noexcept;
    // use base name for better WASI support
    wasm::Importable* getImport(wasm::ModuleItemKind kind, const char* base_name) noexcept;
    wasm::Function* getStartFunction() noexcept;

    // print module
    void print(bool if_stack_ir = false) {
        if (!if_stack_ir) {
            std::cout << *(this->module_);
        } else {
            _out_stackir_module(std::cout, this->module_);
        }
    }

    // scope apis
    bool scopeAdd(const std::string& name) {
        return this->function_scope_.emplace(name).second;
    }
    bool scopeRemove(const std::string& name) {
        auto i = this->function_scope_.find(name);
        if (i == this->function_scope_.end()) return false;
        this->function_scope_.erase(i);
        return true;
    }
    bool scopeContains(const std::string& name) {
        return static_cast<bool>(this->function_scope_.count(name));
    }
    void scopeClear() {
        this->function_scope_.clear();
    }
    const std::set<std::string>& getScope() const {
        return function_scope_;
    }

    // tool api
    wasm::Module*& getModule() {
        return this->module_;
    }

    // insert instructions in operation.post_instructions after the line of pos
    // instructions are indexed from 1
    // pos = 0 equals to insert at the beginning
    // only for careful use in that user should be aware of the valid pos
    // and make sure that the stack is balanced after insertion to pass the validation
    InstrumentResult instrumentFunction(const InstrumentOperation &operation,
                                        const char* name,
                                        size_t pos) noexcept;
    
private:
    InstrumentConfig config_;
    wasm::Module* module_;
    InstrumentState state_ = InstrumentState::idle;
    // record function names that should be instrumented
    // default contain all unimport functions from the original binary
    std::set<std::string> function_scope_;

    InstrumentResult _read_file() noexcept;
    InstrumentResult _write_file() noexcept;
};

std::string InstrumentResult2str(InstrumentResult result);

} // namespace wasm_instrument

#endif