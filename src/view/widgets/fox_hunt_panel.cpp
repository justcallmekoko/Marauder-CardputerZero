// SPDX-License-Identifier: AGPL-3.0-only
#include "fox_hunt_panel.h"
#include "censor_text.h"
#include "asset_manager.h"
#include "channel_survey.h"
#include "theme.h"
#include <algorithm>

namespace view::widgets {
FoxHuntPanel::FoxHuntPanel(lv_obj_t* parent, app::AssetManager& assets) {
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_pos(root_, 0, 17);
    lv_obj_set_size(root_, 320, 107);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    auto label_at = [&](int x, int y, int width, int size) {
        auto* item = lv_label_create(root_);
        lv_obj_set_pos(item, x, y);
        lv_obj_set_width(item, width);
        lv_label_set_long_mode(item, LV_LABEL_LONG_DOT);
        auto* font = assets.load_standard_font(size);
        lv_obj_set_style_text_font(item, font ? font : LV_FONT_DEFAULT, 0);
        return item;
    };
    name_ = label_at(8, 2, 300, 14);
    address_ = label_at(8, 21, 300, 10);
    vendor_ = label_at(8, 36, 152, 10);
    signal_ = label_at(8, 52, 150, 24);
    status_ = label_at(8, 80, 150, 11);
    meta_ = label_at(8, 95, 150, 10);
    chart_ = lv_obj_create(root_);
    lv_obj_remove_style_all(chart_);
    lv_obj_set_pos(chart_, 163, 39);
    lv_obj_set_size(chart_, 149, 66);
    lv_obj_set_style_radius(chart_, 4, 0);
    lv_obj_set_style_bg_opa(chart_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart_, 1, 0);
    lv_obj_clear_flag(chart_, LV_OBJ_FLAG_SCROLLABLE);
    for (std::size_t i = 0; i < bars_.size(); ++i) {
        bars_[i] = lv_obj_create(chart_);
        lv_obj_remove_style_all(bars_[i]);
        lv_obj_set_pos(bars_[i], 4 + static_cast<int>(i) * 7, 61);
        lv_obj_set_size(bars_[i], 5, 1);
        lv_obj_set_style_radius(bars_[i], 2, 0);
        lv_obj_set_style_bg_opa(bars_[i], LV_OPA_COVER, 0);
        lv_obj_add_flag(bars_[i], LV_OBJ_FLAG_HIDDEN);
    }
    show(false);
}
void FoxHuntPanel::show(bool visible) {
    if (visible) lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}
void FoxHuntPanel::render(const model::FoxHunt& hunt, const std::string& label,
                          const std::string& vendor, lv_color_t accent, bool dark_mode, bool tuned, bool censor) {
    const auto colors = view::palette(dark_mode);
    const auto now = lv_tick_get();
    const bool fresh = hunt.fresh(now);
    lv_label_set_text(name_, label.c_str());
    const auto address = censor ? model::censor::mac(hunt.address()) : hunt.address();
    lv_label_set_text(address_, address.c_str());
    lv_label_set_text(vendor_, vendor.empty() ? "Vendor unknown" : vendor.c_str());
    const auto signal = hunt.sample_count() ? std::to_string(hunt.rssi()) + " dBm" : "-- dBm";
    lv_label_set_text(signal_, signal.c_str());
    const auto status = !tuned ? "TUNING CHANNEL" : fresh ? "SIGNAL LIVE" : hunt.sample_count() ?
        "LAST " + std::to_string(hunt.age_ms(now) / 1000) + "s AGO" : "WAITING FOR SIGNAL";
    lv_label_set_text(status_, status.c_str());
    const auto meta = "CH " + std::to_string(platform::wireless::channel_number(hunt.frequency_mhz())) +
        "   " + std::to_string(hunt.frames()) + " frames";
    lv_label_set_text(meta_, meta.c_str());
    lv_obj_set_style_text_color(name_, colors.text, 0);
    lv_obj_set_style_text_color(address_, colors.text_disabled, 0);
    lv_obj_set_style_text_color(vendor_, colors.text_disabled, 0);
    lv_obj_set_style_text_color(signal_, fresh ? accent : colors.text_disabled, 0);
    lv_obj_set_style_text_color(status_, tuned && fresh ? colors.success : colors.warning, 0);
    lv_obj_set_style_text_color(meta_, colors.text_disabled, 0);
    lv_obj_set_style_bg_color(chart_, colors.surface, 0);
    lv_obj_set_style_border_color(chart_, colors.border, 0);
    for (std::size_t i = 0; i < bars_.size(); ++i) {
        auto* bar = bars_[i];
        if (i >= hunt.sample_count()) { lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_HIDDEN);
        const int height = std::clamp((hunt.history()[i] + 95) * 56 / 60, 2, 56);
        lv_obj_set_pos(bar, 4 + static_cast<int>(i) * 7, 61 - height);
        lv_obj_set_height(bar, height);
        lv_obj_set_style_bg_color(bar, accent, 0);
        lv_obj_set_style_bg_opa(bar, fresh ? static_cast<lv_opa_t>(90 + i * 8) : LV_OPA_40, 0);
    }
}
}
