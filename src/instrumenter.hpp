#ifndef instrumenter_h
#define instrumenter_h

#include "binaryen-c.h"
#include <wasm.h>
#include <wasm-stack.h>
#include <vector>

namespace wasm_instrument {

// struct to define a certain operation
// add /add_instructions/ at /location/ of the current expression that matches /targets/
struct InstrumentOperation final {
    struct ExpName {
        wasm::Expression::Id id;
        union ExpOp {
            int no_op; // only set as -1 for no op expressions
            wasm::UnaryOp uop;
            wasm::BinaryOp bop;
            // to be added more op id
        };
        ExpOp exp_op;
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
    std::vector<InstrumentOperation> operations;
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

std::string InstrumentResult2str(InstrumentResult result);

// new Instrumenter with config and run with instrument()
// also provide other useful utilities including create wasm classes(globals, imports, expressions) etc.
class Instrumenter final {
public:
    Instrumenter() noexcept {
        this->module_ = BinaryenModuleCreate();
        this->mallocator_ = BinaryenModuleCreate();
    }
    Instrumenter(const Instrumenter &a) = delete;
    Instrumenter(Instrumenter &&a) = delete;
    Instrumenter &operator=(const Instrumenter &) = delete;
    Instrumenter &operator=(Instrumenter &&) = delete;
    ~Instrumenter() noexcept {
        BinaryenModuleDispose(this->mallocator_);
    }

    // set config, read module and make stack ir emitted
    // prepare for further instrumentations
    InstrumentResult setConfig(InstrumentConfig &config) noexcept;
    // do the general instrumentations with match-and-insert semantics
    // and validate the modified module
    InstrumentResult instrument() noexcept;
    // write the module to binary file with name config.targetname
    InstrumentResult writeBinary() noexcept;

    void clear() {
        this->state_ = InstrumentState::idle;
        BinaryenModuleDispose(this->module_);
        BinaryenModuleDispose(this->mallocator_);
        delete this->added_instructions_;
        this->module_ = BinaryenModuleCreate();
        this->mallocator_ = BinaryenModuleCreate();
    }
    
private:
    InstrumentConfig config_;
    wasm::Module* module_;
    InstrumentState state_ = InstrumentState::idle;

    // allocator for newly created wasm classes(e.g.expressions, new globals, imports etc.)
    // so it must be created at first and deleted at last
    wasm::Module* mallocator_;
    AddedInstructions* added_instructions_; 

    InstrumentResult _read_file() noexcept;
    InstrumentResult _write_file() noexcept;
};

} // namespace wasm_instrument

#endif