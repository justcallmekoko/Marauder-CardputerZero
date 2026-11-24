/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "adapters_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {

AdaptersScreen::AdaptersScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void AdaptersScreen::build_content(lv_obj_t* content) {
    auto* panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_hor(panel, 8, 0);
    lv_obj_set_style_pad_ver(panel, 3, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    auto* details = lv_label_create(panel);
    lv_obj_set_width(details, LV_PCT(100));
    lv_label_set_long_mode(details, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(details, view_model().adapter_detail_subject(), nullptr);
    auto* body_font = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(details, body_font ? body_font : &lv_font_montserrat_10, 0);
    reactive::bind_theme(details, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
