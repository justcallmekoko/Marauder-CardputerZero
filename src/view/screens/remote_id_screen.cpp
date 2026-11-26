// SPDX-License-Identifier: AGPL-3.0-only
#include "remote_id_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "channel_survey.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace screen {
namespace {
std::string mac_text(const uint8_t mac[6]) {
    char text[18];
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return text;
}
const char* aircraft_type(uint8_t value) {
    static constexpr const char* names[] = {"Unknown", "Aeroplane", "Helicopter", "Gyroplane",
        "Hybrid lift", "Ornithopter", "Glider", "Kite", "Balloon", "Captive balloon",
        "Airship", "Parachute", "Rocket", "Tethered", "Ground", "Other"};
    return value < 16 ? names[value] : "Unknown";
}
const char* flight_state(uint8_t value) {
    static constexpr const char* names[] = {"Undeclared", "Ground", "Airborne", "Emergency", "RID failure"};
    return value < 5 ? names[value] : "Unknown";
}
std::string position_text(int32_t e7) {
    char text[22];
    std::snprintf(text, sizeof(text), "%.6f", static_cast<double>(e7) / 1e7);
    return text;
}
}

RemoteIdScreen::RemoteIdScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }

void RemoteIdScreen::build_content(lv_obj_t* content) {
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto label = [&](lv_obj_t* parent, int x, int y, int width, int size) {
        auto* obj = lv_label_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_width(obj, width);
        auto* font = assets().load_standard_font(size);
        lv_obj_set_style_text_font(obj, font ? font : LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(obj, colors.text, 0);
        lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
        return obj;
    };
    title_ = label(content, 8, 1, 175, 14);
    count_ = label(content, 185, 3, 127, 10);
    lv_obj_set_style_text_align(count_, LV_TEXT_ALIGN_RIGHT, 0);
    map_ = lv_obj_create(content);
    lv_obj_remove_style_all(map_);
    lv_obj_set_pos(map_, 5, 18);
    lv_obj_set_size(map_, 104, 104);
    lv_obj_clear_flag(map_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(map_, colors.surface, 0);
    lv_obj_set_style_bg_opa(map_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(map_, 1, 0);
    lv_obj_set_style_border_color(map_, colors.border, 0);
    lv_obj_set_style_radius(map_, 4, 0);
    for (int diameter : {42, 82}) {
        auto* ring = lv_obj_create(map_);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, diameter, diameter);
        lv_obj_set_pos(ring, (104 - diameter) / 2, (104 - diameter) / 2);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, colors.border, 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }
    auto* north = label(map_, 48, 2, 12, 9);
    lv_label_set_text(north, "N");
    auto* you = lv_obj_create(map_);
    lv_obj_remove_style_all(you);
    lv_obj_set_size(you, 6, 6);
    lv_obj_set_pos(you, 49, 49);
    lv_obj_set_style_radius(you, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(you, colors.success, 0);
    lv_obj_set_style_bg_opa(you, LV_OPA_COVER, 0);
    range_ = label(map_, 4, 88, 92, 10);
    gps_hint_ = label(map_, 9, 37, 88, 10);
    lv_label_set_long_mode(gps_hint_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(gps_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(gps_hint_, colors.warning, 0);
    empty_ = label(content, 126, 56, 176, 11);
    lv_label_set_text(empty_, "Listening for aircraft...");
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        auto* path = paths_[i] = lv_line_create(map_);
        lv_obj_set_pos(path, 0, 0);
        lv_obj_set_style_line_color(path, colors.info, 0);
        lv_obj_set_style_line_width(path, 1, 0);
        lv_obj_add_flag(path, LV_OBJ_FLAG_HIDDEN);
        auto* marker = markers_[i] = lv_obj_create(map_);
        lv_obj_remove_style_all(marker);
        lv_obj_set_size(marker, 5, 5);
        lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
        lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
    }
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 114, 18 + static_cast<int>(i) * 26);
        lv_obj_set_size(row.box, 201, 24);
        lv_obj_set_style_radius(row.box, 4, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        lv_obj_clear_flag(row.box, LV_OBJ_FLAG_SCROLLABLE);
        row.name = label(row.box, 5, 1, 188, 12);
        row.meta = label(row.box, 5, 13, 188, 9);
    }
    detail_ = label(content, 8, 18, 304, 11);
    lv_label_set_long_mode(detail_, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    fox_panel_ = std::make_unique<view::widgets::FoxHuntPanel>(content, assets());
    reactive::observe_obj(content, view_model().inventory_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        static_cast<RemoteIdScreen*>(lv_observer_get_user_data(observer))->render();
    }, this);
}

void RemoteIdScreen::render() {
    auto& vm = view_model();
    const auto colors = view::palette(vm.is_dark_mode());
    const auto total = vm.remote_count();
    const auto cursor = vm.remote_cursor();
    const auto mode = vm.remote_view();
    const bool details = mode == 2 && cursor < total;
    const bool operations = mode == 1 && cursor < total;
    const bool fox = mode == 3 && cursor < total;
    int32_t origin_lat{}, origin_lon{};
    const bool have_gps = vm.remote_position(origin_lat, origin_lon);
    static constexpr const char* tabs[] = {"Aircraft / ID", "Aircraft / Position", "Aircraft / Operator"};
    lv_label_set_text(title_, fox ? "Fox hunt" : operations ? "Aircraft operations" :
                      details ? tabs[vm.remote_detail_tab()] : "Remote ID map");
    const auto count = fox ? std::to_string(vm.fox_hunt().frames()) + " frames" :
        std::to_string(total) + " seen  CH " +
        std::to_string(platform::wireless::channel_number(vm.wifi_tuned_frequency()));
    lv_label_set_text(count_, count.c_str());
    lv_obj_set_style_text_color(count_, colors.success, 0);
    fox_panel_->show(fox);
    if (fox) {
        lv_obj_add_flag(map_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
        for (auto& row : rows_) lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        fox_panel_->render(vm.fox_hunt(), vm.fox_label(), vm.fox_vendor(),
                           lv_color_hex(0xD1A8BE), vm.is_dark_mode(),
                           vm.wifi_tuned_frequency() == vm.fox_hunt().frequency_mhz());
        set_key_guide("Esc Operations  P PCAP");
        return;
    }
    if (operations) {
        lv_obj_add_flag(map_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            auto& row = rows_[i];
            if (i >= 2) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(row.box, 8, 22 + static_cast<int>(i) * 42);
            lv_obj_set_size(row.box, 304, 38);
            lv_obj_set_style_bg_color(row.box, i == vm.remote_operation_cursor() ? colors.button : colors.surface, 0);
            lv_obj_set_style_border_color(row.box, i == vm.remote_operation_cursor() ? colors.warning : colors.border, 0);
            lv_obj_set_width(row.name, 290);
            lv_obj_set_width(row.meta, 290);
            lv_obj_set_y(row.name, 5);
            lv_obj_set_y(row.meta, 21);
            lv_label_set_text(row.name, i ? "Fox hunt" : "Information");
            lv_label_set_text(row.meta, i ? "Live signal on the aircraft channel" : "Identity, position, operator");
            lv_obj_set_style_text_color(row.name, colors.text, 0);
            lv_obj_set_style_text_color(row.meta, colors.text_disabled, 0);
        }
        lv_obj_set_pos(empty_, 8, 106);
        lv_obj_set_width(empty_, 304);
        lv_label_set_text(empty_, vm.remote_message().c_str());
        if (vm.remote_message().empty()) lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        set_key_guide("F/X Browse  Enter Open  Esc Map");
        return;
    }
    lv_obj_set_pos(empty_, 126, 56);
    lv_obj_set_width(empty_, 176);
    lv_label_set_text(empty_, "Listening for aircraft...");
    lv_obj_set_pos(detail_, 8, 18);
    if (details) lv_obj_add_flag(map_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(map_, LV_OBJ_FLAG_HIDDEN);
    if (details || total) lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    if (details) {
        for (auto& row : rows_) lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(detail_, LV_OBJ_FLAG_HIDDEN);
        const auto* aircraft = vm.remote_record(cursor);
        std::string text;
        if (vm.remote_detail_tab() == 0) {
            text = std::string("UAS  ") + (aircraft->hasUasId ? aircraft->uasId : "Unknown") +
                "\nMAC  " + mac_text(aircraft->mac) +
                "\nType  " + aircraft_type(aircraft->uaType) +
                "\nState  " + flight_state(aircraft->operationalStatus) +
                "\nFrames  " + std::to_string(aircraft->packetCount) +
                "    RSSI  " + std::to_string(aircraft->rssi) + " dBm" +
                "\nWi-Fi CH  " + std::to_string(aircraft->lastWifiChannel);
        } else if (vm.remote_detail_tab() == 1) {
            text = std::string("Aircraft  ") + (aircraft->hasLocation ?
                position_text(aircraft->latitudeE7) + ", " + position_text(aircraft->longitudeE7) : "No position broadcast") +
                "\nGeo alt  " + (aircraft->altitudeGeoValid ? std::to_string(static_cast<int>(aircraft->altitudeGeoM)) + " m" : "Unknown") +
                "\nSpeed  " + (aircraft->horizontalSpeedValid ? std::to_string(static_cast<int>(aircraft->horizontalSpeedMps)) + " m/s" : "Unknown") +
                "\nHeading  " + (aircraft->directionValid ? std::to_string(aircraft->directionDeg) + " deg" : "Unknown");
            if (have_gps && aircraft->hasLocation) {
                char distance[20];
                remoteIdFormatDistanceKm(remoteIdDistanceMeters(origin_lat, origin_lon,
                    aircraft->latitudeE7, aircraft->longitudeE7), distance, sizeof(distance));
                text += "\nFrom you  " + std::string(distance) + "  " +
                    std::to_string(static_cast<int>(remoteIdBearingDegrees(origin_lat, origin_lon,
                        aircraft->latitudeE7, aircraft->longitudeE7))) + " deg";
            } else text += "\nFrom you  GPS fix needed";
        } else {
            text = std::string("Operator ID  ") + (aircraft->hasOperatorId ? aircraft->operatorId : "Not broadcast") +
                "\nOperator position  " + (aircraft->hasOperatorLocation ?
                    position_text(aircraft->operatorLatitudeE7) + ", " + position_text(aircraft->operatorLongitudeE7) : "Not broadcast") +
                "\nDescription  " + (aircraft->hasDescription ? aircraft->description : "Not broadcast") +
                "\nAuthentication  " + (aircraft->hasAuthentication ? "Observed" : "Not observed") +
                std::string("\nTransport  Wi-Fi ") +
                (aircraft->transportMask & static_cast<uint8_t>(RemoteIdTransport::WifiNan) ? "NAN" : "Beacon");
        }
        lv_label_set_text(detail_, text.c_str());
        set_key_guide("F/X Details  Esc Actions");
        return;
    }
    lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(gps_hint_, have_gps ? "" : "GPS FIX\nNEEDED");
    lv_label_set_text(range_, "");
    float map_radius = 50.0f;
    if (have_gps) {
        for (std::size_t i = 0; i < total; ++i) {
            const auto* aircraft = vm.remote_record(i);
            if (!aircraft) continue;
            map_radius = std::max(map_radius, remoteIdGridScaleMeters(aircraft, 1, origin_lat, origin_lon));
        }
        char scale[18];
        remoteIdFormatGridRadius(map_radius, scale, sizeof(scale));
        lv_label_set_text(range_, scale);
    }
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        const auto* aircraft = i < total ? vm.remote_record(i) : nullptr;
        if (!have_gps || !aircraft || !aircraft->hasLocation) {
            lv_obj_add_flag(markers_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(paths_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto point = remoteIdProjectToGrid(aircraft->latitudeE7, aircraft->longitudeE7,
            origin_lat, origin_lon, 104, 104, map_radius, 6);
        lv_obj_set_pos(markers_[i], point.x - 2, point.y - 2);
        lv_obj_set_style_bg_color(markers_[i], i == cursor ? colors.warning : colors.info, 0);
        lv_obj_remove_flag(markers_[i], LV_OBJ_FLAG_HIDDEN);
        auto& points = path_points_[i];
        std::size_t length{};
        for (std::size_t p = 0; p < aircraft->historyCount; ++p) {
            const auto older = remoteIdProjectHistoryToGrid(*aircraft, aircraft->history[p],
                origin_lat, origin_lon, 104, 104, map_radius, 6);
            points[length++] = {static_cast<lv_value_precise_t>(older.x), static_cast<lv_value_precise_t>(older.y)};
        }
        points[length++] = {static_cast<lv_value_precise_t>(point.x), static_cast<lv_value_precise_t>(point.y)};
        if (length > 1) {
            lv_line_set_points(paths_[i], points.data(), static_cast<uint32_t>(length));
            lv_obj_set_style_line_color(paths_[i], i == cursor ? colors.warning : colors.info, 0);
            lv_obj_set_style_line_width(paths_[i], i == cursor ? 2 : 1, 0);
            lv_obj_remove_flag(paths_[i], LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(paths_[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (cursor < first_) first_ = cursor;
    if (cursor >= first_ + rows_.size()) first_ = cursor + 1 - rows_.size();
    first_ = std::min(first_, total > rows_.size() ? total - rows_.size() : std::size_t{0});
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = first_ + i;
        if (index >= total) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(row.box, 114, 18 + static_cast<int>(i) * 26);
        lv_obj_set_size(row.box, 201, 24);
        lv_obj_set_width(row.name, 188);
        lv_obj_set_width(row.meta, 188);
        lv_obj_set_y(row.name, 1);
        lv_obj_set_y(row.meta, 13);
        const auto* aircraft = vm.remote_record(index);
        const bool selected = index == cursor;
        lv_obj_set_style_bg_color(row.box, selected ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, selected ? colors.warning : colors.border, 0);
        lv_label_set_text(row.name, aircraft->hasUasId ? aircraft->uasId : mac_text(aircraft->mac).c_str());
        lv_obj_set_style_text_color(row.name, aircraft->isLost ? colors.text_disabled : colors.text, 0);
        std::string meta = aircraft->isLost ? "LOST  " : "LIVE  ";
        meta += "CH " + std::to_string(aircraft->lastWifiChannel) + "  " +
            std::to_string(aircraft->rssi) + " dBm";
        if (have_gps && aircraft->hasLocation) {
            char distance[20];
            remoteIdFormatDistanceKm(remoteIdDistanceMeters(origin_lat, origin_lon,
                aircraft->latitudeE7, aircraft->longitudeE7), distance, sizeof(distance));
            meta += "  " + std::string(distance);
        }
        lv_label_set_text(row.meta, meta.c_str());
        lv_obj_set_style_text_color(row.meta, colors.text_disabled, 0);
    }
    set_key_guide("F/X Select  Enter Actions  Esc Back");
}
}
