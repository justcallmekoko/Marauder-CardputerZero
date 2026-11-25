/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include "wireless_capabilities.h"

#include <string>
#include <vector>

namespace platform::wireless {

struct AdapterInfo {
    std::string interface_name;
    std::string ifindex;
    std::string operstate;
    std::string mac_address;
    bool permanent_mac{false};
    std::string driver;
    std::string phy_name;
    std::string bus_type;
    std::string device_path;
    std::string vendor_id;
    std::string product_id;
    std::string manufacturer;
    std::string product_name;
    std::string bands;
    std::string monitor_status;
    std::string display_name() const;
    std::string serial;
    std::string stable_id;
    CapabilityState monitor_mode{CapabilityState::Unknown};
    CapabilityState injection{CapabilityState::Unverified};
    std::string capability_note;
};

struct Catalog {
    std::vector<AdapterInfo> adapters;
    std::string error;
};

Catalog enumerate();
const char* capability_label(CapabilityState state);

} // namespace platform::wireless
