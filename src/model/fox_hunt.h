// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace model {
class FoxHunt {
public:
    static constexpr std::size_t kHistory = 20;
    void begin(std::string address, unsigned frequency_mhz);
    void reset();
    bool observe(std::string_view sender, unsigned frequency_mhz, int rssi,
                 bool has_rssi, uint32_t now_ms);
    bool active() const { return !address_.empty(); }
    bool fresh(uint32_t now_ms) const { return sample_count_ && now_ms - last_seen_ms_ <= 3000; }
    uint32_t age_ms(uint32_t now_ms) const { return sample_count_ ? now_ms - last_seen_ms_ : 0; }
    const std::string& address() const { return address_; }
    unsigned frequency_mhz() const { return frequency_mhz_; }
    int rssi() const { return last_rssi_; }
    uint64_t frames() const { return frames_; }
    std::size_t sample_count() const { return sample_count_; }
    const std::array<int, kHistory>& history() const { return history_; }

private:
    std::string address_;
    unsigned frequency_mhz_{};
    int last_rssi_{};
    uint32_t last_seen_ms_{};
    uint32_t last_chart_ms_{};
    uint64_t frames_{};
    std::array<int, kHistory> history_{};
    std::size_t sample_count_{};
};
}
