// SPDX-License-Identifier: AGPL-3.0-only
#include "gps_tool_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "channel_survey.h"
#include <cstdio>

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
    auto* card = lv_obj_create(content);
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
    lv_label_set_text(heading_, wardrive ? "GPS / Wardrive" : track ? "GPS / Track" : "GPS / Receiver");
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
        detail = "AP " + std::to_string(vm.scanning() ? vm.wifi_items().size() : 0) + "  BT " +
                 std::to_string(vm.bluetooth_record_count()) + "  Saved " +
                 std::to_string(vm.gps_record_count()) + "  CH " +
                 (vm.scanning() ? std::to_string(platform::wireless::channel_number(vm.wifi_tuned_frequency())) : "--");
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
