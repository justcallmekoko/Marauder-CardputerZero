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
    auto* title = title_ = lv_label_create(content);
    view_model().take_traffic_step();
    lv_label_set_text_fmt(title, "RX + TX packets   %s", interface_.c_str());
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

void TrafficScreen::draw(double value, double scale) {
    std::rotate(history_.begin(), history_.begin() + 1, history_.end());
    history_.back() = value;
    if (scale <= 0) scale = std::max(1.0, *std::max_element(history_.begin(), history_.end()));
    for (size_t i = 0; i < bars_.size(); ++i) {
        const int height = std::max(1, static_cast<int>(58 * std::min(history_[i] / scale, 1.0)));
        lv_obj_set_height(bars_[i], height);
        lv_obj_set_y(bars_[i], 84 - height);
    }
}

void TrafficScreen::sample() {
    bool fresh = false;
    if (survey_.valid() && survey_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        channels_ = survey_.get();
        fresh = true;
    }
    if (!survey_.valid()) {
        survey_ = std::async(std::launch::async, platform::wireless::channel_survey, interface_);
    }
    const auto found = std::find_if(channels_.begin(), channels_.end(),
        [&](const auto& item) { return item.mhz == frequency_; });
    const int current = found == channels_.end() ? 0 : 1 + int(found - channels_.begin());
    const int choices = int(channels_.size()) + 1;
    const int index = (current + view_model().take_traffic_step() + choices) % choices;
    const unsigned frequency = index ? channels_[index - 1].mhz : 0;
    if (frequency != frequency_) {
        frequency_ = frequency;
        history_.fill(0);
        draw(0, 100);
        previous_channel_ = {};
        have_sample_ = false;
    }
    set_key_guide(choices > 1 ? "Z/C Channel    Esc Back" : "Interface packets/s    Esc Back");
    if (index) {
        const auto& channel = channels_[index - 1];
        const unsigned number = platform::wireless::channel_number(frequency_);
        if (number) {
            const char* band = frequency_ < 2500 ? "2.4" : frequency_ < 5000 ? "4.9" :
                               frequency_ < 5935 ? "5" : frequency_ < 8000 ? "6" : "60";
            lv_label_set_text_fmt(title_, "Channel %u   %s GHz", number, band);
        } else lv_label_set_text(title_, "Channel unknown");
        lv_label_set_text(status_, "Waiting for driver airtime sample");
        if (fresh && previous_channel_.mhz == frequency_ && channel.active > previous_channel_.active &&
            channel.busy >= previous_channel_.busy) {
            const double busy = std::min(100.0, 100.0 * (channel.busy - previous_channel_.busy) /
                                                   (channel.active - previous_channel_.active));
            draw(busy, 100);
            lv_label_set_text_fmt(status_, "%d%% busy airtime", static_cast<int>(std::round(busy)));
        }
        previous_channel_ = channel;
        return;
    }
    lv_label_set_text_fmt(title_, "RX + TX packets   %s", interface_.c_str());
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
        draw(rate, 0);
        const double peak = *std::max_element(history_.begin(), history_.end());
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
