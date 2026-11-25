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
    assert(platform::wireless::frequency_bands("* 2412.0 MHz [1]\n* 5180.0 MHz [36]") == "2.4 / 5 GHz");
    assert(platform::wireless::frequency_bands("* 5955 MHz [1]") == "6 GHz");
    return 0;
}
