#include "activate.hpp"
#include "dext_transport.hpp"
#include "ds620/scanner.hpp"
#include "ds620/simulated_transport.hpp"

#include <string>

#include <cstdio>
#include <cstring>

namespace {

const char* errorText(ds620::Error error) {
    switch (error) {
        case ds620::Error::Ok:
            return "ok";
        case ds620::Error::Transport:
            return "transport failed";
        case ds620::Error::Protocol:
            return "protocol error";
        case ds620::Error::Status:
            return "scanner reported a failed command";
        case ds620::Error::Short:
            return "short transfer";
        case ds620::Error::Argument:
            return "bad argument";
    }
    return "unknown error";
}

void usage() {
    std::fprintf(stderr,
                 "usage: ds620ctl inquiry [--mock]\n"
                 "\n"
                 "  inquiry         activate DS620Driver, then send SCSI INQUIRY\n"
                 "  --mock          answer from the in-process simulated scanner\n");
}

int inquiry(bool mock) {
    ds620::SimulatedTransport simulated;
    ds620::DextTransport dext;
    ds620::Transport* transport = &simulated;
    if (!mock) {
        std::string activationMessage;
        ds620::Activation activation = ds620::activateDriver(activationMessage);
        if (activation == ds620::Activation::Failed) {
            std::fprintf(stderr, "activation: %s\n",
                         activationMessage.empty() ? "failed" : activationMessage.c_str());
            return 1;
        }
        if (activation == ds620::Activation::NeedsReboot) {
            std::fprintf(stderr, "DS620Driver will become active after a reboot.\n");
            return 1;
        }
        if (!dext.open()) {
            if (dext.openFailure() == ds620::DextTransport::OpenFailure::NotPermitted) {
                std::fprintf(stderr,
                             "DS620Driver owns the scanner, but macOS refused the app's connection. "
                             "The app needs Apple's DriverKit User Client Access entitlement.\n");
            } else {
                std::fprintf(stderr,
                             "DS620Driver is allowed, but it does not own the scanner yet. "
                             "Unplug the scanner, plug it back in, and run inquiry again.\n");
            }
            return 1;
        }
        transport = &dext;
    }

    ds620::Scanner scanner(*transport);
    ds620::Identity identity{};
    ds620::Error error = scanner.inquiry(identity);
    if (error != ds620::Error::Ok) {
        std::fprintf(stderr, "inquiry: %s\n", errorText(error));
        return 1;
    }
    std::printf("vendor: %s\nproduct: %s\nrevision: %s\ntype: %u\n", identity.vendor, identity.product,
                identity.revision, identity.peripheralType);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    bool mock = false;
    const char* command = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mock") == 0) {
            mock = true;
        } else if (command == nullptr) {
            command = argv[i];
        } else {
            usage();
            return 2;
        }
    }
    if (command == nullptr || std::strcmp(command, "inquiry") != 0) {
        usage();
        return command == nullptr ? 2 : 2;
    }
    return inquiry(mock);
}
