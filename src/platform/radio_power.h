// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>
#include <vector>

namespace platform {
struct RadioDevice {
    std::string id, kind, name, location, product, driver, detail, state;
};
struct RadioInventory {
    std::string message;
    std::vector<RadioDevice> devices;
};
RadioInventory read_radios(std::string id, bool enable);
}
