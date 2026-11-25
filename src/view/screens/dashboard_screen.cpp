/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "dashboard_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <string>

namespace screen {
namespace {
lv_color_t accent(model::MarauderMenuColor color) {
    using Color = model::MarauderMenuColor;
    switch (color) {
        case Color::Green: return lv_color_hex(0xa6cead);
        case Color::Cyan: return lv_color_hex(0x94c8c9);
        case Color::Blue: return lv_color_hex(0xa1b7dc);
        case Color::Red: return lv_color_hex(0xd99d9e);
        case Color::Magenta: return lv_color_hex(0xcca0bd);
        case Color::Purple: return lv_color_hex(0xbca8d2);
        case Color::Orange: case Color::Yellow: return lv_color_hex(0xd3ba96);
        default: return lv_color_hex(0xb9c1c8);
    }
}
constexpr const char* root_marks[] = {
    LV_SYMBOL_WIFI, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_GPS,
    LV_SYMBOL_DRIVE, LV_SYMBOL_SETTINGS, LV_SYMBOL_USB
};
}

DashboardScreen::DashboardScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) { init(); }

DashboardScreen::~DashboardScreen() {
    if (menu_observer_) lv_observer_remove(menu_observer_);
}

void DashboardScreen::build_content(lv_obj_t* content) {
    panel_ = content;
    auto* small = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    auto* title = assets().load_standard_font(13);
    menu_title_ = lv_label_create(content);
    lv_obj_set_pos(menu_title_, 7, 3);
    lv_obj_set_style_text_font(menu_title_, small ? small : &lv_font_montserrat_10, 0);
    footer_ = lv_label_create(content);
    lv_obj_align(footer_, LV_ALIGN_TOP_RIGHT, -7, 3);
    lv_obj_set_style_text_font(footer_, small ? small : &lv_font_montserrat_10, 0);

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto* tile = rows_[i] = lv_obj_create(content);
        lv_obj_remove_style_all(tile);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tile, 5, 0);
        marks_[i] = lv_label_create(tile);
        lv_obj_set_style_text_font(marks_[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(marks_[i], 6, 3);
        row_labels_[i] = lv_label_create(tile);
        lv_label_set_long_mode(row_labels_[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(row_labels_[i], title ? title : &lv_font_montserrat_14, 0);
        lv_obj_align(row_labels_[i], LV_ALIGN_BOTTOM_LEFT, 6, -3);
    }
    menu_observer_ = reactive::observe_obj(content, view_model().marauder_menu_generation_subject(), menu_changed_cb, this);
    reactive::observe_obj(content, view_model().dark_mode_subject(), menu_changed_cb, this);
}

void DashboardScreen::render_menu() {
    const auto colors = view::palette(view_model().is_dark_mode());
    const bool root = view_model().marauder_menu_at_root();
    const int columns = static_cast<int>(view_model().menu_columns());
    const int width = (308 - (columns - 1) * 4) / columns;
    lv_label_set_text(menu_title_, root ? "Launcher" : view_model().marauder_menu_title());
    const auto position = std::to_string(view_model().marauder_menu_selected_index() + 1) + " / " +
                          std::to_string(view_model().marauder_menu_item_count());
    const char* message = view_model().settings_message();
    lv_label_set_text(footer_, *message ? message : position.c_str());
    set_key_guide(root ? "F/X Up/Down  Z/C Left/Right  Enter Open  Esc Exit" :
                         "F/X Up/Down  Z/C Left/Right  Enter Open  Esc Back");
    lv_obj_set_style_text_color(menu_title_, colors.text, 0);
    lv_obj_set_style_text_color(footer_, colors.text_disabled, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto* tile = rows_[i];
        if (i >= view_model().marauder_menu_item_count()) {
            lv_obj_add_flag(tile, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_HIDDEN);
        const auto& item = view_model().marauder_menu_item(i);
        const bool focused = i == view_model().marauder_menu_selected_index();
        auto ink = accent(item.color);
        if (!view_model().is_dark_mode()) ink = lv_color_mix(colors.text, ink, 128);
        lv_obj_set_pos(tile, 6 + (i % columns) * (width + 4), 20 + (i / columns) * 51);
        lv_obj_set_size(tile, width, 47);
        lv_obj_set_style_bg_color(tile, focused ? lv_color_mix(ink, colors.button, 32) : colors.button, 0);
        lv_obj_set_style_border_width(tile, focused ? 2 : 1, 0);
        lv_obj_set_style_border_color(tile, focused ? ink : colors.border, 0);
        lv_label_set_text(marks_[i], root ? root_marks[i] :
                          !item.available ? LV_SYMBOL_MINUS : item.opens_submenu ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(marks_[i], item.available ? ink : colors.text_disabled, 0);
        std::string label(item.label);
        if (item.action == model::MarauderMenuAction::CycleBatteryDisplay)
            label += std::string(": ") + model::battery_display_name(view_model().battery_display());
        const auto value = view_model().setting_value(item.action);
        if (!value.empty()) {
            lv_label_set_text(marks_[i], value.c_str());
            lv_obj_set_style_text_font(marks_[i], &lv_font_montserrat_10, 0);
        } else {
            lv_obj_set_style_text_font(marks_[i], &lv_font_montserrat_14, 0);
        }
        if (!item.available) label += "  (soon)";
        lv_label_set_text(row_labels_[i], label.c_str());
        lv_obj_set_width(row_labels_[i], width - 16);
        lv_obj_set_style_text_color(row_labels_[i], item.available ? colors.text : colors.text_disabled, 0);
    }
}

void DashboardScreen::menu_changed_cb(lv_observer_t* observer, lv_subject_t*) {
    static_cast<DashboardScreen*>(lv_observer_get_user_data(observer))->render_menu();
}
} // namespace screen
