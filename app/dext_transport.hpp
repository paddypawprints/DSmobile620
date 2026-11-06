#pragma once

#include "ds620/transport.hpp"

#include <cstdint>

namespace ds620 {

// Bulk pipes of the DS620Driver user client. Fails with Transport when the
// extension is not installed or the scanner is not plugged in.
class DextTransport : public Transport {
public:
    DextTransport();
    ~DextTransport() override;

    DextTransport(const DextTransport&) = delete;
    DextTransport& operator=(const DextTransport&) = delete;

    enum class OpenFailure { None, Absent, NotPermitted };

    bool open();
    OpenFailure openFailure() const { return openFailure_; }
    void close();

    Error bulkOut(const uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) override;
    Error bulkIn(uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) override;
    Error abort() override;

private:
    uint32_t connection_;
    bool open_;
    OpenFailure openFailure_;
};

}  // namespace ds620
