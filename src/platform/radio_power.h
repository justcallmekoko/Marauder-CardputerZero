// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstdint>

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
    std::string tune(unsigned mhz);
    int capture_socket();
private:
    int channel_{-1}, child_{-1};
    std::mutex mutex_;
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
    bool position(int32_t& latitude_e7, int32_t& longitude_e7) const;
private:
    int socket_{-1};
    int child_{-1};
    std::string pending_;
    int32_t latitude_e7_{}, longitude_e7_{};
    bool position_valid_{};
    std::chrono::steady_clock::time_point position_time_{};
};
struct BluetoothObservation {
    std::string address, name, address_type;
    int rssi{-128};
};
class BluetoothSession {
public:
    BluetoothSession() = default;
    BluetoothSession(const BluetoothSession&) = delete;
    BluetoothSession& operator=(const BluetoothSession&) = delete;
    ~BluetoothSession();
    bool start(const std::string& adapter);
    void stop();
    std::vector<BluetoothObservation> poll(std::string& status);
    bool active() const { return socket_ >= 0; }
private:
    int socket_{-1}, child_{-1};
    std::string pending_;
};
std::string begin_radio_session();
void end_radio_session(const std::string& directory);
RadioInventory read_radios(std::string id, bool enable, const std::string& directory);
}
