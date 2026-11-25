// SPDX-License-Identifier: AGPL-3.0-only
#include "traffic_screen.h"
#include "asset_manager.h"
#include "theme.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace screen {
TrafficScreen::TrafficScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets)
    : BaseScreen(model, assets) {
    init();
}

TrafficScreen::~TrafficScreen() {
    if (timer_) lv_timer_delete(timer_);
}

void TrafficScreen::build_content(lv_obj_t* content) {
    const auto colors = view::palette(view_model().is_dark_mode());
    for (const auto& adapter : view_model().adapters()) {
        if (adapter.stable_id == view_model().selected_radio_id()) interface_ = adapter.interface_name;
    }
    auto* title = lv_label_create(content);
    lv_label_set_text_fmt(title, "Adapter traffic   %s", interface_.c_str());
    lv_obj_set_pos(title, 10, 4);
    auto* font = assets().load_standard_font(14);
    lv_obj_set_style_text_font(title, font ? font : LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(title, colors.text, 0);
    for (size_t i = 0; i < bars_.size(); ++i) {
        auto* bar = bars_[i] = lv_obj_create(content);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 8, 1);
        lv_obj_set_pos(bar, 10 + static_cast<int>(i) * 10, 83);
        lv_obj_set_style_bg_color(bar, colors.info, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }
    status_ = lv_label_create(content);
    lv_obj_set_pos(status_, 10, 93);
    lv_obj_set_style_text_color(status_, colors.text, 0);
    font = assets().load_standard_font(12);
    lv_obj_set_style_text_font(status_, font ? font : LV_FONT_DEFAULT, 0);
    set_key_guide("30 seconds   RX + TX packets/s    Esc  Back");
    if (interface_.empty()) {
        lv_label_set_text(status_, "Select a WiFi adapter first");
        return;
    }
    sample();
    timer_ = lv_timer_create([](lv_timer_t* timer) {
        static_cast<TrafficScreen*>(lv_timer_get_user_data(timer))->sample();
    }, 1000, this);
}

void TrafficScreen::sample() {
    uint64_t rx{}, tx{};
    const std::string path = "/sys/class/net/" + interface_ + "/statistics/";
    if (!(std::ifstream(path + "rx_packets") >> rx) || !(std::ifstream(path + "tx_packets") >> tx)) {
        have_sample_ = false;
        history_.fill(0);
        for (auto* bar : bars_) {
            lv_obj_set_height(bar, 1);
            lv_obj_set_y(bar, 83);
        }
        lv_label_set_text(status_, "Adapter counters unavailable");
        return;
    }
    const uint32_t elapsed = lv_tick_elaps(tick_);
    if (have_sample_ && elapsed && rx >= previous_rx_ && tx >= previous_tx_) {
        const double rate = (double(rx - previous_rx_) + double(tx - previous_tx_)) * 1000 / elapsed;
        std::rotate(history_.begin(), history_.begin() + 1, history_.end());
        history_.back() = rate;
        const double peak = *std::max_element(history_.begin(), history_.end());
        for (size_t i = 0; i < bars_.size(); ++i) {
            const int height = std::max(1, static_cast<int>(58 * history_[i] / std::max(1.0, peak)));
            lv_obj_set_height(bars_[i], height);
            lv_obj_set_y(bars_[i], 84 - height);
        }
        const auto text = std::to_string(std::llround(rate)) + " pkt/s     Peak " + std::to_string(std::llround(peak));
        lv_label_set_text(status_, text.c_str());
    } else {
        lv_label_set_text(status_, "Sampling...");
    }
    have_sample_ = true;
    previous_rx_ = rx;
    previous_tx_ = tx;
    tick_ = lv_tick_get();
}
}
