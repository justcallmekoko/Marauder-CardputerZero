// SPDX-License-Identifier: AGPL-3.0-only
#include "map_packs_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "map_packs.h"
#include <cstdio>
#include <algorithm>
#include <limits>

namespace screen {
namespace {
std::string size_text(std::uint64_t bytes) {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%.1f %s", bytes >= 1073741824ULL ? bytes / 1073741824.0 : bytes / 1048576.0,
                  bytes >= 1073741824ULL ? "GiB" : "MiB");
    return buffer;
}
}
MapPacksScreen::MapPacksScreen(viewmodel::BaseViewModel& vm, app::AssetManager& assets)
    : BaseScreen(vm, assets) { sizes_.fill(std::numeric_limits<std::uint64_t>::max()); init(); }
void MapPacksScreen::build_content(lv_obj_t* content) {
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
    title_ = label(content, 8, 1, 128, 13);
    index_ = label(content, 139, 3, 52, 10);
    space_ = label(content, 196, 3, 116, 10);
    lv_obj_set_style_text_align(space_, LV_TEXT_ALIGN_RIGHT, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 6, 18 + static_cast<int>(i) * 20);
        lv_obj_set_size(row.box, 308, 19);
        lv_obj_set_style_radius(row.box, 4, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        row.name = label(row.box, 8, 0, 290, 10);
        row.meta = label(row.box, 8, 10, 290, 8);
    }
    progress_title_ = label(content, 8, 29, 304, 13);
    progress_bar_ = lv_bar_create(content);
    lv_obj_set_pos(progress_bar_, 8, 55);
    lv_obj_set_size(progress_bar_, 304, 10);
    lv_bar_set_range(progress_bar_, 0, 100);
    progress_detail_ = label(content, 8, 74, 304, 11);
    lv_obj_add_flag(progress_title_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(progress_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(progress_detail_, LV_OBJ_FLAG_HIDDEN);
    zoom_ = label(content, 8, 99, 304, 11);
    message_ = label(content, 8, 112, 304, 10);
    reactive::observe_obj(content, view_model().map_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        static_cast<MapPacksScreen*>(lv_observer_get_user_data(observer))->render();
    }, this);
}
void MapPacksScreen::render() {
    const auto colors = view::palette(view_model().is_dark_mode());
    if (storage_revision_ != view_model().map_storage_revision()) {
        storage_revision_ = view_model().map_storage_revision();
        sizes_.fill(std::numeric_limits<std::uint64_t>::max());
    }
    lv_label_set_text(title_, "OFFLINE MAPS");
    const auto index = std::to_string(view_model().map_pack_cursor() + 1) + "/" +
        std::to_string(platform::maps::pack_count());
    lv_label_set_text(index_, index.c_str());
    lv_obj_set_style_text_color(index_, colors.info, 0);
    const auto free = "Free " + size_text(platform::maps::free_bytes());
    lv_label_set_text(space_, free.c_str());
    lv_obj_set_style_text_color(space_, colors.info, 0);
    const bool busy = view_model().map_installing();
    for (auto& row : rows_) {
        if (busy) lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto* item : {progress_title_, progress_bar_, progress_detail_}) {
        if (busy) lv_obj_remove_flag(item, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
    }
    if (busy) {
        const auto& selected = platform::maps::pack(view_model().map_pack_cursor());
        const auto title = std::string(view_model().map_deleting() ? "Deleting  " : "Downloading  ") + selected.name;
        lv_label_set_text(progress_title_, title.c_str());
        lv_obj_set_style_text_color(progress_title_, colors.info, 0);
        const auto done = view_model().map_progress_done();
        const auto total = view_model().map_progress_total();
        const int percent = total ? static_cast<int>(std::min(100.0, 100.0 * done / total)) : 0;
        lv_bar_set_value(progress_bar_, percent, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(progress_bar_, colors.border, LV_PART_MAIN);
        lv_obj_set_style_bg_color(progress_bar_, colors.info, LV_PART_INDICATOR);
        const auto detail = view_model().map_deleting() ? std::string("Removing saved map files...") :
            view_model().map_progress_bytes() ? size_text(done) + " / " + size_text(total) + "  " + std::to_string(percent) + "%" :
            std::to_string(done) + " / " + std::to_string(total) + " tiles  " + std::to_string(percent) + "%";
        lv_label_set_text(progress_detail_, detail.c_str());
        lv_obj_set_style_text_color(progress_detail_, colors.text, 0);
        const auto zoom = "Selected zoom  z" + std::to_string(view_model().map_zoom());
        lv_label_set_text(zoom_, zoom.c_str());
        lv_label_set_text(message_, view_model().map_message().c_str());
        set_key_guide(view_model().map_deleting() ? "Please wait" : "Esc Pause download");
        return;
    }
    const unsigned first = std::min(view_model().map_pack_cursor() > 1 ? view_model().map_pack_cursor() - 2 : 0u,
        platform::maps::pack_count() - static_cast<unsigned>(rows_.size()));
    for (unsigned i = 0; i < rows_.size(); ++i) {
        const unsigned index = first + i;
        const auto& pack = platform::maps::pack(index);
        auto& row = rows_[i];
        if (sizes_[index] == std::numeric_limits<std::uint64_t>::max())
            sizes_[index] = platform::maps::storage_size(index);
        const auto saved = sizes_[index];
        const bool focus = index == view_model().map_pack_cursor();
        const bool installed = platform::maps::installed(index);
        lv_obj_set_style_bg_color(row.box, focus ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, focus ? colors.info : colors.border, 0);
        const auto name = std::string(index >= 3 ? std::string(pack.group) + "  /  " : "") + pack.name +
            (index == view_model().map_pack() && installed ? "   ACTIVE" : "");
        lv_label_set_text(row.name, name.c_str());
        const auto meta = index == 2 ?
            (view_model().map_local_estimate() ? "~" + size_text(view_model().map_local_estimate()) +
                " for 3 km   saved " + size_text(saved) : "GPS fix needed   saved " + size_text(saved)) :
            index >= 3 ? (installed ? "Installed " + size_text(saved) +
                "   through z" + std::to_string(platform::maps::installed_zoom(index)) :
                (saved ? "Partial " + size_text(saved) + "   resume" :
                "~" + size_text(platform::maps::area_estimate(index, view_model().map_zoom())) +
                "   z1-" + std::to_string(view_model().map_zoom()))) :
            (installed ? std::string("Installed") : "Download " + size_text(pack.archive_bytes)) +
                "   z1-6   disk " + size_text(pack.installed_bytes);
        lv_label_set_text(row.meta, meta.c_str());
        lv_obj_set_style_text_color(row.name, focus ? colors.info : colors.text, 0);
        lv_obj_set_style_text_color(row.meta, colors.text_disabled, 0);
    }
    const auto zoom = "Zoom  < z" + std::to_string(view_model().map_zoom()) + " >" +
        (view_model().map_pack_cursor() >= 3 ? "   est " + size_text(view_model().map_area_estimate()) : "");
    lv_label_set_text(zoom_, zoom.c_str());
    lv_obj_set_style_text_color(zoom_, colors.info, 0);
    lv_label_set_text(message_, view_model().map_message().c_str());
    lv_obj_set_style_text_color(message_, colors.text_disabled, 0);
    set_key_guide("F/X Maps  Z/C Zoom  Enter Get/Use  D Delete  Esc Back");
}
}
