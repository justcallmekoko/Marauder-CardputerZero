/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

using model::MarauderMenuAction;
using model::MarauderMenuModel;

namespace {

bool select_index(MarauderMenuModel& menu, std::size_t index) {
    while (menu.selected_index() != index) menu.next();
    return menu.selected_index() == index;
}

} // namespace

int main() {
    MarauderMenuModel menu;
    if (menu.item_count() != 5 || menu.selected_index() != 0 || menu.depth() != 1) return 1;
    menu.previous();
    if (menu.selected_index() != 4) return 2;
    menu.next();
    if (menu.selected_index() != 0) return 3;

    // WiFi > Scanners > Access Points routes to the implemented managed scan.
    if (menu.activate() != MarauderMenuAction::None || menu.depth() != 2) return 4;
    if (menu.activate() != MarauderMenuAction::None || menu.depth() != 3) return 5;
    if (menu.activate() != MarauderMenuAction::OpenWifiScan) return 6;

    // Unavailable items stay in place and never emit an action.
    menu.next();
    if (menu.item(menu.selected_index()).available) return 7;
    if (menu.activate() != MarauderMenuAction::None || menu.depth() != 3) return 8;

    // Back restores the selection in each parent frame.
    if (!menu.back() || menu.depth() != 2 || menu.selected_index() != 0) return 9;
    if (!menu.back() || menu.depth() != 1 || menu.selected_index() != 0) return 10;
    if (menu.back()) return 11;

    // Every root category opens a non-empty submenu and returns to its root selection.
    for (std::size_t root_index = 0; root_index < 5; ++root_index) {
        if (!select_index(menu, root_index)) return 12;
        if (!menu.item(root_index).opens_submenu || !menu.item(root_index).available) return 13;
        if (menu.activate() != MarauderMenuAction::None || menu.depth() != 2 || menu.item_count() == 0) return 14;
        if (!menu.back() || menu.depth() != 1 || menu.selected_index() != root_index) return 15;
    }

    // Device and Settings expose the currently implemented actions.
    select_index(menu, 3);
    menu.activate();
    if (menu.activate() != MarauderMenuAction::OpenAdapters) return 16;
    menu.next();
    if (menu.activate() != MarauderMenuAction::OpenDiagnostics) return 17;
    menu.back();

    select_index(menu, 4);
    menu.activate();
    if (menu.activate() != MarauderMenuAction::ToggleTheme) return 18;
    menu.next();
    if (menu.activate() != MarauderMenuAction::OpenAdapters) return 19;
    menu.next();
    if (menu.activate() != MarauderMenuAction::OpenDiagnostics) return 20;

    return 0;
}
