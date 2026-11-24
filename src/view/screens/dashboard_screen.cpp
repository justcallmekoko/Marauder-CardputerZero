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
constexpr uint32_t kMarauderBlack = 0x000000;
constexpr uint32_t kMarauderWhite = 0xffffff;
constexpr uint32_t kMarauderDarkGray = 0x303030;
constexpr uint32_t kMarauderLightGray = 0xd6d3d6;

lv_color_t menu_color(model::MarauderMenuColor color) {
    switch (color) {
        case model::MarauderMenuColor::White: return lv_color_hex(0xffffff);
        case model::MarauderMenuColor::Cyan: return lv_color_hex(0x00ffff);
        case model::MarauderMenuColor::Blue: return lv_color_hex(0x0000ff);
        case model::MarauderMenuColor::Red: return lv_color_hex(0xff0000);
        case model::MarauderMenuColor::Green: return lv_color_hex(0x00ff00);
        case model::MarauderMenuColor::LightGrey: return lv_color_hex(kMarauderLightGray);
        case model::MarauderMenuColor::Magenta: return lv_color_hex(0xff00ff);
        case model::MarauderMenuColor::Purple: return lv_color_hex(0x7b007b);
        case model::MarauderMenuColor::Orange: return lv_color_hex(0xffb600);
        case model::MarauderMenuColor::Yellow: return lv_color_hex(0xffff00);
    }
    return lv_color_hex(kMarauderWhite);
}

bool needs_light_label(model::MarauderMenuColor color) {
    return color == model::MarauderMenuColor::Blue ||
           color == model::MarauderMenuColor::Purple;
}
}

DashboardScreen::DashboardScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

DashboardScreen::~DashboardScreen() {
    if (menu_observer_) lv_observer_remove(menu_observer_);
}

void DashboardScreen::build_content(lv_obj_t* content) {
    panel_ = lv_obj_create(content);
    lv_obj_remove_style_all(panel_);
    lv_obj_set_size(panel_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_hor(panel_, 8, 0);
    lv_obj_set_style_pad_ver(panel_, 2, 0);
    lv_obj_set_style_pad_row(panel_, 1, 0);
    lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);

    menu_title_ = lv_label_create(panel_);
    lv_obj_set_size(menu_title_, LV_PCT(100), 15);
    auto* title_font = assets().load_standard_font(11);
    lv_obj_set_style_text_font(menu_title_, title_font ? title_font : &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(menu_title_, 1, 0);

    auto* row_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        rows_[index] = lv_obj_create(panel_);
        lv_obj_remove_style_all(rows_[index]);
        lv_obj_set_size(rows_[index], LV_PCT(100), 14);
        lv_obj_set_style_radius(rows_[index], 1, 0);
        lv_obj_set_style_pad_hor(rows_[index], 4, 0);
        lv_obj_clear_flag(rows_[index], LV_OBJ_FLAG_SCROLLABLE);

        row_labels_[index] = lv_label_create(rows_[index]);
        lv_obj_set_width(row_labels_[index], LV_PCT(100));
        lv_label_set_long_mode(row_labels_[index], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(row_labels_[index], row_font ? row_font : &lv_font_montserrat_10, 0);
        lv_obj_align(row_labels_[index], LV_ALIGN_LEFT_MID, 0, 0);
    }

    footer_ = lv_label_create(panel_);
    lv_obj_set_size(footer_, LV_PCT(100), 11);
    lv_label_set_long_mode(footer_, LV_LABEL_LONG_CLIP);
    auto* footer_font = assets().load_standard_font(9, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(footer_, footer_font ? footer_font : &lv_font_montserrat_10, 0);

    menu_observer_ = reactive::observe_obj(panel_,
                                           view_model().marauder_menu_generation_subject(),
                                           menu_changed_cb,
                                           this);
    render_menu();
}

void DashboardScreen::render_menu() {
    if (!panel_) return;
    const auto background = lv_color_hex(kMarauderBlack);
    const auto muted_text = lv_color_hex(kMarauderLightGray);

    lv_obj_set_style_bg_color(panel_, background, 0);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
    lv_label_set_text(menu_title_, view_model().marauder_menu_title());
    lv_obj_set_style_text_color(menu_title_, muted_text, 0);

    const auto count = view_model().marauder_menu_item_count();
    const auto selected = view_model().marauder_menu_selected_index();
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        if (index >= count) {
            lv_obj_add_flag(rows_[index], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(rows_[index], LV_OBJ_FLAG_HIDDEN);
        const auto& item = view_model().marauder_menu_item(index);
        const auto item_color = menu_color(item.color);
        std::string label = index == selected ? "> " : "  ";
        label += item.label;
        if (item.opens_submenu) label += "  >";
        if (!item.available) label += "  [soon]";
        lv_label_set_text(row_labels_[index], label.c_str());

        const bool active = index == selected;
        lv_obj_set_style_bg_opa(rows_[index], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(rows_[index], item.available ? item_color : lv_color_hex(kMarauderDarkGray), 0);
        const bool light_label = item.available && needs_light_label(item.color);
        lv_obj_set_style_border_width(rows_[index], !active && light_label ? 2 : 0, 0);
        lv_obj_set_style_border_side(rows_[index], LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_color(rows_[index], item_color, 0);
        lv_obj_set_style_text_color(row_labels_[index],
                                    active && item.available && !light_label ? lv_color_hex(kMarauderBlack) :
                                    light_label ? lv_color_hex(kMarauderWhite) :
                                    item.available ? item_color : muted_text,
                                    0);
    }

    const auto& selected_item = view_model().marauder_menu_item(selected);
    if (!selected_item.available) {
        lv_label_set_text(footer_, "COMING SOON   A / LEFT / ESC  BACK");
    } else if (view_model().marauder_menu_at_root()) {
        lv_label_set_text(footer_, "W/S MOVE   ENTER/D SELECT   HOLD ESC EXIT");
    } else {
        lv_label_set_text(footer_, "W/S MOVE   ENTER/D SELECT   A BACK");
    }
    lv_obj_set_style_text_color(footer_, muted_text, 0);
}

void DashboardScreen::menu_changed_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* screen = static_cast<DashboardScreen*>(lv_observer_get_user_data(observer));
    if (screen) screen->render_menu();
}

} // namespace screen
