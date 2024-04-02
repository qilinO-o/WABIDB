#ifndef config_builder_h
#define config_builder_h

#include "instrumenter.hpp"

namespace wasm_instrument {

// 1 to 1 related to config.operations
// data structure for transformed instruction string to stack ir
struct AddedInstructions {
    struct AddedInstruction {
        std::vector<wasm::StackInst*> pre_instructions;
        std::vector<wasm::StackInst*> post_instructions;
    };
    std::vector<AddedInstruction> vec;
};

// construct and use by the class Instrumenter
class ConfigBuilder final {
public:
    ConfigBuilder() noexcept {
        temp_module_ = BinaryenModuleCreate();
    };
    ConfigBuilder(const ConfigBuilder &a) = delete;
    ConfigBuilder(ConfigBuilder &&a) = delete;
    ConfigBuilder &operator=(const ConfigBuilder &) = delete;
    ConfigBuilder &operator=(ConfigBuilder &&) = delete;
    ~ConfigBuilder() noexcept {
        BinaryenModuleDispose(temp_module_);
    };

    AddedInstructions* makeConfig(const InstrumentConfig &config) noexcept;
private:
    wasm::Module* temp_module_;
};

}

#endif