// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>
#include <cstdint>
#include <string>

namespace screen {
class TrafficScreen : public BaseScreen {
public:
    TrafficScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
    ~TrafficScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void sample();
    std::string interface_;
    std::array<double, 30> history_{};
    std::array<lv_obj_t*, 30> bars_{};
    lv_obj_t* status_{};
    lv_timer_t* timer_{};
    uint64_t previous_rx_{}, previous_tx_{};
    uint32_t tick_{};
    bool have_sample_{};
};
}
