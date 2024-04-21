#ifndef instrumenter_h
#define instrumenter_h

#include "binaryen-c.h"
#include <wasm.h>
#include <wasm-stack.h>
#include <vector>

namespace wasm_instrument {

const wasm::FeatureSet FEATURE_SPEC = wasm::FeatureSet::BulkMemory |
                                    wasm::FeatureSet::Multivalue |
                                    wasm::FeatureSet::SignExt |
                                    wasm::FeatureSet::MutableGlobals |
                                    wasm::FeatureSet::SIMD |
                                    wasm::FeatureSet::ReferenceTypes |
                                    wasm::FeatureSet::TruncSat;

// struct to define a certain operation
// add /add_instructions/ at /location/ of the current expression that matches /targets/
struct InstrumentOperation final {
    struct ExpName {
        wasm::Expression::Id id;
        union ExpOp {
            wasm::UnaryOp uop;
            wasm::BinaryOp bop;
            // control flow op mark its begin or end
            wasm::StackInst::Op cop;
            // to be added more op id
        };
        // nullopt to ignore Op check
        std::optional<ExpOp> exp_op;
        // nullopt to ignore type check
        std::optional<BinaryenType> exp_type;
    };
    // targets of all operations should be *Orthogonal* !
    std::vector<ExpName> targets;
    std::vector<std::string> pre_instructions;
    std::vector<std::string> post_instructions;
};

// 1 to 1 related to config.operations
// data structure for transformed instruction string to stack ir
struct AddedInstructions {
    struct AddedInstruction {
        std::vector<wasm::StackInst*> pre_instructions;
        std::vector<wasm::StackInst*> post_instructions;
    };
    std::vector<AddedInstruction> vec;
};

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
        this->module_ = BinaryenModuleCreate();
    }
    Instrumenter(const Instrumenter &a) = delete;
    Instrumenter(Instrumenter &&a) = delete;
    Instrumenter &operator=(const Instrumenter &) = delete;
    Instrumenter &operator=(Instrumenter &&) = delete;
    ~Instrumenter() noexcept {
        BinaryenModuleDispose(this->module_);
    }

    // set config, read module and make stack ir emitted
    // prepare for further instrumentations
    InstrumentResult setConfig(const InstrumentConfig &config) noexcept;
    // do the general instrumentations with match-and-insert semantics
    // and validate the modified module
    InstrumentResult instrument(const std::vector<InstrumentOperation> &operations) noexcept;
    // write the module to binary file with name config.targetname
    InstrumentResult writeBinary() noexcept;

    // instrumenter can be re-used after call clear()
    void clear() {
        this->state_ = InstrumentState::idle;
        BinaryenModuleDispose(this->module_);
        delete this->added_instructions_;
        this->module_ = BinaryenModuleCreate();
    }

    // below: return nullptr denotes add or get failed
    wasm::Global* addGlobal(const char* name, 
                            BinaryenType type, 
                            bool if_mutable, 
                            BinaryenLiteral value) noexcept;
    // add functions at one time
    // cannot be called twice!
    void addFunctions(const std::vector<std::string> &names,
                    const std::vector<std::string> &func_bodies) noexcept;
    // Multiple memories proposal needed!
    wasm::Memory* addMemory(const char* name, bool if_shared, int init_pages, int max_pages) noexcept;
    
    // below: if internal_name already exists, just turn the element to import by set external name
    void addImportFunction(const char* internal_name,
                            const char* external_module_name,
                            const char* external_base_name,
                            BinaryenType params,
                            BinaryenType results) noexcept;
    void addImportGlobal(const char* internal_name,
                        const char* external_module_name,
                        const char* external_base_name,
                        BinaryenType type,
                        bool if_mutable) noexcept;
    // Multiple memories proposal needed!
    void addImportMemory(const char* internal_name,
                        const char* external_module_name,
                        const char* external_base_name,
                        bool if_shared) noexcept;
    wasm::Export* addExport(wasm::ModuleItemKind kind, const char* internal_name,
                            const char* external_name) noexcept;

    wasm::Global* getGlobal(const char* name) noexcept;
    wasm::Function* getFunction(const char* name) noexcept;
    wasm::Memory* getMemory(const char* name) noexcept;
    wasm::Export* getExport(const char* external_name) noexcept;
    // use base name for better WASI support
    wasm::Importable* getImport(wasm::ModuleItemKind kind, const char* base_name) noexcept;
    wasm::Function* getStartFunction() noexcept;

    // print module
    void print(bool if_stack_ir = false) {
        if (!if_stack_ir) {
            BinaryenModulePrint(this->module_);
        } else {
            BinaryenModulePrintStackIR(this->module_, false);
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

    // tool api
    wasm::Module* getModule() {
        return this->module_;
    }
    
private:
    InstrumentConfig config_;
    wasm::Module* module_;
    InstrumentState state_ = InstrumentState::idle;
    AddedInstructions* added_instructions_;
    // record function names that should be instrumented
    // default contain all unimport functions from the original binary
    std::set<std::string> function_scope_;

    InstrumentResult _read_file() noexcept;
    InstrumentResult _write_file() noexcept;
};

std::string InstrumentResult2str(InstrumentResult result);
bool _readTextData(const std::string& input, wasm::Module& wasm);

} // namespace wasm_instrument

#endif