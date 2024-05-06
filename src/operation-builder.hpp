#ifndef operation_builder_h
#define operation_builder_h

#include "instr-utils.hpp"

namespace wasm_instrument {

// construct and use by the class Instrumenter
// serve to parse operations in a config
class OperationBuilder final {
public:
    OperationBuilder() noexcept = default;
    OperationBuilder(const OperationBuilder &a) = delete;
    OperationBuilder(OperationBuilder &&a) = delete;
    OperationBuilder &operator=(const OperationBuilder &) = delete;
    OperationBuilder &operator=(OperationBuilder &&) = delete;
    ~OperationBuilder() noexcept = default;

    AddedInstructions* makeOperations(wasm::Module* &mallocator, const std::vector<InstrumentOperation> &operations) noexcept;
private:

};

}

#endif