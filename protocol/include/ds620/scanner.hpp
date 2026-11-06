#pragma once

#include "ds620/transport.hpp"

namespace ds620 {

inline constexpr uint8_t kInquiryOpcode = 0x12;
inline constexpr uint32_t kInquiryLength = 36;

struct Identity {
    char vendor[9];
    char product[17];
    char revision[5];
    uint8_t peripheralType;
};

// Speaks SCSI commands over bulk-only transport. No IOKit types.
class Scanner {
public:
    explicit Scanner(Transport& transport);

    Error inquiry(Identity& identity, uint32_t timeoutMs = kDefaultTimeoutMs);

private:
    Error command(bool dataIn, const uint8_t* cdb, uint8_t cdbLength, uint8_t* data, uint32_t dataLength,
                  uint32_t timeoutMs, uint32_t& transferred);

    Transport& transport_;
    uint32_t nextTag_;
};

}  // namespace ds620
