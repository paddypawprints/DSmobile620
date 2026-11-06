#pragma once

#include "ds620/scanner.hpp"
#include "ds620/transport.hpp"

#include <cstdint>
#include <vector>

namespace ds620 {

// In-process DS-620 that answers SCSI INQUIRY the way the real unit did.
// Used by the protocol tests and by `ds620ctl inquiry --mock`.
class SimulatedTransport : public Transport {
public:
    Error bulkOut(const uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) override;
    Error bulkIn(uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) override;
    Error abort() override;

    void setInquiryStatus(uint8_t status);

private:
    enum class Phase {
        Command,
        DataOut,
        DataIn,
        Status,
    };

    Error acceptCommand(const uint8_t* data, uint32_t length);
    void queueStatus();

    Phase phase_ = Phase::Command;
    uint32_t tag_ = 0;
    uint32_t dataLength_ = 0;
    bool dataIn_ = false;
    uint8_t cdb_[16] = {};
    uint8_t cdbLength_ = 0;
    uint8_t inquiryStatus_ = 0;
    std::vector<uint8_t> incoming_;
};

}  // namespace ds620
