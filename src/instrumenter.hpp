#include "binaryen-c.h"
#include "wasm.h"
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
    std::vector<wasm::StackInst*> pre_instructions;
    std::vector<wasm::StackInst*> post_instructions;
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
    file_path_error,
    open_module_error,
    instrument_error,
    validate_error,
    generation_error
};

std::string InstrumentResult2str(InstrumentResult result);

// new Instrumenter with config and run with instrument()
class Instrumenter final {
public:
    Instrumenter() noexcept = default;
    explicit Instrumenter(InstrumentConfig &config) noexcept {
        this->config_.filename = config.filename;
        this->config_.targetname = config.targetname;
        this->config_.operations.assign(config.operations.begin(), config.operations.end());
    }
    Instrumenter(const Instrumenter &a) = delete;
    Instrumenter(Instrumenter &&a) = delete;
    Instrumenter &operator=(const Instrumenter &) = delete;
    Instrumenter &operator=(Instrumenter &&) = delete;
    ~Instrumenter() noexcept = default;

    InstrumentResult instrument() noexcept;
    
private:
    InstrumentConfig config_;
    wasm::Module module_;

    InstrumentResult _read_file() noexcept;
    InstrumentResult _write_file() noexcept;
};

} // namespace wasm_instrument