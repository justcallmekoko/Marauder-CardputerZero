// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "channel_survey.h"
#include <algorithm>
#include <string>
#include <vector>

namespace platform::wireless {
struct WardriveRadioPlan {
    std::string stable_id;
    std::vector<unsigned> supported;
    std::vector<unsigned> channels;
};

inline bool wardrive_priority_channel(unsigned mhz, unsigned profile) {
    const auto channel = channel_number(mhz);
    if (profile == 1) return mhz < 3000 && (channel == 1 || channel == 6 || channel == 11);
    if (profile == 2) return mhz < 3000 && (channel == 1 || channel == 5 || channel == 9 || channel == 13);
    if (profile == 3) return mhz >= 5000 && mhz < 5900 && channel >= 36 && channel <= 48;
    if (profile == 4) return mhz >= 5000 && mhz < 5900 && channel >= 149 && channel <= 161;
    return false;
}

inline std::vector<WardriveRadioPlan> wardrive_channel_blocks(std::vector<WardriveRadioPlan> radios,
                                                               unsigned priority_profile) {
    std::vector<unsigned> all;
    for (auto& radio : radios) {
        std::sort(radio.supported.begin(), radio.supported.end());
        radio.supported.erase(std::unique(radio.supported.begin(), radio.supported.end()), radio.supported.end());
        all.insert(all.end(), radio.supported.begin(), radio.supported.end());
        radio.channels.clear();
    }
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    if (radios.empty()) return radios;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const auto ideal = std::min(radios.size() - 1, i * radios.size() / all.size());
        std::size_t chosen = radios.size();
        for (std::size_t j = 0; j < radios.size(); ++j) {
            if (!std::binary_search(radios[j].supported.begin(), radios[j].supported.end(), all[i])) continue;
            if (chosen == radios.size() ||
                (j > ideal ? j - ideal : ideal - j) <
                (chosen > ideal ? chosen - ideal : ideal - chosen)) chosen = j;
        }
        if (chosen < radios.size()) radios[chosen].channels.push_back(all[i]);
    }
    // Priority channels get one extra visit per pass; no unsupported channel is introduced.
    for (auto& radio : radios) {
        std::vector<unsigned> preferred;
        for (const auto mhz : radio.channels)
            if (wardrive_priority_channel(mhz, priority_profile)) preferred.push_back(mhz);
        radio.channels.insert(radio.channels.end(), preferred.begin(), preferred.end());
    }
    return radios;
}
}
