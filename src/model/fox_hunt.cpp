// SPDX-License-Identifier: AGPL-3.0-only
#include "fox_hunt.h"
#include <algorithm>
#include <utility>

namespace model {
void FoxHunt::begin(std::string address, unsigned frequency_mhz) {
    reset();
    address_ = std::move(address);
    frequency_mhz_ = frequency_mhz;
}
void FoxHunt::reset() { *this = {}; }
bool FoxHunt::observe(std::string_view sender, unsigned frequency_mhz, int rssi,
                      bool has_rssi, uint32_t now_ms) {
    if (!active() || sender != address_ ||
        (frequency_mhz && frequency_mhz != frequency_mhz_)) return false;
    ++frames_;
    if (has_rssi && rssi <= 0 && rssi >= -127) {
        last_rssi_ = rssi;
        last_seen_ms_ = now_ms;
        if (!sample_count_ || now_ms - last_chart_ms_ >= 200) {
            if (sample_count_ == history_.size())
                std::move(history_.begin() + 1, history_.end(), history_.begin());
            else ++sample_count_;
            last_chart_ms_ = now_ms;
        }
        history_[sample_count_ - 1] = rssi;
    }
    return true;
}
}
