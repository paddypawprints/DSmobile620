#pragma once

#include <cstddef>
#include <cstdint>

namespace ds620 {

enum class Error : int {
    Ok = 0,
    Transport = -1,
    Protocol = -2,
    Status = -3,
    Short = -4,
    Argument = -5,
};

// Bulk-only transport underneath the scanner commands. The DriverKit
// extension supplies one of these. Tests supply a simulated device.
class Transport {
public:
    virtual ~Transport() = default;

    virtual Error bulkOut(const uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) = 0;
    virtual Error bulkIn(uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) = 0;
    virtual Error abort() = 0;
};

inline constexpr uint32_t kDefaultTimeoutMs = 5000;

}  // namespace ds620
