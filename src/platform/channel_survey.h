// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace platform::wireless {
struct ChannelSample { unsigned mhz; uint64_t active, busy; };
std::vector<ChannelSample> channel_survey(const std::string& interface);
}
