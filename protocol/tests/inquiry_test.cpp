#include "ds620/scanner.hpp"
#include "ds620/simulated_transport.hpp"

#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        failures += 1;
    }
}

void testInquiry() {
    ds620::SimulatedTransport transport;
    ds620::Scanner scanner(transport);
    ds620::Identity identity{};
    ds620::Error error = scanner.inquiry(identity);
    expect(error == ds620::Error::Ok, "inquiry succeeds");
    expect(std::strcmp(identity.vendor, "Brother") == 0, "vendor is Brother");
    expect(std::strcmp(identity.product, "DS-620") == 0, "product is DS-620");
    expect(std::strcmp(identity.revision, "0.40") == 0, "revision is 0.40");
    expect(identity.peripheralType == 0, "peripheral type is direct-access");
}

void testFailedStatus() {
    ds620::SimulatedTransport transport;
    transport.setInquiryStatus(0x01);
    ds620::Scanner scanner(transport);
    ds620::Identity identity{};
    ds620::Error error = scanner.inquiry(identity);
    expect(error == ds620::Error::Status, "failed CSW is reported");
}

void testSecondInquiry() {
    ds620::SimulatedTransport transport;
    ds620::Scanner scanner(transport);
    ds620::Identity first{};
    ds620::Identity second{};
    expect(scanner.inquiry(first) == ds620::Error::Ok, "first inquiry");
    expect(scanner.inquiry(second) == ds620::Error::Ok, "second inquiry");
    expect(std::strcmp(second.revision, "0.40") == 0, "second revision");
}

}  // namespace

int main() {
    testInquiry();
    testFailedStatus();
    testSecondInquiry();
    if (failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
