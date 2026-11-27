// SPDX-License-Identifier: AGPL-3.0-only
#include "gps_tool_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "channel_survey.h"
#include "map_packs.h"
#include "map_projection.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <unordered_set>

namespace screen {
namespace {
lv_obj_t* label(lv_obj_t* parent, int x, int y, int width, const lv_font_t* font) {
    auto* item = lv_label_create(parent);
    lv_obj_set_pos(item, x, y);
    lv_obj_set_width(item, width);
    lv_label_set_long_mode(item, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(item, font ? font : LV_FONT_DEFAULT, 0);
    return item;
}
std::string coordinate(int32_t e7) {
    char text[24]{};
    std::snprintf(text, sizeof(text), "%.7f", static_cast<double>(e7) / 1e7);
    return text;
}
}
GpsToolScreen::GpsToolScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }
void GpsToolScreen::build_content(lv_obj_t* content) {
    auto* small = assets().load_standard_font(10);
    auto* medium = assets().load_standard_font(12);
    auto* large = assets().load_standard_font(16);
    const auto colors = view::palette(view_model().is_dark_mode());
    heading_ = label(content, 9, 3, 220, medium);
    state_ = label(content, 235, 4, 76, small);
    lv_obj_set_style_text_align(state_, LV_TEXT_ALIGN_RIGHT, 0);
    auto* card = summary_card_ = lv_obj_create(content);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 7, 22);
    lv_obj_set_size(card, 306, 70);
    lv_obj_set_style_radius(card, 5, 0);
    lv_obj_set_style_bg_color(card, colors.surface, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, colors.border, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    auto* stripe = lv_obj_create(card);
    lv_obj_remove_style_all(stripe);
    lv_obj_set_pos(stripe, 0, 0);
    lv_obj_set_size(stripe, 3, 70);
    lv_obj_set_style_bg_color(stripe, lv_color_hex(0xD1A5B8), 0);
    lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, 0);
    status_ = label(card, 12, 5, 275, large);
    position_ = label(card, 12, 29, 275, medium);
    detail_ = label(card, 12, 51, 275, small);
    const auto count_card = [&](int x, int width, lv_color_t accent, const char* title, lv_obj_t*& number) {
        auto* panel = lv_obj_create(content);
        lv_obj_remove_style_all(panel);
        lv_obj_set_pos(panel, x, 20);
        lv_obj_set_size(panel, width, 43);
        lv_obj_set_style_radius(panel, 5, 0);
        lv_obj_set_style_bg_color(panel, colors.surface, 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(panel, colors.border, 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        auto* stripe = lv_obj_create(panel);
        lv_obj_remove_style_all(stripe);
        lv_obj_set_size(stripe, 2, 43);
        lv_obj_set_style_bg_color(stripe, accent, 0);
        lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, 0);
        auto* name = label(panel, 9, 2, width - 16, small);
        lv_label_set_text(name, title);
        lv_obj_set_style_text_color(name, colors.text_disabled, 0);
        number = label(panel, 9, 12, width - 16, assets().load_standard_font(24));
        lv_obj_set_style_text_color(number, accent, 0);
        return panel;
    };
    wifi_card_ = count_card(7, 98, colors.success, "WI-FI APs", wifi_count_);
    bluetooth_card_ = count_card(110, 98, colors.info, "BLUETOOTH", bluetooth_count_);
    total_card_ = count_card(213, 100, colors.warning, "TOTAL", total_count_);
    wardrive_status_ = label(content, 9, 65, 302, small);
    for (std::size_t i = 0; i < feed_rows_.size(); ++i) {
        auto& row = feed_rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 7, 78 + static_cast<int>(i) * 15);
        lv_obj_set_size(row.box, 306, 15);
        lv_obj_set_style_bg_color(row.box, colors.surface, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row.box, colors.border, 0);
        lv_obj_set_style_border_side(row.box, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        lv_obj_clear_flag(row.box, LV_OBJ_FLAG_SCROLLABLE);
        row.type = label(row.box, 5, 1, 25, small);
        row.name = label(row.box, 35, 1, 150, small);
        row.address = label(row.box, 185, 2, 114, assets().load_standard_font(9));
        lv_obj_set_style_text_align(row.address, LV_TEXT_ALIGN_RIGHT, 0);
    }
    map_ = lv_obj_create(content);
    lv_obj_remove_style_all(map_);
    lv_obj_set_pos(map_, 5, 19);
    lv_obj_set_size(map_, 310, 104);
    lv_obj_clear_flag(map_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(map_, colors.surface, 0);
    lv_obj_set_style_bg_opa(map_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(map_, colors.border, 0);
    lv_obj_set_style_border_width(map_, 1, 0);
    lv_obj_set_style_radius(map_, 4, 0);
    for (auto& tile : map_tiles_) {
        tile = lv_image_create(map_);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_HIDDEN);
    }
    map_plot_ = lv_obj_create(map_);
    lv_obj_remove_style_all(map_plot_);
    lv_obj_set_size(map_plot_, 310, 104);
    lv_obj_clear_flag(map_plot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map_plot_, [](lv_event_t* event) {
        static_cast<GpsToolScreen*>(lv_event_get_user_data(event))->draw_wardrive_map(event);
    }, LV_EVENT_DRAW_MAIN, this);
    map_hint_ = label(map_, 35, 43, 240, medium);
    lv_label_set_long_mode(map_hint_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(map_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(map_hint_, colors.warning, 0);
    map_meta_ = label(map_, 4, 91, 210, assets().load_standard_font(9));
    map_credit_ = label(map_, 209, 93, 97, assets().load_standard_font(8));
    for (auto* item : {map_meta_, map_credit_}) {
        lv_obj_set_style_bg_color(item, colors.bar, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(item, colors.text, 0);
    }
    lv_obj_set_style_text_align(map_credit_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(map_credit_, "© OSM contributors");
    const auto map_count = [&](int x, lv_obj_t*& count, lv_color_t accent) {
        auto* panel = lv_obj_create(map_);
        lv_obj_remove_style_all(panel);
        lv_obj_set_pos(panel, x, 3);
        lv_obj_set_size(panel, 98, 21);
        lv_obj_set_style_radius(panel, 4, 0);
        lv_obj_set_style_bg_color(panel, colors.bar, 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(panel, accent, 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        count = label(panel, 4, 3, 90, small);
        lv_obj_set_style_text_color(count, accent, 0);
    };
    map_count(3, map_wifi_, colors.success);
    map_count(106, map_bluetooth_, colors.info);
    map_count(209, map_total_, colors.warning);
    message_ = label(content, 9, 96, 302, small);
    file_ = label(content, 9, 110, 302, small);
    reactive::observe_obj(content, view_model().gps_tool_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        static_cast<GpsToolScreen*>(lv_observer_get_user_data(observer))->render();
    }, this);
}
void GpsToolScreen::render() {
    auto& vm = view_model();
    const auto colors = view::palette(vm.is_dark_mode());
    const bool wardrive = vm.gps_tool() == viewmodel::BaseViewModel::GpsTool::Wardrive;
    const bool map_view = wardrive && vm.wardrive_map_view();
    const bool track = vm.gps_tool() == viewmodel::BaseViewModel::GpsTool::Track;
    const bool record = vm.gps_recording();
    if (wardrive) {
        lv_obj_add_flag(summary_card_, LV_OBJ_FLAG_HIDDEN);
        for (auto* obj : {wifi_card_, bluetooth_card_, total_card_, wardrive_status_})
            if (map_view) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        for (auto& row : feed_rows_)
            if (map_view) lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(file_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(summary_card_, LV_OBJ_FLAG_HIDDEN);
        for (auto* obj : {wifi_card_, bluetooth_card_, total_card_, wardrive_status_})
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        for (auto& row : feed_rows_) lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(message_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(file_, LV_OBJ_FLAG_HIDDEN);
    }
    if (map_view) lv_obj_remove_flag(map_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(map_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(heading_, wardrive ? (map_view ? "Wardrive / map" : "Wardrive / unique seen") :
                      track ? "GPS / Track" : "GPS / Receiver");
    lv_label_set_text(state_, record ? wardrive ?
        (vm.bluetooth_scanning() ? "REC Wi+BT" : "REC Wi") : "REC" :
        wardrive || track ? "IDLE" : "LIVE");
    const std::string fix = lv_subject_get_string(vm.gps_status_subject());
    const std::string top = vm.gps_receiver_id().empty() ? "No receiver selected" : "GPS  " + fix;
    lv_label_set_text(status_, top.c_str());
    int32_t lat{}, lon{};
    const bool positioned = vm.remote_position(lat, lon);
    const auto position = positioned ?
        vm.screen_coordinate(coordinate(lat)) + "  /  " + vm.screen_coordinate(coordinate(lon)) :
        "Position waiting for fix";
    lv_label_set_text(position_, position.c_str());
    std::string detail;
    if (wardrive) {
        lv_label_set_text_fmt(wifi_count_, "%zu", vm.wifi_items().size());
        lv_label_set_text_fmt(bluetooth_count_, "%zu", vm.bluetooth_observed_count());
        lv_label_set_text_fmt(total_count_, "%zu", vm.wifi_items().size() + vm.bluetooth_observed_count());
        const auto& message = vm.gps_tool_message();
        const bool alert = message.find("failed") != std::string::npos ||
                           message.find("unavailable") != std::string::npos ||
                           message.find("Enable") != std::string::npos;
        const auto meta = alert ? message :
            "CH " + (vm.scanning() ? std::to_string(platform::wireless::channel_number(vm.wifi_tuned_frequency())) : "--") +
            "   GPS " + fix + "   Saved " + std::to_string(vm.gps_record_count());
        lv_label_set_text(wardrive_status_, meta.c_str());
        lv_obj_set_style_text_color(wardrive_status_, alert || !positioned ? colors.warning : colors.text_disabled, 0);
        const auto& sightings = vm.wardrive_sightings();
        for (std::size_t i = 0; i < feed_rows_.size(); ++i) {
            auto& row = feed_rows_[i];
            if (i >= sightings.size()) {
                lv_label_set_text(row.type, i == 0 ? "--" : "");
                lv_label_set_text(row.name, i == 0 ? "Waiting for device sightings" : "");
                lv_label_set_text(row.address, "");
            } else {
                const auto& sighting = sightings[sightings.size() - 1 - i];
                lv_label_set_text(row.type, sighting.bluetooth ? "BT" : "AP");
                const auto name = vm.screen_name(sighting.name.empty() ? "[unnamed]" : sighting.name);
                lv_label_set_text(row.name, name.c_str());
                const auto address = vm.screen_mac(sighting.address);
                lv_label_set_text(row.address, address.c_str());
            }
            const bool bluetooth = i < sightings.size() && sightings[sightings.size() - 1 - i].bluetooth;
            lv_obj_set_style_text_color(row.type, i >= sightings.size() ? colors.text_disabled :
                                        bluetooth ? colors.info : colors.success, 0);
            lv_obj_set_style_text_color(row.name, colors.text, 0);
            lv_obj_set_style_text_color(row.address, colors.text_disabled, 0);
        }
        if (map_view) render_wardrive_map();
    } else if (track) detail = std::to_string(vm.gps_record_count()) + " positions saved";
    else detail = vm.gps_receiver_id().empty() ? "Device > Radios and GPS > Radio setup" :
                  vm.gps_receiver_name();
    lv_label_set_text(detail_, detail.c_str());
    lv_label_set_text(message_, vm.gps_tool_message().c_str());
    const auto filename = vm.gps_file_name().empty() ? "CSV files: ~/Documents/Marauder/gps" :
                          "File: " + vm.screen_capture_name(vm.gps_file_name());
    lv_label_set_text(file_, filename.c_str());
    lv_obj_set_style_text_color(heading_, colors.text, 0);
    lv_obj_set_style_text_color(state_, record ? colors.error : colors.text_disabled, 0);
    lv_obj_set_style_text_color(status_, positioned ? colors.success : colors.warning, 0);
    lv_obj_set_style_text_color(position_, colors.text, 0);
    lv_obj_set_style_text_color(detail_, colors.info, 0);
    lv_obj_set_style_text_color(message_, colors.text_disabled, 0);
    lv_obj_set_style_text_color(file_, colors.text_disabled, 0);
    set_key_guide(wardrive ? (map_view ? "M Feed  Enter Start/Stop  Esc Back" :
                              "M Map  Enter Start/Stop  Esc Back") :
                  track ? "Enter Start/Stop  Esc Save & Back  P PCAP" : "Esc Back  P PCAP");
}
void GpsToolScreen::render_wardrive_map() {
    auto& vm = view_model();
    int32_t lat{}, lon{};
    const bool live_fix = vm.remote_position(lat, lon);
    bool have_center = live_fix;
    if (!have_center && !vm.wardrive_route().empty()) {
        lat = vm.wardrive_route().back().latitude_e7;
        lon = vm.wardrive_route().back().longitude_e7;
        have_center = true;
    }
    if (!have_center && !vm.wardrive_map_points().empty()) {
        lat = vm.wardrive_map_points().back().latitude_e7;
        lon = vm.wardrive_map_points().back().longitude_e7;
        have_center = true;
    }
    map_zoom_ = vm.map_zoom();
    const auto center = have_center ? platform::maps::project(lat, lon, map_zoom_) : platform::maps::Pixel{};
    map_center_x_ = center.x;
    map_center_y_ = center.y;
    map_has_position_ = have_center && !vm.censor_enabled();
    const auto center_tile = have_center ? platform::maps::tile(vm.map_pack(), map_zoom_,
        static_cast<unsigned>(center.x / 256), static_cast<unsigned>(center.y / 256)) : std::filesystem::path{};
    const bool pack_ready = platform::maps::installed(vm.map_pack());
    const bool have_tiles = map_has_position_ && pack_ready && !center_tile.empty() &&
        std::filesystem::is_regular_file(center_tile);
    lv_label_set_text(map_hint_, vm.censor_enabled() ? "CENSOR ON / MAP HIDDEN" :
        !have_center ? "GPS FIX NEEDED" : !pack_ready ? "NO MAP PACK INSTALLED" :
        !have_tiles ? "NO TILE AT THIS LOCATION" : "");
    if (have_tiles) lv_obj_remove_flag(map_credit_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(map_credit_, LV_OBJ_FLAG_HIDDEN);
    for (std::size_t i = 0; i < map_tiles_.size(); ++i) {
        const int tx = static_cast<int>(std::floor((center.x - 155) / 256)) + static_cast<int>(i % 3);
        const int ty = static_cast<int>(std::floor((center.y - 52) / 256)) + static_cast<int>(i / 3);
        const auto path = have_tiles && tx >= 0 && ty >= 0 ?
            platform::maps::tile(vm.map_pack(), map_zoom_, static_cast<unsigned>(tx),
                                 static_cast<unsigned>(ty)) : std::filesystem::path{};
        if (path.empty() || !std::filesystem::is_regular_file(path)) {
            lv_obj_add_flag(map_tiles_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto source = "A:" + path.string();
        if (source != map_tile_paths_[i]) {
            map_tile_paths_[i] = source;
            lv_image_set_src(map_tiles_[i], map_tile_paths_[i].c_str());
        }
        lv_obj_set_pos(map_tiles_[i], static_cast<int>(std::lround(tx * 256 - center.x + 155)),
                                      static_cast<int>(std::lround(ty * 256 - center.y + 52)));
        lv_obj_remove_flag(map_tiles_[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text_fmt(map_wifi_, "WI-FI  %zu", vm.wifi_items().size());
    lv_label_set_text_fmt(map_bluetooth_, "BT  %zu", vm.bluetooth_observed_count());
    lv_label_set_text_fmt(map_total_, "TOTAL  %zu", vm.wifi_items().size() + vm.bluetooth_observed_count());
    const auto meta = !vm.gps_recording() ? vm.gps_tool_message() :
        !vm.bluetooth_scanning() ? vm.gps_tool_message() :
        std::string(live_fix ? "GPS FIX" : have_center ? "LAST FIX" : "NO GPS") +
        "  z" + std::to_string(map_zoom_) + "  " + std::to_string(vm.wardrive_map_points().size()) + " points";
    lv_label_set_text(map_meta_, meta.c_str());
    lv_obj_invalidate(map_plot_);
}
void GpsToolScreen::draw_wardrive_map(lv_event_t* event) {
    if (!map_has_position_) return;
    auto& vm = view_model();
    const auto colors = view::palette(vm.is_dark_mode());
    auto* layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(map_plot_, &area);
    const auto projected = [&](const viewmodel::BaseViewModel::WardriveMapPoint& item) {
        const auto pixel = platform::maps::project(item.latitude_e7, item.longitude_e7, map_zoom_);
        return lv_point_precise_t{
            static_cast<lv_value_precise_t>(area.x1 + 155 + platform::maps::wrapped_x(pixel.x, map_center_x_, map_zoom_)),
            static_cast<lv_value_precise_t>(area.y1 + 52 + pixel.y - map_center_y_)};
    };
    const auto visible = [&](const lv_point_precise_t& point) {
        return point.x >= area.x1 - 2 && point.x <= area.x2 + 2 &&
               point.y >= area.y1 - 2 && point.y <= area.y2 + 2;
    };
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(0xD08282);
    line.width = 2;
    const auto& route = vm.wardrive_route();
    for (std::size_t i = 1; i < route.size(); ++i) {
        line.p1 = projected(route[i - 1]);
        line.p2 = projected(route[i]);
        if (line.p1.x == line.p2.x && line.p1.y == line.p2.y) continue;
        if (visible(line.p1) || visible(line.p2)) lv_draw_line(layer, &line);
    }
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    dot.bg_opa = LV_OPA_TRANSP;
    dot.border_width = 2;
    dot.border_opa = LV_OPA_COVER;
    std::unordered_set<unsigned> drawn;
    for (const auto& item : vm.wardrive_map_points()) {
        const auto point = projected(item);
        if (!visible(point)) continue;
        const auto x = static_cast<int>(point.x) - area.x1;
        const auto y = static_cast<int>(point.y) - area.y1;
        const unsigned key = (item.bluetooth ? 1u << 18 : 0u) | (static_cast<unsigned>(x + 2) << 8) |
                             static_cast<unsigned>(y + 2);
        if (!drawn.insert(key).second) continue;
        dot.border_color = item.bluetooth ? colors.info : colors.success;
        const int radius = item.bluetooth ? 7 : 5;
        const lv_area_t marker{static_cast<int32_t>(point.x) - radius, static_cast<int32_t>(point.y) - radius,
                               static_cast<int32_t>(point.x) + radius, static_cast<int32_t>(point.y) + radius};
        lv_draw_rect(layer, &dot, &marker);
    }
    int32_t live_lat{}, live_lon{};
    if (vm.remote_position(live_lat, live_lon)) {
        dot.border_width = 0;
        dot.bg_opa = LV_OPA_COVER;
        dot.bg_color = colors.warning;
        const lv_area_t here{area.x1 + 152, area.y1 + 49, area.x1 + 158, area.y1 + 55};
        lv_draw_rect(layer, &dot, &here);
    }
}
}
