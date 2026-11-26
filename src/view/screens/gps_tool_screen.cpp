// SPDX-License-Identifier: AGPL-3.0-only
#include "gps_tool_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "channel_survey.h"
#include <cstdio>
#include <initializer_list>

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
    const bool track = vm.gps_tool() == viewmodel::BaseViewModel::GpsTool::Track;
    const bool record = vm.gps_recording();
    if (wardrive) {
        lv_obj_add_flag(summary_card_, LV_OBJ_FLAG_HIDDEN);
        for (auto* obj : {wifi_card_, bluetooth_card_, total_card_, wardrive_status_})
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        for (auto& row : feed_rows_) lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
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
    lv_label_set_text(heading_, wardrive ? "Wardrive / unique seen" : track ? "GPS / Track" : "GPS / Receiver");
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
    set_key_guide(wardrive || track ? "Enter Start/Stop  Esc Save & Back  P PCAP" : "Esc Back  P PCAP");
}
}
