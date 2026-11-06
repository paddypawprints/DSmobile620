#include "ds620/scanner.hpp"

#include <cstring>

namespace ds620 {
namespace {

constexpr uint32_t kCbwSignature = 0x43425355;  // 'USBC'
constexpr uint32_t kCswSignature = 0x53425355;  // 'USBS'
constexpr uint8_t kCbwFlagIn = 0x80;
constexpr uint8_t kCswStatusOk = 0x00;
constexpr size_t kCbwSize = 31;
constexpr size_t kCswSize = 13;
constexpr size_t kCdbSize = 16;

struct CommandBlock {
    uint32_t signature;
    uint32_t tag;
    uint32_t dataTransferLength;
    uint8_t flags;
    uint8_t lun;
    uint8_t cbLength;
    uint8_t cb[kCdbSize];
} __attribute__((packed));

struct CommandStatus {
    uint32_t signature;
    uint32_t tag;
    uint32_t dataResidue;
    uint8_t status;
} __attribute__((packed));

static_assert(sizeof(CommandBlock) == kCbwSize, "CBW is 31 bytes");
static_assert(sizeof(CommandStatus) == kCswSize, "CSW is 13 bytes");

void copyField(char* dest, size_t destSize, const uint8_t* src, size_t srcSize) {
    size_t end = srcSize;
    while (end > 0 && (src[end - 1] == ' ' || src[end - 1] == '\0')) {
        end -= 1;
    }
    if (end >= destSize) {
        end = destSize - 1;
    }
    std::memcpy(dest, src, end);
    dest[end] = '\0';
}

}  // namespace

Scanner::Scanner(Transport& transport) : transport_(transport), nextTag_(1) {}

Error Scanner::command(bool dataIn, const uint8_t* cdb, uint8_t cdbLength, uint8_t* data, uint32_t dataLength,
                       uint32_t timeoutMs, uint32_t& transferred) {
    if (cdb == nullptr || cdbLength == 0 || cdbLength > kCdbSize) {
        return Error::Argument;
    }
    if (dataLength > 0 && data == nullptr) {
        return Error::Argument;
    }

    CommandBlock block{};
    block.signature = kCbwSignature;
    block.tag = nextTag_++;
    block.dataTransferLength = dataLength;
    block.flags = dataIn ? kCbwFlagIn : 0;
    block.lun = 0;
    block.cbLength = cdbLength;
    std::memcpy(block.cb, cdb, cdbLength);

    uint32_t sent = 0;
    Error error = transport_.bulkOut(reinterpret_cast<const uint8_t*>(&block), kCbwSize, timeoutMs, sent);
    if (error != Error::Ok) {
        return error;
    }
    if (sent != kCbwSize) {
        return Error::Short;
    }

    transferred = 0;
    if (dataLength > 0) {
        if (dataIn) {
            error = transport_.bulkIn(data, dataLength, timeoutMs, transferred);
        } else {
            error = transport_.bulkOut(data, dataLength, timeoutMs, transferred);
        }
        if (error != Error::Ok) {
            return error;
        }
    }

    CommandStatus status{};
    uint32_t statusBytes = 0;
    error = transport_.bulkIn(reinterpret_cast<uint8_t*>(&status), kCswSize, timeoutMs, statusBytes);
    if (error != Error::Ok) {
        return error;
    }
    if (statusBytes != kCswSize || status.signature != kCswSignature || status.tag != block.tag) {
        return Error::Protocol;
    }
    if (status.status != kCswStatusOk) {
        return Error::Status;
    }
    return Error::Ok;
}

Error Scanner::inquiry(Identity& identity, uint32_t timeoutMs) {
    uint8_t cdb[6] = {kInquiryOpcode, 0, 0, 0, static_cast<uint8_t>(kInquiryLength), 0};
    uint8_t raw[kInquiryLength] = {};
    uint32_t got = 0;
    Error error = command(true, cdb, sizeof(cdb), raw, kInquiryLength, timeoutMs, got);
    if (error != Error::Ok) {
        return error;
    }
    if (got < kInquiryLength) {
        return Error::Short;
    }

    identity = {};
    identity.peripheralType = raw[0] & 0x1f;
    copyField(identity.vendor, sizeof(identity.vendor), raw + 8, 8);
    copyField(identity.product, sizeof(identity.product), raw + 16, 16);
    copyField(identity.revision, sizeof(identity.revision), raw + 32, 4);
    return Error::Ok;
}

}  // namespace ds620
