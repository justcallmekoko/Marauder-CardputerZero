/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "adapters_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <string>

namespace screen {

AdaptersScreen::AdaptersScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void AdaptersScreen::build_content(lv_obj_t* content) {
    auto* small = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    auto* medium = assets().load_standard_font(13);
    heading_ = lv_label_create(content);
    lv_obj_set_pos(heading_, 7, 3);
    lv_obj_set_style_text_font(heading_, small ? small : &lv_font_montserrat_10, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto* row = rows_[i] = lv_obj_create(content);
        lv_obj_remove_style_all(row);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(row, 6, 23 + 32 * i);
        lv_obj_set_size(row, 114, 28);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        labels_[i] = lv_label_create(row);
        lv_obj_set_width(labels_[i], 99);
        lv_label_set_long_mode(labels_[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(labels_[i], medium ? medium : &lv_font_montserrat_14, 0);
        lv_obj_align(labels_[i], LV_ALIGN_LEFT_MID, 6, 0);
    }
    info_ = lv_label_create(content);
    lv_obj_set_pos(info_, 130, 22);
    lv_obj_set_size(info_, 183, 99);
    lv_label_set_long_mode(info_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(info_, small ? small : &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_line_space(info_, 2, 0);
    bands_ = lv_label_create(content);
    lv_obj_set_pos(bands_, 130, 103);
    lv_obj_set_style_pad_hor(bands_, 4, 0);
    lv_obj_set_style_text_font(bands_, small ? small : &lv_font_montserrat_10, 0);
    expanded_ = lv_label_create(content);
    lv_obj_set_pos(expanded_, 7, 2);
    lv_obj_set_width(expanded_, 306);
    lv_obj_set_style_text_font(expanded_, small ? small : &lv_font_montserrat_10, 0);
    lv_label_bind_text(expanded_, view_model().adapter_detail_subject(), nullptr);
    reactive::observe_obj(content, view_model().adapter_detail_subject(), changed, this);
    reactive::observe_obj(content, view_model().dark_mode_subject(), changed, this);
}

void AdaptersScreen::changed(lv_observer_t* observer, lv_subject_t*) {
    static_cast<AdaptersScreen*>(lv_observer_get_user_data(observer))->render();
}

void AdaptersScreen::render() {
    auto& vm = view_model();
    const auto colors = view::palette(vm.is_dark_mode());
    const auto& radios = vm.adapters();
    const auto cursor = vm.adapter_cursor();
    const bool summary = !radios.empty() && !vm.adapter_details_open();
    const auto first = cursor - cursor % rows_.size();
    auto visible = [](lv_obj_t* obj, bool show) {
        if (show) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };
    set_key_guide(radios.empty() ? "Z/Esc  Back" : summary ?
        "F/X  Browse    Enter  Select    D  Details    Esc  Back" :
        "F/X  Browse    Enter  Select    D  Summary    Esc  Back");
    visible(expanded_, !summary);
    visible(heading_, summary);
    visible(info_, summary);
    visible(bands_, summary);
    lv_obj_set_style_text_color(expanded_, colors.text, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const auto index = first + i;
        visible(rows_[i], summary && index < radios.size());
        if (!summary || index >= radios.size()) continue;
        const auto& radio = radios[index];
        const bool focus = index == cursor;
        const bool active = radio.stable_id == vm.selected_radio_id();
        const auto name = (active ? "* " : "") + radio.interface_name;
        lv_label_set_text(labels_[i], name.c_str());
        lv_obj_set_style_text_color(labels_[i], colors.text, 0);
        lv_obj_set_style_bg_color(rows_[i], colors.button, 0);
        lv_obj_set_style_border_width(rows_[i], focus ? 2 : 1, 0);
        lv_obj_set_style_border_color(rows_[i], focus ? colors.info : colors.border, 0);
    }
    if (!summary) return;
    const auto& radio = radios[cursor];
    const auto title = "Radio adapters    " + std::to_string(cursor + 1) + " / " + std::to_string(radios.size());
    lv_label_set_text(heading_, title.c_str());
    lv_obj_set_style_text_color(heading_, colors.text_disabled, 0);
    const bool active = radio.stable_id == vm.selected_radio_id();
    const std::string description = std::string(active ? "SELECTED" : "AVAILABLE") + "\n" +
        radio.display_name() + "\n" +
        radio.bus_type + "  " + radio.operstate + "\n" + radio.driver + "  " + radio.phy_name +
        "\n" + radio.mac_address;
    lv_label_set_text(bands_, radio.bands.empty() ? "Bands unknown" : radio.bands.c_str());
    lv_obj_set_style_bg_color(bands_, colors.success, 0);
    lv_obj_set_style_bg_opa(bands_, LV_OPA_20, 0);
    lv_obj_set_style_text_color(bands_, colors.success, 0);
    lv_label_set_text(info_, description.c_str());
    lv_obj_set_style_text_color(info_, colors.text, 0);
}
} // namespace screen
