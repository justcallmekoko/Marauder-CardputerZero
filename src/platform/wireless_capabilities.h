/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <string>
#include <string_view>

namespace platform::wireless {

enum class CapabilityState {
    Supported,
    Unsupported,
    Unknown,
    Unverified,
};

struct MonitorModeProbe {
    CapabilityState state{CapabilityState::Unknown};
    std::string note;
    std::string bands;
};

CapabilityState monitor_mode_from_iw_phy_info(std::string_view output);
MonitorModeProbe probe_monitor_mode(const std::string& phy_name);

} // namespace platform::wireless
