// SPDX-License-Identifier: AGPL-3.0-only
#include "bluetooth_capture_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include <algorithm>
#include <string>

namespace screen {
namespace {
lv_color_t accent(const platform::ble::Classification& item, const view::ThemePalette& colors) {
    using platform::ble::Kind;
    if (item.matches & platform::ble::bit(Kind::FindMy)) return colors.success;
    if (item.matches & platform::ble::bit(Kind::IBeacon)) return colors.info;
    if (item.matches & platform::ble::bit(Kind::Flipper)) return colors.primary;
    if (item.matches & platform::ble::bit(Kind::Flock)) return colors.warning;
    if (item.matches & platform::ble::bit(Kind::Meta)) return lv_color_hex(0xAD9CC9);
    if (item.matches & platform::ble::bit(Kind::SerialModule)) return colors.error;
    return colors.text_disabled;
}
}
BluetoothCaptureScreen::BluetoothCaptureScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { init(); }

void BluetoothCaptureScreen::build_content(lv_obj_t* content) {
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto label = [&](lv_obj_t* parent, int x, int y, int width, int size) {
        auto* object = lv_label_create(parent);
        lv_obj_set_pos(object, x, y);
        lv_obj_set_width(object, width);
        lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
        auto* font = assets().load_standard_font(size);
        lv_obj_set_style_text_font(object, font ? font : LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(object, colors.text, 0);
        return object;
    };
    title_ = label(content, 8, 1, 185, 14);
    counts_ = label(content, 189, 3, 123, 10);
    lv_obj_set_style_text_align(counts_, LV_TEXT_ALIGN_RIGHT, 0);
    status_ = label(content, 8, 19, 304, 10);
    detail_ = label(content, 8, 31, 304, 11);
    lv_label_set_long_mode(detail_, LV_LABEL_LONG_WRAP);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 6, 28 + static_cast<int>(i) * 23);
        lv_obj_set_size(row.box, 308, 22);
        lv_obj_set_style_radius(row.box, 4, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        lv_obj_clear_flag(row.box, LV_OBJ_FLAG_SCROLLABLE);
        row.name = label(row.box, 6, 0, 187, 11);
        row.meta = label(row.box, 195, 2, 105, 9);
        lv_obj_set_style_text_align(row.meta, LV_TEXT_ALIGN_RIGHT, 0);
        row.data = label(row.box, 6, 12, 294, 9);
    }
    fox_panel_ = std::make_unique<view::widgets::FoxHuntPanel>(content, assets());
    reactive::observe_obj(content, view_model().bluetooth_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        static_cast<BluetoothCaptureScreen*>(lv_observer_get_user_data(observer))->render();
    }, this);
}

void BluetoothCaptureScreen::render() {
    auto& vm = view_model();
    const auto colors = view::palette(vm.is_dark_mode());
    const auto view = vm.bluetooth_view();
    const bool feed = view == 0, filters = view == 1, devices = view == 2,
               actions = view == 3, info = view == 4, fox = view == 5;
    const auto visible = vm.bluetooth_visible_indices();
    const auto& records = vm.bluetooth_devices();
    const auto& sightings = vm.bluetooth_feed();
    const auto* selected = vm.bluetooth_selected();
    const std::string title = feed ? "Bluetooth / advertisements" : filters ? "Bluetooth / filters" :
        devices ? "Bluetooth / devices" : actions ? "Device operations" :
        info ? "Device information" : "Fox hunt";
    lv_label_set_text(title_, title.c_str());
    const std::string count = std::to_string(vm.bluetooth_total()) + " reports  " +
        std::to_string(visible.size()) + " devices";
    lv_label_set_text(counts_, count.c_str());
    lv_obj_set_style_text_color(counts_, colors.info, 0);
    const auto state = vm.bluetooth_running() ? "LIVE" : vm.bluetooth_starting() ? "STARTING" : "PAUSED";
    const auto filter = vm.bluetooth_filter() ? "FILTER" : "ALL";
    const std::string status = std::string(state) + "  " + vm.bluetooth_adapter_name() + "  " + filter + "  " +
        (vm.bluetooth_builder() ? "BUILD ON" : "BUILD OFF") + "  " + vm.bluetooth_status();
    lv_label_set_text(status_, status.c_str());
    lv_obj_set_style_text_color(status_, vm.bluetooth_running() ? colors.success : colors.warning, 0);
    fox_panel_->show(fox);
    if (fox && selected) {
        fox_panel_->render(vm.bluetooth_fox(), vm.screen_name(selected->observation.name.empty() ?
            selected->classification.type : selected->observation.name), selected->classification.vendor,
            accent(selected->classification, colors), vm.is_dark_mode(), true, vm.censor_enabled(), true);
    }
    if (info && selected) {
        const auto& observed = selected->observation;
        const auto& classified = selected->classification;
        const std::string detail = vm.screen_name(observed.name.empty() ? "Unnamed device" : observed.name) +
            "\n" + vm.screen_mac(observed.address) + "  " + observed.address_type +
            "\nType: " + classified.type + "  Vendor: " + classified.vendor +
            "\nRSSI " + (observed.rssi >= -127 && observed.rssi <= 0 ?
                           std::to_string(observed.rssi) + " dBm" : "unknown") + "   " +
            std::to_string(selected->sightings) + " sightings" +
            (classified.detail.empty() ? "" : vm.censor_enabled() ? "\nSignature details hidden" : "\n" + classified.detail) +
            (classified.evidence.empty() ? "" : "\n" + classified.evidence);
        lv_label_set_text(detail_, detail.c_str());
        lv_obj_remove_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    const std::size_t cursor = filters ? vm.bluetooth_filter_cursor() :
        actions ? vm.bluetooth_action_cursor() : vm.bluetooth_cursor();
    const std::size_t total = filters ? static_cast<std::size_t>(platform::ble::Kind::Count) + 1 :
        actions ? 2 : devices ? visible.size() :
        sightings.size() > vm.bluetooth_feed_offset() ? sightings.size() - vm.bluetooth_feed_offset() : 0;
    const auto first = feed ? (total > rows_.size() ? total - rows_.size() : 0) :
        cursor >= rows_.size() ? cursor - rows_.size() + 1 : 0;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = first + i;
        if (info || fox || index >= total) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        const auto* record = devices ? &records[visible[index]] : feed ? &sightings[index] : selected;
        const bool highlighted = !feed && index == cursor;
        const auto tint = record ? accent(record->classification, colors) : colors.info;
        lv_obj_set_style_bg_color(row.box, highlighted ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, highlighted ? colors.info : tint, 0);
        std::string name, meta;
        if (filters) {
            name = index ? platform::ble::label(static_cast<platform::ble::Kind>(index - 1)) : "All advertisements";
            meta = (index ? (vm.bluetooth_filter() & platform::ble::bit(static_cast<platform::ble::Kind>(index - 1))) != 0 :
                            vm.bluetooth_filter() == 0) ? "ON" : "OFF";
        } else if (actions) {
            name = index ? "Fox hunt" : "Information";
            meta = index ? "SIGNAL" : "VIEW";
        } else if (record) {
            name = vm.screen_name(record->observation.name.empty() ? record->classification.type : record->observation.name);
            meta = (record->observation.rssi >= -127 && record->observation.rssi <= 0 ?
                    std::to_string(record->observation.rssi) + " dBm" : "RSSI ?") + "  " +
                (feed ? record->classification.type : std::to_string(record->sightings) + " seen");
        }
        lv_label_set_text(row.name, name.c_str());
        lv_label_set_text(row.meta, meta.c_str());
        const std::string data = record && (feed || devices) ? vm.screen_mac(record->observation.address) +
            (feed ? vm.censor_enabled() ? "  [bytes hidden]" : "  " + record->observation.raw_ad :
             "  " + record->classification.type + " / " + record->classification.vendor) : "";
        lv_label_set_text(row.data, data.c_str());
        lv_obj_set_style_text_color(row.name, record ? tint : colors.text, 0);
        lv_obj_set_style_text_color(row.meta, colors.text_disabled, 0);
        lv_obj_set_style_text_color(row.data, colors.text_disabled, 0);
    }
    set_key_guide(feed ? "F/X Scroll  T Filter  B Build  L List  Enter Pause" :
        filters ? "F/X Choose  Enter Toggle  Esc Back" :
        devices ? "F/X Scroll  Enter Actions  Esc Back" :
        actions ? "F/X Choose  Enter Open  Esc Back" : "Esc Back");
}
} // namespace screen
