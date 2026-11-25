// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace platform::wireless {
constexpr unsigned channel_number(unsigned mhz) {
    if (mhz == 2484) return 14;
    if (mhz == 5935) return 2;
    if (mhz >= 2412 && mhz <= 2472 && (mhz-2407)%5 == 0) return (mhz-2407)/5;
    if (mhz >= 4910 && mhz <= 4980 && mhz%5 == 0) return (mhz-4000)/5;
    if (mhz > 5000 && mhz < 5925 && mhz%5 == 0) return (mhz-5000)/5;
    if (mhz >= 5955 && mhz <= 7115 && mhz%5 == 0) return (mhz-5950)/5;
    if (mhz >= 58320 && mhz <= 70200 && (mhz-56160)%2160 == 0) return (mhz-56160)/2160;
    return 0;
}
struct ChannelSample { unsigned mhz; uint64_t active, busy; };
std::vector<ChannelSample> channel_survey(const std::string& interface);
}
