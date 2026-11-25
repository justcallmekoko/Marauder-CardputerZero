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
    const int mode = vm.wifi_view();
    const bool details = (mode == 1 || mode == 3) && cursor < items.size();
    const auto* clients = cursor < items.size() ? &items[cursor].clients : nullptr;
    const bool client_list = mode == 2 && clients;
    const auto selected = client_list || mode == 3 ? vm.wifi_client_cursor() : cursor;
    const auto total = (client_list || mode == 3) && clients ? clients->size() : items.size();
    const auto colors = view::palette(vm.is_dark_mode());
    auto accent = colors.success;
    lv_label_set_text(title_, mode == 3 ? "Client info" : client_list ? "Observed clients" : details ? "Access point info" : "Access points");
    lv_obj_set_style_text_color(title_, colors.text, 0);
    const auto count = !vm.wifi_error().empty() ? "Capture failed" :
        (mode == 0 && items.empty()) ? "Listening" : total == 0 ? "0 seen" :
        std::to_string(selected + 1) + " / " + std::to_string(total);
    lv_label_set_text(count_, count.c_str());
    lv_obj_set_style_text_color(count_, accent, 0);
    if (mode != last_mode_) { first_ = 0; last_mode_ = mode; }
    if (selected < first_) first_ = selected;
    if (selected >= first_ + rows_.size()) first_ = selected + 1 - rows_.size();
    first_ = std::min(first_, total > rows_.size() ? total - rows_.size() : size_t{0});
    for (size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = first_ + i;
        if (details || index >= total) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(row.box, index == selected ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, index == selected ? accent : colors.border, 0);
        const auto name = client_list ? (*clients)[index].mac : items[index].ssid;
        lv_label_set_text(row.name, name.c_str());
        lv_obj_set_style_text_color(row.name, colors.text, 0);
        const auto metric = client_list ?
            std::to_string((*clients)[index].frames) + " fr  " +
                ((*clients)[index].has_rssi ? std::to_string((*clients)[index].rssi) + "dBm" : "RSSI ?") :
            "CH " + std::to_string(items[index].channel) + "  " +
                (items[index].has_rssi ? std::to_string(items[index].rssi) + "dBm" : "RSSI ?");
        lv_label_set_text(row.metric, metric.c_str());
        lv_obj_set_style_text_color(row.metric, index == selected ? accent : colors.text_disabled, 0);
    }
    std::string message;
    if (mode == 3 && clients && vm.wifi_client_cursor() < clients->size()) {
        const auto& client = (*clients)[vm.wifi_client_cursor()];
        message = client.mac + "\n" + client.vendor.substr(0, 40) +
            "\nRSSI  " + (client.has_rssi ? std::to_string(client.rssi) + " dBm" : "Not observed uplink") +
            "\nFrames  " + std::to_string(client.frames) +
            "\nAP  " + items[cursor].ssid;
    } else if (mode == 1 && cursor < items.size()) {
        const auto& item = items[cursor];
        message = item.ssid + "\n" + item.bssid;
        message += "\nVendor  " + item.vendor.substr(0, 40);
        message += "\nCH " + std::to_string(item.channel) + "    " +
            (item.has_rssi ? std::to_string(item.rssi) + " dBm" : "RSSI unknown");
        message += "\n" + (item.security.empty() ? "Security unknown" : item.security) +
            "   " + std::to_string(item.frames) + " frames";
        message += "\n" + std::to_string(item.clients.size()) + " observed clients";
    } else if (client_list && clients->empty()) {
        message = "No client frames observed yet";
    } else if (mode == 0 && items.empty()) {
        message = !vm.wifi_error().empty() ? vm.wifi_error() : vm.scanning() ? "Listening on supported channels..." :
            "No access points found";
    }
    lv_obj_set_style_text_color(message_, colors.text, 0);
    lv_label_set_text(message_, message.c_str());
    set_key_guide(mode == 3 ? "Esc Clients" : mode == 2 ? "F/X Browse  Enter Info  Esc AP" :
                  mode == 1 ? "Enter Clients  Esc List" : "F/X Browse  Enter Info  Esc Back");
}
}
