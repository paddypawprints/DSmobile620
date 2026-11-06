#include "DS620Driver.h"

#include "DS620UserClient.h"

#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryDescriptor.h>
#include <USBDriverKit/AppleUSBDefinitions.h>
#include <USBDriverKit/IOUSBHostInterface.h>
#include <USBDriverKit/IOUSBHostPipe.h>

struct DS620Driver_IVars {
    IOUSBHostInterface* interface;
    IOUSBHostPipe* pipeOut;
    IOUSBHostPipe* pipeIn;
};

namespace {

constexpr uint32_t kMaxTransfer = 16u * 1024u * 1024u;

const IOUSBEndpointDescriptor* asEndpoint(const uint8_t* cursor, uint8_t length) {
    if (length < sizeof(IOUSBEndpointDescriptor)) {
        return nullptr;
    }
    return reinterpret_cast<const IOUSBEndpointDescriptor*>(cursor);
}

kern_return_t findBulkPipes(IOUSBHostInterface* interface, uint8_t* outAddress, uint8_t* inAddress) {
    *outAddress = 0;
    *inAddress = 0;
    const IOUSBConfigurationDescriptor* config = interface->CopyConfigurationDescriptor();
    if (config == nullptr) {
        return kIOReturnNotFound;
    }
    const uint16_t total = config->wTotalLength;
    if (total < sizeof(IOUSBConfigurationDescriptor)) {
        IOFree(const_cast<IOUSBConfigurationDescriptor*>(config), total);
        return kIOReturnBadArgument;
    }

    const IOUSBInterfaceDescriptor* intf = interface->GetInterfaceDescriptor(config);
    const uint8_t* start = reinterpret_cast<const uint8_t*>(config);
    const uint8_t* end = start + total;
    const uint8_t* cursor = reinterpret_cast<const uint8_t*>(intf);
    kern_return_t result = kIOReturnSuccess;
    if (intf == nullptr || cursor < start || cursor + intf->bLength > end) {
        result = kIOReturnBadArgument;
    } else {
        cursor += intf->bLength;
        while (cursor + 2 <= end) {
            const uint8_t length = cursor[0];
            const uint8_t type = cursor[1];
            if (length < 2 || cursor + length > end) {
                result = kIOReturnBadArgument;
                break;
            }
            if (type == kIOUSBDescriptorTypeInterface) {
                break;
            }
            if (type == kIOUSBDescriptorTypeEndpoint) {
                const IOUSBEndpointDescriptor* endpoint = asEndpoint(cursor, length);
                if (endpoint != nullptr &&
                    (endpoint->bmAttributes & kIOUSBEndpointDescriptorTransferType) ==
                        kIOUSBEndpointDescriptorTransferTypeBulk) {
                    if ((endpoint->bEndpointAddress & kIOUSBEndpointDescriptorDirection) != 0) {
                        *inAddress = endpoint->bEndpointAddress;
                    } else {
                        *outAddress = endpoint->bEndpointAddress;
                    }
                }
            }
            cursor += length;
        }
    }
    IOFree(const_cast<IOUSBConfigurationDescriptor*>(config), total);
    if (result != kIOReturnSuccess) {
        return result;
    }
    if (*outAddress == 0 || *inAddress == 0) {
        return kIOReturnNotFound;
    }
    return kIOReturnSuccess;
}

}  // namespace

bool DS620Driver::init() {
    if (!super::init()) {
        return false;
    }
    ivars = IONewZero(DS620Driver_IVars, 1);
    return ivars != nullptr;
}

void DS620Driver::free() {
    IOSafeDeleteNULL(ivars, DS620Driver_IVars, 1);
    super::free();
}

kern_return_t IMPL(DS620Driver, Start) {
    kern_return_t result = Start(provider, SUPERDISPATCH);
    if (result != kIOReturnSuccess) {
        return result;
    }

    IOUSBHostInterface* interface = OSDynamicCast(IOUSBHostInterface, provider);
    if (interface == nullptr) {
        Stop(provider);
        return kIOReturnUnsupported;
    }

    result = interface->Open(this, 0, nullptr);
    if (result != kIOReturnSuccess) {
        Stop(provider);
        return result;
    }
    ivars->interface = interface;
    interface->retain();

    uint8_t outAddress = 0;
    uint8_t inAddress = 0;
    result = findBulkPipes(interface, &outAddress, &inAddress);
    if (result != kIOReturnSuccess) {
        Stop(provider);
        return result;
    }

    result = interface->CopyPipe(outAddress, &ivars->pipeOut);
    if (result != kIOReturnSuccess) {
        ivars->pipeOut = nullptr;
        Stop(provider);
        return result;
    }
    result = interface->CopyPipe(inAddress, &ivars->pipeIn);
    if (result != kIOReturnSuccess) {
        ivars->pipeIn = nullptr;
        Stop(provider);
        return result;
    }

    IOLog("DS620Driver claimed bulk out 0x%02x in 0x%02x\n", outAddress, inAddress);
    RegisterService();
    return kIOReturnSuccess;
}

kern_return_t IMPL(DS620Driver, Stop) {
    if (ivars == nullptr) {
        return Stop(provider, SUPERDISPATCH);
    }
    if (ivars->pipeOut != nullptr) {
        ivars->pipeOut->release();
        ivars->pipeOut = nullptr;
    }
    if (ivars->pipeIn != nullptr) {
        ivars->pipeIn->release();
        ivars->pipeIn = nullptr;
    }
    if (ivars->interface != nullptr) {
        ivars->interface->Close(this, 0);
        ivars->interface->release();
        ivars->interface = nullptr;
    }
    return Stop(provider, SUPERDISPATCH);
}

kern_return_t IMPL(DS620Driver, NewUserClient) {
    IOService* created = nullptr;
    kern_return_t result = Create(this, "UserClientProperties", &created);
    if (result != kIOReturnSuccess) {
        return result;
    }
    *userClient = OSDynamicCast(IOUserClient, created);
    if (*userClient == nullptr) {
        created->release();
        return kIOReturnUnsupported;
    }
    return kIOReturnSuccess;
}

kern_return_t DS620Driver::BulkOut(IOMemoryDescriptor* buffer, uint32_t length, uint32_t timeoutMs,
                                   uint32_t* transferred) {
    if (ivars->pipeOut == nullptr || buffer == nullptr || transferred == nullptr || length == 0 ||
        length > kMaxTransfer) {
        return kIOReturnBadArgument;
    }
    *transferred = 0;
    return ivars->pipeOut->IO(buffer, length, transferred, timeoutMs);
}

kern_return_t DS620Driver::BulkIn(IOMemoryDescriptor* buffer, uint32_t length, uint32_t timeoutMs,
                                  uint32_t* transferred) {
    if (ivars->pipeIn == nullptr || buffer == nullptr || transferred == nullptr || length == 0 ||
        length > kMaxTransfer) {
        return kIOReturnBadArgument;
    }
    *transferred = 0;
    return ivars->pipeIn->IO(buffer, length, transferred, timeoutMs);
}

kern_return_t DS620Driver::AbortPipes() {
    kern_return_t outResult = kIOReturnSuccess;
    kern_return_t inResult = kIOReturnSuccess;
    if (ivars->pipeOut != nullptr) {
        outResult = ivars->pipeOut->Abort(0, kIOReturnAborted, nullptr);
    }
    if (ivars->pipeIn != nullptr) {
        inResult = ivars->pipeIn->Abort(0, kIOReturnAborted, nullptr);
    }
    return outResult != kIOReturnSuccess ? outResult : inResult;
}
