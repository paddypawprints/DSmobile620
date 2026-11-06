#include "DS620UserClient.h"

#include <string.h>

#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryDescriptor.h>
#include <DriverKit/IOMemoryMap.h>
#include <DriverKit/OSData.h>

struct DS620UserClient_IVars {
    DS620Driver* driver;
};

namespace {

constexpr uint32_t kBulkOut = 0;
constexpr uint32_t kBulkIn = 1;
constexpr uint32_t kAbort = 2;
constexpr uint32_t kMethodCount = 3;
constexpr uint32_t kMaxTransfer = 16u * 1024u * 1024u;

void releaseMap(IOMemoryMap* map) {
    if (map != nullptr) {
        map->release();
    }
}

kern_return_t copyIn(IOUserClientMethodArguments* arguments, IOBufferMemoryDescriptor** buffer, uint32_t* length) {
    const void* bytes = nullptr;
    uint32_t count = 0;
    IOMemoryMap* map = nullptr;

    if (arguments->structureInput != nullptr) {
        const size_t available = arguments->structureInput->getLength();
        if (available == 0 || available > kMaxTransfer) {
            return kIOReturnBadArgument;
        }
        bytes = arguments->structureInput->getBytesNoCopy();
        count = static_cast<uint32_t>(available);
    } else if (arguments->structureInputDescriptor != nullptr) {
        uint64_t available = 0;
        kern_return_t result = arguments->structureInputDescriptor->GetLength(&available);
        if (result != kIOReturnSuccess) {
            return result;
        }
        if (available == 0 || available > kMaxTransfer) {
            return kIOReturnBadArgument;
        }
        result = arguments->structureInputDescriptor->CreateMapping(kIOMemoryMapReadOnly, 0, 0, 0, 0, &map);
        if (result != kIOReturnSuccess) {
            return result;
        }
        bytes = reinterpret_cast<const void*>(map->GetAddress());
        count = static_cast<uint32_t>(map->GetLength());
    } else {
        return kIOReturnBadArgument;
    }

    if (bytes == nullptr || count == 0 || count > kMaxTransfer) {
        releaseMap(map);
        return kIOReturnBadArgument;
    }

    IOBufferMemoryDescriptor* created = nullptr;
    kern_return_t result = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut, count, 0, &created);
    if (result != kIOReturnSuccess) {
        releaseMap(map);
        return result;
    }
    result = created->SetLength(count);
    if (result == kIOReturnSuccess) {
        IOAddressSegment segment{};
        result = created->GetAddressRange(&segment);
        if (result == kIOReturnSuccess) {
            memcpy(reinterpret_cast<void*>(segment.address), bytes, count);
        }
    }
    releaseMap(map);
    if (result != kIOReturnSuccess) {
        created->release();
        return result;
    }
    *buffer = created;
    *length = count;
    return kIOReturnSuccess;
}

kern_return_t makeInBuffer(uint32_t capacity, IOBufferMemoryDescriptor** buffer) {
    IOBufferMemoryDescriptor* created = nullptr;
    kern_return_t result = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionIn, capacity, 0, &created);
    if (result != kIOReturnSuccess) {
        return result;
    }
    result = created->SetLength(capacity);
    if (result != kIOReturnSuccess) {
        created->release();
        return result;
    }
    *buffer = created;
    return kIOReturnSuccess;
}

kern_return_t publishBytes(IOUserClientMethodArguments* arguments, const void* bytes, uint32_t length) {
    if (arguments->structureOutputDescriptor != nullptr) {
        IOMemoryMap* map = nullptr;
        kern_return_t result = arguments->structureOutputDescriptor->CreateMapping(0, 0, 0, 0, 0, &map);
        if (result != kIOReturnSuccess) {
            return result;
        }
        if (map->GetLength() < length) {
            map->release();
            return kIOReturnNoSpace;
        }
        memcpy(reinterpret_cast<void*>(map->GetAddress()), bytes, length);
        map->release();
        return kIOReturnSuccess;
    }

    OSData* data = OSData::withBytes(bytes, length);
    if (data == nullptr) {
        return kIOReturnNoMemory;
    }
    arguments->structureOutput = data;
    return kIOReturnSuccess;
}

kern_return_t handleBulkOut(DS620Driver* driver, IOUserClientMethodArguments* arguments) {
    if (driver == nullptr || arguments->scalarInputCount < 1 || arguments->scalarOutput == nullptr) {
        return kIOReturnBadArgument;
    }
    IOBufferMemoryDescriptor* buffer = nullptr;
    uint32_t length = 0;
    kern_return_t result = copyIn(arguments, &buffer, &length);
    if (result != kIOReturnSuccess) {
        return result;
    }
    uint32_t transferred = 0;
    const uint32_t timeoutMs = static_cast<uint32_t>(arguments->scalarInput[0]);
    result = driver->BulkOut(buffer, length, timeoutMs, &transferred);
    buffer->release();
    if (result != kIOReturnSuccess) {
        return result;
    }
    arguments->scalarOutput[0] = transferred;
    arguments->scalarOutputCount = 1;
    return kIOReturnSuccess;
}

kern_return_t handleBulkIn(DS620Driver* driver, IOUserClientMethodArguments* arguments) {
    if (driver == nullptr || arguments->scalarInputCount < 2 || arguments->scalarOutput == nullptr) {
        return kIOReturnBadArgument;
    }
    const uint32_t capacity = static_cast<uint32_t>(arguments->scalarInput[0]);
    const uint32_t timeoutMs = static_cast<uint32_t>(arguments->scalarInput[1]);
    if (capacity == 0 || capacity > kMaxTransfer || capacity > arguments->structureOutputMaximumSize) {
        return kIOReturnBadArgument;
    }
    IOBufferMemoryDescriptor* buffer = nullptr;
    kern_return_t result = makeInBuffer(capacity, &buffer);
    if (result != kIOReturnSuccess) {
        return result;
    }
    uint32_t transferred = 0;
    result = driver->BulkIn(buffer, capacity, timeoutMs, &transferred);
    if (result == kIOReturnSuccess) {
        IOAddressSegment segment{};
        result = buffer->GetAddressRange(&segment);
        if (result == kIOReturnSuccess) {
            result = publishBytes(arguments, reinterpret_cast<const void*>(segment.address), transferred);
        }
    }
    buffer->release();
    if (result != kIOReturnSuccess) {
        return result;
    }
    arguments->scalarOutput[0] = transferred;
    arguments->scalarOutputCount = 1;
    return kIOReturnSuccess;
}

DS620Driver* driverFrom(OSObject* target) { return OSDynamicCast(DS620Driver, target); }

kern_return_t dispatchBulkOut(OSObject* target, void* reference, IOUserClientMethodArguments* arguments) {
    (void)reference;
    return handleBulkOut(driverFrom(target), arguments);
}

kern_return_t dispatchBulkIn(OSObject* target, void* reference, IOUserClientMethodArguments* arguments) {
    (void)reference;
    return handleBulkIn(driverFrom(target), arguments);
}

kern_return_t dispatchAbort(OSObject* target, void* reference, IOUserClientMethodArguments* arguments) {
    (void)reference;
    (void)arguments;
    DS620Driver* driver = driverFrom(target);
    if (driver == nullptr) {
        return kIOReturnNotReady;
    }
    return driver->AbortPipes();
}

const IOUserClientMethodDispatch kMethods[kMethodCount] = {
    {dispatchBulkOut, 0, 1, kIOUserClientVariableStructureSize, 1, 0},
    {dispatchBulkIn, 0, 2, 0, 1, kIOUserClientVariableStructureSize},
    {dispatchAbort, 0, 0, 0, 0, 0},
};

}  // namespace

bool DS620UserClient::init() {
    if (!super::init()) {
        return false;
    }
    ivars = IONewZero(DS620UserClient_IVars, 1);
    return ivars != nullptr;
}

void DS620UserClient::free() {
    IOSafeDeleteNULL(ivars, DS620UserClient_IVars, 1);
    super::free();
}

kern_return_t IMPL(DS620UserClient, Start) {
    kern_return_t result = Start(provider, SUPERDISPATCH);
    if (result != kIOReturnSuccess) {
        return result;
    }
    ivars->driver = OSDynamicCast(DS620Driver, provider);
    if (ivars->driver == nullptr) {
        Stop(provider);
        return kIOReturnUnsupported;
    }
    ivars->driver->retain();
    return kIOReturnSuccess;
}

kern_return_t IMPL(DS620UserClient, Stop) {
    if (ivars != nullptr && ivars->driver != nullptr) {
        ivars->driver->release();
        ivars->driver = nullptr;
    }
    return Stop(provider, SUPERDISPATCH);
}

kern_return_t DS620UserClient::ExternalMethod(uint64_t selector, IOUserClientMethodArguments* arguments,
                                              const IOUserClientMethodDispatch* dispatch, OSObject* target,
                                              void* reference) {
    (void)dispatch;
    (void)target;
    (void)reference;
    if (ivars == nullptr || ivars->driver == nullptr) {
        return kIOReturnNotReady;
    }
    return IOUserClient::ExternalMethod(selector, arguments, kMethods, ivars->driver, nullptr);
}
