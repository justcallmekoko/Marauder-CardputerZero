// SPDX-License-Identifier: AGPL-3.0-only
#include "radio_setup_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include <algorithm>
#include <vector>

namespace screen {
namespace {
lv_obj_t* label(lv_obj_t* parent, int x, int y, int width, int height, const lv_font_t* font) {
    auto* value = lv_label_create(parent);
    lv_obj_set_pos(value, x, y);
    lv_obj_set_size(value, width, height);
    lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(value, font, 0);
    lv_label_set_text(value, "");
    return value;
}
lv_color_t accent(const std::string& kind) {
    return lv_color_hex(kind == "WiFi" ? 0xA6C9AD : kind == "Bluetooth" ? 0x96BBDD : 0xD1A5B8);
}
}
RadioSetupScreen::RadioSetupScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }

void RadioSetupScreen::build_content(lv_obj_t* content) {
    const lv_font_t* small = assets().load_standard_font(10);
    const lv_font_t* medium = assets().load_standard_font(13);
    if (!small) small = &lv_font_montserrat_10;
    if (!medium) medium = &lv_font_montserrat_14;
    heading_ = label(content, 7, 2, 153, 17, medium);
    count_ = label(content, 159, 4, 154, 14, small);
    lv_obj_set_style_text_align(count_, LV_TEXT_ALIGN_RIGHT, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(row.box, 5, 22 + i * 25);
        lv_obj_set_size(row.box, 149, 23);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row.box, 4, 0);
        row.icon = label(row.box, 6, 3, 18, 17, &lv_font_montserrat_14);
        row.name = label(row.box, 28, 3, 75, 17, medium);
        row.state = label(row.box, 99, 5, 43, 14, small);
        lv_obj_set_style_text_align(row.state, LV_TEXT_ALIGN_RIGHT, 0);
    }
    type_ = label(content, 164, 23, 150, 16, small);
    product_ = label(content, 164, 41, 150, 29, medium);
    location_ = label(content, 164, 72, 150, 14, small);
    driver_ = label(content, 164, 88, 150, 14, small);
    detail_ = label(content, 164, 103, 150, 25, small);
    reactive::observe_obj(content, view_model().gps_status_subject(), changed, this);
    reactive::observe_obj(content, view_model().marauder_menu_generation_subject(), changed, this);
}
void RadioSetupScreen::changed(lv_observer_t* observer, lv_subject_t*) {
    static_cast<RadioSetupScreen*>(lv_observer_get_user_data(observer))->render();
}
void RadioSetupScreen::render() {
    auto& vm = view_model();
    const auto& devices = vm.radios();
    const bool picker = vm.current_page() == model::AppPage::BluetoothAdapters;
    const auto indices = picker ? vm.bluetooth_adapter_indices() : std::vector<std::size_t>{};
    const auto count = picker ? indices.size() : devices.size();
    auto colors = view::palette(vm.is_dark_mode());
    auto put = [](lv_obj_t* obj, const std::string& text, lv_color_t color) {
        lv_label_set_text(obj, text.c_str());
        lv_obj_set_style_text_color(obj, color, 0);
    };
    put(heading_, picker ? "Bluetooth adapter" : "Radio setup", colors.text);
    const auto cursor = picker ? vm.bluetooth_adapter_cursor() : vm.radio_cursor();
    put(count_, vm.radio_message() == "Ready" ? std::to_string(count) + (picker ? " adapters" : " devices") :
        vm.radio_message(), colors.text_disabled);
    const auto first = cursor < rows_.size() ? 0 : cursor + 1 - rows_.size();
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = first + i;
        if (index >= count) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        const auto& d = devices[picker ? indices[index] : index];
        const auto tint = accent(d.kind);
        lv_obj_set_style_bg_color(row.box, index == cursor ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        lv_obj_set_style_border_color(row.box, index == cursor ? tint : colors.border, 0);
        put(row.icon, d.kind == "WiFi" ? LV_SYMBOL_WIFI : d.kind == "Bluetooth" ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_GPS, tint);
        put(row.name, d.name, colors.text);
        const bool selected = picker && d.id == vm.selected_bluetooth_id();
        put(row.state, selected ? "PICKED" : d.state, selected || d.state == "On" ? tint : colors.text_disabled);
    }
    if (!count) {
        put(type_, vm.radio_busy() ? "Finding devices..." : picker ? "No Bluetooth adapters" : "No devices available", colors.text);
        for (auto* field : {product_, location_, driver_, detail_}) lv_label_set_text(field, "");
        set_key_guide("D Refresh    Esc Back");
        return;
    }
    const auto& d = devices[picker ? indices[std::min(cursor, count - 1)] : std::min(cursor, count - 1)];
    const bool gps = d.kind == "GPS";
    const auto band_label = d.bands.empty() ? "Bands unknown" : d.bands;
    put(type_, picker ? d.id == vm.selected_bluetooth_id() ? "Bluetooth / Selected" : "Bluetooth / Available" :
        d.kind == "WiFi" ? band_label : gps && d.state == "On" ? "GPS / Selected" : d.kind, accent(d.kind));
    lv_obj_set_style_bg_color(type_, accent(d.kind), 0);
    lv_obj_set_style_bg_opa(type_, d.kind == "WiFi" ? LV_OPA_20 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(type_, d.kind == "WiFi" ? 4 : 0, 0);
    put(product_, d.product, colors.text);
    put(location_, d.location, colors.text);
    const std::string receiver_state = lv_subject_get_string(vm.gps_status_subject());
    const bool verified = gps && d.state == "On" && (receiver_state == "No fix" ||
        receiver_state == "Fix" || receiver_state.find(" sat") != std::string::npos);
    put(driver_, verified ? "GPS receiver" : d.driver, colors.text_disabled);
    put(detail_, picker ? d.state == "On" ? "Ready for collection" : "Enable in Radio setup" :
        vm.screen_text(d.detail), colors.text_disabled);
    const char* action = d.state == "On" ? "Disable" :
                         d.state == "Off" ? "Enable" : "";
    const std::string guide = picker ? std::string("F/X Browse   ") +
        (d.state == "On" && !vm.radio_busy() ? "Enter Select   " : "") + "D Refresh   Esc Back" : std::string("F/X Browse   ") +
        (!vm.radio_busy() && *action ? std::string("Enter ") + action + "   " : "") + "D Refresh   Esc Back";
    set_key_guide(guide.c_str());
}
}
