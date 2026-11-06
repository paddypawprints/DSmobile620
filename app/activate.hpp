#pragma once

#include <string>

namespace ds620 {

enum class Activation {
    Completed,
    NeedsReboot,
    Failed,
};

// Asks macOS to activate the embedded DS620Driver extension and waits until
// the request finishes. User approval leaves this call blocked.
Activation activateDriver(std::string& message);

}  // namespace ds620
