/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

#include <string>

using model::MarauderMenuAction;
using model::MarauderMenuColor;
using model::MarauderMenuModel;

namespace {

bool select_index(MarauderMenuModel& menu, std::size_t index) {
    while (menu.selected_index() != index) menu.next();
    return menu.selected_index() == index;
}

} // namespace

int main() {
    MarauderMenuModel menu;
    if (menu.item_count() != 4 || menu.selected_index() != 0 || menu.depth() != 1) return 1;
    if (std::string(menu.title()) != "MARAUDER") return 21;
    const MarauderMenuColor root_colors[] = {
        MarauderMenuColor::Green,
        MarauderMenuColor::Cyan,
        MarauderMenuColor::Red,
        MarauderMenuColor::Blue,
    };
    for (std::size_t index = 0; index < menu.item_count(); ++index) {
        if (menu.item(index).color != root_colors[index]) return 22;
    }
    menu.move(-1, 0);
    if (menu.selected_index() != 0) return 30;
    menu.move(1, 0);
    menu.move(0, 1);
    if (menu.selected_index() != 3 || menu.depth() != 1) return 31;
    menu.move(-1, 0);
    menu.move(0, -1);
    if (menu.selected_index() != 0) return 32;
    menu.previous();
    if (menu.selected_index() != 3) return 2;
    menu.next();
    if (menu.selected_index() != 0) return 3;

    // WiFi > Networks > Access Points routes to live monitor capture.
    if (menu.activate() != MarauderMenuAction::None || menu.depth() != 2) return 4;
    if (menu.activate() != MarauderMenuAction::None || menu.depth() != 3) return 5;
    if (menu.activate() != MarauderMenuAction::OpenWifiScan) return 6;

    if (menu.item_count() != 1) return 7;

    // Back restores the selection in each parent frame.
    if (!menu.back() || menu.depth() != 2 || menu.selected_index() != 0) return 9;
    if (!menu.back() || menu.depth() != 1 || menu.selected_index() != 0) return 10;
    if (menu.back()) return 11;

    // Every root category opens a non-empty submenu and returns to its root selection.
    for (std::size_t root_index = 0; root_index < 4; ++root_index) {
        if (!select_index(menu, root_index)) return 12;
        if (!menu.item(root_index).opens_submenu || !menu.item(root_index).available) return 13;
        if (menu.activate() != MarauderMenuAction::None || menu.depth() != 2 || menu.item_count() == 0) return 14;
        if (!menu.back() || menu.depth() != 1 || menu.selected_index() != root_index) return 15;
    }

    // Device and Settings expose the currently implemented actions.
    select_index(menu, 3);
    menu.activate();
    if (menu.columns() != 1 || menu.item_count() != 5) return 16;
    const std::vector<std::vector<MarauderMenuAction>> groups = {
        {MarauderMenuAction::OpenRadioSetup, MarauderMenuAction::OpenAdapters, MarauderMenuAction::CycleTrafficSource},
        {MarauderMenuAction::Brightness, MarauderMenuAction::DimDelay, MarauderMenuAction::CycleBatteryDisplay},
        {MarauderMenuAction::CycleStorage, MarauderMenuAction::OpenPcapFiles, MarauderMenuAction::Reserve, MarauderMenuAction::CycleCache},
        {MarauderMenuAction::UploadPolicy, MarauderMenuAction::WigleName, MarauderMenuAction::WigleToken, MarauderMenuAction::WdgKey}
    };
    for (const auto& group : groups) {
        menu.activate();
        if (menu.item_count() != group.size() || menu.columns() != 1) return 17;
        for (auto action : group) {
            if (menu.activate() != action) return 18;
            menu.next();
        }
        menu.back();
        menu.next();
    }
    if (menu.activate() != MarauderMenuAction::OpenDiagnostics) return 41;
    return 0;
}
