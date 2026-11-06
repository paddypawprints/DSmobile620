#include "dext_transport.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>

namespace ds620 {
namespace {

constexpr uint32_t kBulkOut = 0;
constexpr uint32_t kBulkIn = 1;
constexpr uint32_t kAbort = 2;

Error map(kern_return_t result) {
    return result == KERN_SUCCESS ? Error::Ok : Error::Transport;
}

}  // namespace

DextTransport::DextTransport() : connection_(0), open_(false), openFailure_(OpenFailure::None) {}

DextTransport::~DextTransport() { close(); }

bool DextTransport::open() {
    close();
    openFailure_ = OpenFailure::Absent;
    CFMutableDictionaryRef matching = IOServiceMatching("IOUserService");
    if (matching == nullptr) {
        return false;
    }
    CFDictionarySetValue(matching, CFSTR("IOUserClass"), CFSTR("DS620Driver"));
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, matching);
    if (service == IO_OBJECT_NULL) {
        return false;
    }
    io_connect_t connection = 0;
    kern_return_t result = IOServiceOpen(service, mach_task_self(), 0, &connection);
    IOObjectRelease(service);
    if (result != KERN_SUCCESS) {
        openFailure_ = result == kIOReturnNotPermitted ? OpenFailure::NotPermitted : OpenFailure::Absent;
        return false;
    }
    connection_ = connection;
    open_ = true;
    openFailure_ = OpenFailure::None;
    return true;
}

void DextTransport::close() {
    if (!open_) {
        return;
    }
    IOServiceClose(connection_);
    connection_ = 0;
    open_ = false;
}

Error DextTransport::bulkOut(const uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) {
    if (!open_ || (data == nullptr && length > 0)) {
        return Error::Argument;
    }
    uint64_t input = timeoutMs;
    uint64_t output = 0;
    uint32_t outputCount = 1;
    kern_return_t result = IOConnectCallMethod(connection_, kBulkOut, &input, 1, data, length, &output, &outputCount,
                                                nullptr, nullptr);
    if (result != KERN_SUCCESS || outputCount != 1 || output > length) {
        return map(result == KERN_SUCCESS ? KERN_FAILURE : result);
    }
    transferred = static_cast<uint32_t>(output);
    return Error::Ok;
}

Error DextTransport::bulkIn(uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) {
    if (!open_ || data == nullptr || length == 0) {
        return Error::Argument;
    }
    uint64_t input[2] = {length, timeoutMs};
    uint64_t output = 0;
    uint32_t outputCount = 1;
    size_t outputSize = length;
    kern_return_t result = IOConnectCallMethod(connection_, kBulkIn, input, 2, nullptr, 0, &output, &outputCount, data,
                                                &outputSize);
    if (result != KERN_SUCCESS || outputCount != 1) {
        return map(result == KERN_SUCCESS ? KERN_FAILURE : result);
    }
    transferred = static_cast<uint32_t>(outputSize);
    return Error::Ok;
}

Error DextTransport::abort() {
    if (!open_) {
        return Error::Argument;
    }
    return map(IOConnectCallMethod(connection_, kAbort, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr));
}

}  // namespace ds620
