// SPDX-License-Identifier: AGPL-3.0-only
#include "wifi_scan_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "channel_survey.h"
#include <algorithm>

namespace screen {
namespace {
lv_color_t frame_color(platform::wireless::FrameKind kind) {
    using K = platform::wireless::FrameKind;
    switch (kind) {
        case K::Beacon: return lv_color_hex(0x97BA8D);
        case K::Probe: return lv_color_hex(0x8FBFC9);
        case K::Deauth: return lv_color_hex(0xC68B92);
        case K::Eapol: return lv_color_hex(0xC6A0CB);
        case K::Sae: return lv_color_hex(0xD4B58E);
        case K::Data: return lv_color_hex(0x91A9CC);
        case K::Association: return lv_color_hex(0xAEC39C);
        case K::Authentication: return lv_color_hex(0xC2AB90);
        case K::Action: return lv_color_hex(0xAAB2C5);
        case K::Control: return lv_color_hex(0x9CAEB7);
        default: return lv_color_hex(0xA1A2A8);
    }
}
}
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
    title_ = label(content, 8, 1, 152, 14);
    builder_ = label(content, 158, 3, 45, 11);
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
        row.detail = label(row.box, 7, 13, 296, 9);
        lv_obj_add_flag(row.detail, LV_OBJ_FLAG_HIDDEN);
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
    const bool operations = mode == 4 || mode == 5;
    const bool raw_feed = mode == 7;
    const bool feed = mode == 6 || raw_feed;
    const bool station_list = mode == 8;
    const bool filter_list = mode == 9;
    const auto selected = filter_list ? vm.raw_filter_cursor() : station_list ? vm.station_cursor() :
        operations ? vm.wifi_operation_cursor() : client_list || mode == 3 ? vm.wifi_client_cursor() : cursor;
    const auto total = operations ? (mode == 4 ? size_t{4} : size_t{3}) :
        filter_list ? static_cast<size_t>(platform::wireless::FrameKind::Count) + 1 :
        station_list ? vm.station_refs().size() :
        (client_list || mode == 3) && clients ? clients->size() : items.size();
    const auto colors = view::palette(vm.is_dark_mode());
    auto accent = colors.success;
    lv_label_set_text(title_, raw_feed ? "Raw frames" : filter_list ? "Frame filters" : station_list ? "Stations" :
                      feed ? (vm.wifi_feed_station() ? "Station traffic" : "AP traffic") :
                      mode == 5 ? "Station operations" : mode == 4 ? "AP operations" :
                      mode == 3 ? "Client info" : client_list ? "Observed clients" :
                      details ? vm.wifi_info_tab() == 1 ? "AP authentication" :
                                vm.wifi_info_tab() == 2 ? "AP encryption" : "Access point info" :
                      "Access points");
    lv_obj_set_style_text_color(title_, colors.text, 0);
    lv_label_set_text(builder_, vm.scanning() ? (vm.list_builder_active() ? "BUILD" : "B OFF") : "");
    lv_obj_set_style_text_color(builder_, vm.list_builder_active() ? colors.success : colors.text_disabled, 0);
    const auto count = raw_feed ? std::to_string(vm.raw_displayed_count()) + "/" +
                                    std::to_string(vm.raw_feed_count()) :
        mode == 6 ? std::to_string(vm.wifi_feed_count()) + " frames" :
        !vm.wifi_error().empty() ? "Needs attention" :
        (mode == 0 && items.empty()) ? "Listening" : total == 0 ? "0 seen" :
        std::to_string(selected + 1) + " / " + std::to_string(total);
    lv_label_set_text(count_, count.c_str());
    lv_obj_set_style_text_color(count_, accent, 0);
    if (mode != last_mode_) { first_ = 0; last_mode_ = mode; }
    if (!feed) {
        if (selected < first_) first_ = selected;
        if (selected >= first_ + rows_.size()) first_ = selected + 1 - rows_.size();
        first_ = std::min(first_, total > rows_.size() ? total - rows_.size() : size_t{0});
    }
    const auto& frames = raw_feed ? vm.raw_feed_frames() : vm.wifi_feed_frames();
    const auto offset = raw_feed ? vm.raw_feed_offset() : vm.wifi_feed_offset();
    const auto feed_end = frames.size() > offset ? frames.size() - offset : size_t{0};
    const auto feed_start = feed_end > rows_.size() ? feed_end - rows_.size() : size_t{0};
    for (size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = feed ? feed_start + i : first_ + i;
        if (details || (feed ? index >= feed_end : index >= total)) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        const bool compact = feed || operations;
        lv_obj_set_y(row.box, (compact ? 36 : 21) + static_cast<int>(i) * (compact ? 21 : 25));
        lv_obj_set_height(row.box, compact ? 20 : 23);
        lv_obj_set_style_bg_color(row.box, !feed && index == selected ? colors.button : colors.surface, 0);
        const auto kind = filter_list && index ? static_cast<platform::wireless::FrameKind>(index - 1) :
            feed ? frames[index].kind : platform::wireless::FrameKind::Other;
        lv_obj_set_style_border_color(row.box, (feed || (filter_list && index)) ? frame_color(kind) :
                                             index == selected ? accent : colors.border, 0);
        if (feed) {
            const auto& frame = frames[index];
            lv_obj_remove_flag(row.detail, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(row.name, 0);
            lv_obj_set_y(row.metric, 1);
            lv_obj_set_y(row.detail, 11);
            lv_label_set_text(row.name, frame.type.c_str());
            const auto signal = frame.has_rssi ? std::to_string(frame.rssi) + " dBm" : "";
            lv_label_set_text(row.metric, signal.c_str());
            const auto display_address = [&](const std::string& mac) -> std::string {
                if (mac.empty()) return "?";
                if (!raw_feed && cursor < items.size() && mac == items[cursor].bssid &&
                    !items[cursor].ssid.empty() && items[cursor].ssid != "[hidden/unknown]")
                    return items[cursor].ssid;
                return mac;
            };
            const std::string route = display_address(frame.sender) + " > " + display_address(frame.receiver);
            lv_label_set_text(row.detail, route.c_str());
            lv_obj_set_style_text_color(row.detail, colors.text_disabled, 0);
            lv_obj_set_style_text_color(row.name, frame_color(frame.kind), 0);
            lv_obj_set_style_text_color(row.metric, colors.text_disabled, 0);
            continue;
        }
        lv_obj_add_flag(row.detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(row.name, 3);
        lv_obj_set_y(row.metric, 4);
        const bool record = operations && index == (mode == 4 ? 3u : 2u);
        const auto scope = mode == 4 ? platform::pcap::Scope::AccessPoint : platform::pcap::Scope::Station;
        const std::string target = mode == 4 ? items[cursor].bssid :
            clients && vm.wifi_client_cursor() < clients->size() ? (*clients)[vm.wifi_client_cursor()].mac : "";
        const bool recording_target = record && vm.pcap_recording_target(scope, target);
        const auto& station = station_list ? vm.station_refs()[index] : std::pair<size_t, size_t>{0, 0};
        const auto station_client = station_list ? items[station.first].clients[station.second] :
            platform::wireless::WifiNetwork::Client{};
        if (station_list) {
            lv_obj_remove_flag(row.detail, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(row.name, 0);
            lv_obj_set_y(row.metric, 1);
            lv_obj_set_y(row.detail, 12);
            const auto association = "AP " + items[station.first].ssid +
                "  CH " + std::to_string(items[station.first].channel);
            lv_label_set_text(row.detail, association.c_str());
            lv_obj_set_style_text_color(row.detail, colors.text_disabled, 0);
        }
        const std::string name = filter_list ? (index ? platform::wireless::frame_kind_label(kind) : "All frames") :
            station_list ? station_client.mac : operations ?
            (record ? recording_target ? (mode == 4 ? "Stop AP recording" : "Stop station recording") :
                                       (mode == 4 ? "Record this AP" : "Record this station") :
             mode == 4 ? (index == 0 ? "Information" : index == 1 ? "Observed clients" : "Traffic filter") :
                         (index == 0 ? "Information" : "Traffic filter")) :
            client_list ? (*clients)[index].mac : items[index].ssid;
        lv_label_set_text(row.name, name.c_str());
        lv_obj_set_style_text_color(row.name, filter_list && index ? frame_color(kind) : colors.text, 0);
        const auto metric = filter_list ? (index ? (vm.raw_filter_mask() & platform::wireless::frame_kind_bit(kind) ? "ON" : "OFF") :
                                              (vm.raw_filter_mask() ? "OFF" : "ON")) :
            station_list ? std::to_string(station_client.frames) + " fr  " +
                (station_client.has_rssi ? std::to_string(station_client.rssi) + "dBm" : "RSSI ?") :
            operations ? (record ? recording_target ? "REC" : vm.pcap_active() ? "BUSY" : "START" :
                                           name == "Traffic filter" ? "LIVE" : "VIEW") : client_list ?
            std::to_string((*clients)[index].frames) + " fr  " +
                ((*clients)[index].has_rssi ? std::to_string((*clients)[index].rssi) + "dBm" : "RSSI ?") :
            "CH " + std::to_string(items[index].channel) + "  " +
                (items[index].has_rssi ? std::to_string(items[index].rssi) + "dBm" : "RSSI ?");
        lv_label_set_text(row.metric, metric.c_str());
        lv_obj_set_style_text_color(row.metric, recording_target ? colors.error : index == selected ? accent : colors.text_disabled, 0);
    }
    std::string message;
    if (raw_feed) {
        const auto channel = platform::wireless::channel_number(static_cast<int>(vm.wifi_tuned_frequency()));
        message = "CH " + std::to_string(channel) + "   " +
            (vm.raw_filter_mask() ? "FILTERED" : "ALL TYPES") +
            (vm.raw_feed_offset() ? "   HISTORY" : "   LIVE");
        if (!vm.wifi_error().empty()) message = vm.wifi_error();
        else if (frames.empty()) message = "CH " + std::to_string(channel) + "   Waiting for frames";
    } else if (feed && cursor < items.size()) {
        const auto channel = items[cursor].channel;
        message = "CH " + std::to_string(channel) + "   " + vm.wifi_feed_target() +
            (vm.wifi_tuned_frequency() == static_cast<unsigned>(items[cursor].frequency_mhz) ?
                (vm.wifi_feed_offset() ? "   HISTORY" : "   LIVE") : "   TUNING");
        if (!vm.wifi_error().empty()) message = vm.wifi_error();
        if (frames.empty() && vm.wifi_error().empty()) message = "CH " + std::to_string(channel) + "   Waiting for frames";
    } else if (operations && cursor < items.size()) {
        message = !vm.wifi_error().empty() ? vm.wifi_error() : mode == 4 ? items[cursor].ssid + "   CH " + std::to_string(items[cursor].channel) :
            clients && vm.wifi_client_cursor() < clients->size() ? (*clients)[vm.wifi_client_cursor()].mac : "";
    } else if (mode == 3 && clients && vm.wifi_client_cursor() < clients->size()) {
        const auto& client = (*clients)[vm.wifi_client_cursor()];
        message = client.mac + "\n" + client.vendor.substr(0, 40) +
            "\nRSSI  " + (client.has_rssi ? std::to_string(client.rssi) + " dBm" : "Not observed uplink") +
            "\nFrames  " + std::to_string(client.frames) +
            "\nAP  " + items[cursor].ssid;
    } else if (mode == 1 && cursor < items.size()) {
        const auto& item = items[cursor];
        const auto& security = item.security_info;
        if (vm.wifi_info_tab() == 1) {
            message = item.ssid + "\n" + (security.summary.empty() ? "Security unknown" : security.summary) +
                "\nAKM  " + (security.akm.empty() ? "Unknown" : security.akm) +
                "\nPMF  " + (security.pmf.empty() ? "Unknown" : security.pmf) +
                "\nRSNXE  " + (security.rsnxe.empty() ? "Unknown" : security.rsnxe);
            if (!security.note.empty()) message += "\nNote  " + security.note;
        } else if (vm.wifi_info_tab() == 2) {
            message = item.ssid + "\nPairwise  " + (security.pairwise.empty() ? "Unknown" : security.pairwise) +
                "\nGroup  " + (security.group.empty() ? "Unknown" : security.group) +
                "\nMgmt  " + (security.group_management.empty() ? "Unknown" : security.group_management);
            if (!security.legacy_wpa.empty()) message += "\nLegacy  " + security.legacy_wpa;
        } else {
            message = item.ssid + "\n" + item.bssid;
            message += "\nVendor  " + item.vendor.substr(0, 40);
            message += "\nCH " + std::to_string(item.channel) + "    " +
                (item.has_rssi ? std::to_string(item.rssi) + " dBm" : "RSSI unknown");
            message += "\n" + (item.security.empty() ? "Security unknown" : item.security) +
                "   " + std::to_string(item.frames) + " frames";
            message += "\n" + std::to_string(item.clients.size()) + " observed clients";
        }
    } else if (station_list && vm.station_refs().empty()) {
        message = vm.list_builder_active() ? "Waiting for associated station frames" : "Press B to build station list";
    } else if (client_list && clients->empty()) {
        message = "No client frames observed yet";
    } else if (mode == 0 && items.empty()) {
        message = !vm.wifi_error().empty() ? vm.wifi_error() :
            vm.raw_session() && !vm.list_builder_active() ? "Press B to build AP list" :
            vm.scanning() ? "Listening on supported channels..." :
            "No access points found";
    }
    lv_obj_set_style_text_color(message_, colors.text, 0);
    auto* message_font = assets().load_standard_font(feed || (mode == 1 && vm.wifi_info_tab()) ? 10 : 12);
    lv_obj_set_style_text_font(message_, message_font ? message_font : LV_FONT_DEFAULT, 0);
    lv_label_set_text(message_, message.c_str());
    set_key_guide(raw_feed ? "F/X Scroll  T Filter  B Build  Enter APs  D STA" :
                  filter_list ? "F/X Browse  Enter Toggle  T/Esc Feed" :
                  station_list ? "F/X Browse  Enter Open  B Build  Esc Feed" :
                  feed ? "F/X Scroll  Esc Operations" : operations ? "F/X Browse  Enter Open  Esc Back" :
                  mode == 3 ? "Esc Operations" : mode == 2 ? "F/X Browse  Enter Operations  Esc AP" :
                  mode == 1 ? "D Security  Esc Operations" :
                  vm.raw_session() ? "F/X Browse  Enter Open  B Build  Esc Feed" :
                  "F/X Browse  Enter Info  D Restart  Esc Back");
}
}
