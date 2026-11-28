// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "channel_survey.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace platform::wireless {
struct WardriveChannel {
    unsigned frequency_mhz{};
    unsigned channel{};
    std::size_t access_points{};
};

class WardriveChannels {
public:
    // A later beacon may reveal the primary channel of an AP first seen without one.
    bool observe(const std::string& bssid, unsigned frequency_mhz, bool advertised) {
        auto [item, inserted] = frequencies_.try_emplace(bssid, frequency_mhz);
        if (!inserted && frequency_mhz && (advertised || !item->second)) {
            item->second = frequency_mhz;
        }
        return inserted;
    }
    void clear() { frequencies_.clear(); }
    std::size_t size() const { return frequencies_.size(); }
    std::vector<WardriveChannel> distribution() const {
        std::unordered_map<unsigned, std::size_t> counts;
        for (const auto& [bssid, frequency] : frequencies_) {
            (void)bssid;
            ++counts[frequency];
        }
        std::vector<WardriveChannel> result;
        result.reserve(counts.size());
        for (const auto& [frequency, count] : counts)
            result.push_back({frequency, channel_number(frequency), count});
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            // Put the unknown bucket after numbered channels.
            return (a.frequency_mhz ? a.frequency_mhz : ~0u) <
                   (b.frequency_mhz ? b.frequency_mhz : ~0u);
        });
        return result;
    }
private:
    std::unordered_map<std::string, unsigned> frequencies_;
};
}
