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
class MonitorSession {
public:
    MonitorSession() = default;
    MonitorSession(const MonitorSession&) = delete;
    MonitorSession& operator=(const MonitorSession&) = delete;
    ~MonitorSession();
    std::string start(const std::string& name);
    std::string stop();
private:
    int channel_{-1}, child_{-1};
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
std::string begin_radio_session();
void end_radio_session(const std::string& directory);
RadioInventory read_radios(std::string id, bool enable, const std::string& directory);
}
