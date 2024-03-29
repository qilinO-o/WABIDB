#include "binaryen-c.h"
#include "wasm.h"
#include <vector>

namespace wasm_instrument {

struct InstrumentOperation final {
    enum Loaction {
        before,
        after
    };
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
    std::vector<ExpName> targets;
    Loaction location;
    std::vector<wasm::Expression*> added_instructions;
};

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
};

} // namespace wasm_instrument