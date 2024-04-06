#ifndef config_builder_h
#define config_builder_h

#include "instrumenter.hpp"

namespace wasm_instrument {

// construct and use by the class Instrumenter
// serve to parse operations in a config
class ConfigBuilder final {
public:
    ConfigBuilder() noexcept = default;
    ConfigBuilder(const ConfigBuilder &a) = delete;
    ConfigBuilder(ConfigBuilder &&a) = delete;
    ConfigBuilder &operator=(const ConfigBuilder &) = delete;
    ConfigBuilder &operator=(ConfigBuilder &&) = delete;
    ~ConfigBuilder() noexcept = default;

    AddedInstructions* makeConfig(wasm::Module &mallocator, const InstrumentConfig &config) noexcept;
private:

};

}

#endif