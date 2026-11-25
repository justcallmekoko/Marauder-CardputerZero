// SPDX-License-Identifier: AGPL-3.0-only
#include "wifi_scan_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include <algorithm>

namespace screen {
WifiScanScreen::WifiScanScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }

void WifiScanScreen::build_content(lv_obj_t* content) {
    auto label = [&](lv_obj_t* parent, int x, int y, int width, int size) {
        auto* obj = lv_label_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_width(obj, width);
        auto* font = assets().load_standard_font(size);
        lv_obj_set_style_text_font(obj, font ? font : LV_FONT_DEFAULT, 0);
        lv_label_set_text(obj, "");
        lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
        return obj;
    };
    title_ = label(content, 8, 1, 198, 14);
    count_ = label(content, 205, 3, 106, 11);
    lv_obj_set_style_text_align(count_, LV_TEXT_ALIGN_RIGHT, 0);
    message_ = label(content, 9, 23, 303, 12);
    lv_label_set_long_mode(message_, LV_LABEL_LONG_WRAP);
    for (size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 5, 21 + i * 25);
        lv_obj_set_size(row.box, 310, 23);
        lv_obj_clear_flag(row.box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(row.box, 4, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        row.name = label(row.box, 7, 3, 186, 12);
        lv_label_set_long_mode(row.name, LV_LABEL_LONG_DOT);
        row.metric = label(row.box, 194, 4, 108, 10);
        lv_obj_set_style_text_align(row.metric, LV_TEXT_ALIGN_RIGHT, 0);
    }
    reactive::observe_obj(content, view_model().inventory_subject(), [](lv_observer_t* o, lv_subject_t*) {
        static_cast<WifiScanScreen*>(lv_observer_get_user_data(o))->render();
    }, this);
}

void WifiScanScreen::render() {
    auto& vm = view_model();
    const auto& items = vm.wifi_items();
    const auto cursor = vm.wifi_cursor();
    const bool stations = vm.station_list();
    const bool details = vm.wifi_details_open() && cursor < items.size();
    const auto colors = view::palette(vm.is_dark_mode());
    auto accent = stations ? colors.info : colors.success;
    lv_label_set_text(title_, stations ? (details ? "Station info" : "Hotspot stations") :
                                         (details ? "Access point info" : "Access points"));
    lv_obj_set_style_text_color(title_, colors.text, 0);
    const auto count = vm.scanning() ? (items.empty() ? "Searching" : "Refreshing") : !vm.wifi_error().empty() ? "Refresh failed" :
        items.empty() ? "0 seen" : std::to_string(cursor + 1) + " / " + std::to_string(items.size());
    lv_label_set_text(count_, count.c_str());
    lv_obj_set_style_text_color(count_, accent, 0);
    if (cursor < first_) first_ = cursor;
    if (cursor >= first_ + rows_.size()) first_ = cursor + 1 - rows_.size();
    first_ = std::min(first_, items.size() > rows_.size() ? items.size() - rows_.size() : size_t{0});
    for (size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = first_ + i;
        if (details || index >= items.size()) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        const auto& item = items[index];
        lv_obj_set_style_bg_color(row.box, index == cursor ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, index == cursor ? accent : colors.border, 0);
        lv_label_set_text(row.name, item.ssid.c_str());
        lv_obj_set_style_text_color(row.name, colors.text, 0);
        const auto metric = (stations ? std::string{} : "CH " + std::to_string(item.channel) + "   ") +
            (item.rssi ? std::to_string(item.rssi) + " dBm" : "RSSI unknown");
        lv_label_set_text(row.metric, metric.c_str());
        lv_obj_set_style_text_color(row.metric, index == cursor ? accent : colors.text_disabled, 0);
    }
    std::string message;
    if (details) {
        const auto& item = items[cursor];
        message = stations ? item.bssid : item.ssid + "\n" + item.bssid;
        message += "\nVendor  " + (item.vendor.empty() ? std::string("Pending") : item.vendor.substr(0, 40));
        if (stations) message += "\nRX  " + (item.rx_packets.empty() ? "unknown" : item.rx_packets) +
            "    TX  " + (item.tx_packets.empty() ? "unknown" : item.tx_packets) + " packets";
        else message += "\nCH " + std::to_string(item.channel) + "    " + std::to_string(item.frequency_mhz) +
            " MHz    " + std::to_string(item.rssi) + " dBm\n" + item.security;
    } else if (items.empty()) {
        message = !vm.wifi_error().empty() ? vm.wifi_error() : vm.scanning() ? "Refreshing inventory..." :
            stations ? "No stations connected to this hotspot" : "No access points found";
    }
    lv_obj_set_style_text_color(message_, colors.text, 0);
    lv_label_set_text(message_, message.c_str());
    set_key_guide(details ? "F/X Browse    D Refresh    Esc List" : items.empty() ? "D Refresh    Esc Back" :
                  "F/X Browse    Enter Info    D Refresh    Esc Back");
}
}
