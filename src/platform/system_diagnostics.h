/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform::diagnostics {

struct NetworkInterface {
    std::string name;
    std::string driver;
    std::string state;
    bool wireless{false};
};

struct UsbDevice {
    std::string vendor_id;
    std::string product_id;
    std::string name;
};

struct ToolStatus {
    std::string name;
    bool available{false};
};

struct Snapshot {
    std::string model;
    std::string operating_system;
    std::string kernel;
    std::string uptime;
    std::string memory;
    std::string storage;
    std::vector<NetworkInterface> network_interfaces;
    std::vector<UsbDevice> usb_devices;
    std::vector<std::string> peripheral_nodes;
    std::vector<ToolStatus> tools;
    bool gpsd_socket{false};
};

Snapshot collect();
std::string dashboard_summary(const Snapshot& snapshot);
std::string detail_text(const Snapshot& snapshot);

} // namespace platform::diagnostics
