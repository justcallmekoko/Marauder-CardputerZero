/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wireless_capabilities.h"

#include <cassert>

using platform::wireless::CapabilityState;
using platform::wireless::monitor_mode_from_iw_phy_info;

int main() {
    const char* supported = R"(Wiphy phy1
	Supported interface modes:
		 * managed
		 * AP
		 * monitor
	Band 1:
)";
    const char* unsupported = R"(Wiphy phy0
	Supported interface modes:
		 * managed
		 * AP
	Band 1:
)";
    assert(monitor_mode_from_iw_phy_info(supported) == CapabilityState::Supported);
    assert(monitor_mode_from_iw_phy_info(unsupported) == CapabilityState::Unsupported);
    assert(monitor_mode_from_iw_phy_info("Wiphy phy2\n") == CapabilityState::Unknown);
    assert(monitor_mode_from_iw_phy_info("Supported interface modes:\n") == CapabilityState::Unknown);
    return 0;
}
