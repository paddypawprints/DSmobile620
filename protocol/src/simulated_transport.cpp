#include "ds620/simulated_transport.hpp"

#include <cstring>

namespace ds620 {
namespace {

constexpr uint32_t kCbwSignature = 0x43425355;
constexpr uint32_t kCswSignature = 0x53425355;
constexpr uint8_t kCbwFlagIn = 0x80;
constexpr size_t kCbwSize = 31;
constexpr size_t kCswSize = 13;

void appendStatus(std::vector<uint8_t>& incoming, uint32_t tag, uint32_t residue, uint8_t status) {
    uint8_t bytes[kCswSize] = {};
    std::memcpy(bytes + 0, &kCswSignature, 4);
    std::memcpy(bytes + 4, &tag, 4);
    std::memcpy(bytes + 8, &residue, 4);
    bytes[12] = status;
    incoming.insert(incoming.end(), bytes, bytes + kCswSize);
}

void appendInquiry(std::vector<uint8_t>& incoming) {
    uint8_t raw[kInquiryLength] = {};
    raw[0] = 0x00;
    raw[1] = 0x80;
    raw[2] = 0x05;
    raw[3] = 0x02;
    raw[4] = 31;
    std::memcpy(raw + 8, "Brother ", 8);
    std::memcpy(raw + 16, "DS-620          ", 16);
    std::memcpy(raw + 32, "0.40", 4);
    incoming.insert(incoming.end(), raw, raw + kInquiryLength);
}

}  // namespace

void SimulatedTransport::setInquiryStatus(uint8_t status) { inquiryStatus_ = status; }

Error SimulatedTransport::abort() {
    phase_ = Phase::Command;
    incoming_.clear();
    return Error::Ok;
}

Error SimulatedTransport::bulkOut(const uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) {
    (void)timeoutMs;
    if (data == nullptr && length > 0) {
        return Error::Argument;
    }
    if (phase_ == Phase::Command) {
        Error error = acceptCommand(data, length);
        if (error == Error::Ok) {
            transferred = length;
        }
        return error;
    }
    if (phase_ == Phase::DataOut) {
        if (length > dataLength_) {
            return Error::Protocol;
        }
        transferred = length;
        dataLength_ -= length;
        if (dataLength_ == 0) {
            queueStatus();
        }
        return Error::Ok;
    }
    return Error::Protocol;
}

Error SimulatedTransport::bulkIn(uint8_t* data, uint32_t length, uint32_t timeoutMs, uint32_t& transferred) {
    (void)timeoutMs;
    if (data == nullptr || length == 0) {
        return Error::Argument;
    }
    if (phase_ != Phase::DataIn && phase_ != Phase::Status) {
        return Error::Protocol;
    }
    if (incoming_.empty()) {
        return Error::Short;
    }
    uint32_t count = length;
    if (count > incoming_.size()) {
        count = static_cast<uint32_t>(incoming_.size());
    }
    std::memcpy(data, incoming_.data(), count);
    incoming_.erase(incoming_.begin(), incoming_.begin() + static_cast<std::ptrdiff_t>(count));
    transferred = count;
    if (incoming_.empty()) {
        phase_ = Phase::Command;
    }
    return Error::Ok;
}

Error SimulatedTransport::acceptCommand(const uint8_t* data, uint32_t length) {
    if (length != kCbwSize) {
        return Error::Protocol;
    }
    uint32_t signature = 0;
    std::memcpy(&signature, data, 4);
    if (signature != kCbwSignature) {
        return Error::Protocol;
    }
    std::memcpy(&tag_, data + 4, 4);
    std::memcpy(&dataLength_, data + 8, 4);
    const uint8_t flags = data[12];
    cdbLength_ = data[14];
    if (cdbLength_ == 0 || cdbLength_ > sizeof(cdb_)) {
        return Error::Protocol;
    }
    std::memset(cdb_, 0, sizeof(cdb_));
    std::memcpy(cdb_, data + 15, cdbLength_);
    dataIn_ = (flags & kCbwFlagIn) != 0;

    if (cdb_[0] == kInquiryOpcode && dataIn_ && dataLength_ >= kInquiryLength) {
        appendInquiry(incoming_);
        appendStatus(incoming_, tag_, dataLength_ - kInquiryLength, inquiryStatus_);
        phase_ = Phase::DataIn;
        return Error::Ok;
    }

    queueStatus();
    return Error::Ok;
}

void SimulatedTransport::queueStatus() {
    appendStatus(incoming_, tag_, dataLength_, 0x01);
    dataLength_ = 0;
    phase_ = Phase::Status;
}

}  // namespace ds620
