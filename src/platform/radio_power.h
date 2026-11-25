// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>
#include <vector>

namespace platform {
struct RadioDevice {
    std::string id, kind, name, location, product, driver, detail, state, bands;
};
struct RadioInventory {
    std::string message;
    std::vector<RadioDevice> devices;
};
class GpsSession {
public:
    GpsSession() = default;
    GpsSession(const GpsSession&) = delete;
    GpsSession& operator=(const GpsSession&) = delete;
    ~GpsSession();
    bool start(const std::string& id);
    void stop();
    std::string poll();
private:
    int socket_{-1};
    int child_{-1};
    std::string pending_;
};
RadioInventory read_radios(std::string id, bool enable);
}
