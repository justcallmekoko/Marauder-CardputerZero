/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

#include <string>
#include <vector>

using model::MarauderMenuAction;
using model::MarauderMenuModel;

namespace {

bool select_index(MarauderMenuModel& menu, std::size_t index) {
    while (menu.selected_index() != index) menu.next();
    return menu.selected_index() == index;
}

bool expect_actions(MarauderMenuModel& menu,
                    const std::vector<MarauderMenuAction>& actions,
                    int& failure,
                    int first_failure) {
    if (menu.item_count() != actions.size()) {
        failure = first_failure;
        return false;
    }
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (!select_index(menu, i) || menu.activate() != actions[i]) {
            failure = first_failure + static_cast<int>(i) + 1;
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    MarauderMenuModel menu;
    if (menu.item_count() != 4 || menu.columns() != 2 || menu.depth() != 1) return 1;
    const char* roots[] = {"WiFi", "Bluetooth", "Location", "Device"};
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::string(menu.item(i).label) != roots[i] || !menu.item(i).opens_submenu) return 2 + static_cast<int>(i);
    }

    // Tile navigation wraps while preserving valid cells in uneven rows.
    menu.move(-1, 0);
    if (menu.selected_index() != 1) return 10;
    menu.move(0, -1);
    if (menu.selected_index() != 3) return 11;
    menu.move(1, 0);
    if (menu.selected_index() != 2) return 12;
    select_index(menu, 0);

    // WiFi is organized by intent. Attack placeholders remain visible.
    menu.activate();
    if (menu.depth() != 2 || menu.item_count() != 4 || std::string(menu.item(0).label) != "Discover" ||
        std::string(menu.item(1).label) != "Monitor" || std::string(menu.item(2).label) != "Attacks" ||
        std::string(menu.item(3).label) != "Setup") return 20;
    menu.activate();
    if (menu.depth() != 3 || menu.item_count() != 2 || std::string(menu.title()) != "WiFi > Discover") return 21;
    if (menu.activate() != MarauderMenuAction::OpenWifiScan) return 22;
    menu.next();
    if (menu.activate() != MarauderMenuAction::OpenPineapple) return 23;
    menu.back();
    select_index(menu, 1);
    menu.activate();
    if (menu.item_count() != 2 || menu.activate() != MarauderMenuAction::OpenRawCapture) return 24;
    menu.next();
    if (menu.activate() != MarauderMenuAction::OpenTraffic) return 25;
    menu.back();
    select_index(menu, 2);
    menu.activate();
    if (menu.item_count() != 3 || menu.item(0).available || menu.item(1).available || menu.item(2).available) return 26;
    menu.back();
    select_index(menu, 3);
    menu.activate();
    if (menu.item_count() != 3 || menu.activate() != MarauderMenuAction::OpenAdapters) return 27;
    menu.next();
    if (menu.activate() != MarauderMenuAction::CycleTrafficSource) return 28;
    menu.next();
    if (menu.item(2).available || menu.activate() != MarauderMenuAction::None) return 29;
    menu.back();
    menu.back();

    // Bluetooth filtered discovery is flattened into one keyboard-reachable list.
    select_index(menu, 1);
    menu.activate();
    if (menu.item_count() != 4 || std::string(menu.item(0).label) != "Discover" ||
        std::string(menu.item(1).label) != "Monitor" || std::string(menu.item(2).label) != "Attacks" ||
        std::string(menu.item(3).label) != "Setup") return 40;
    menu.activate();
    if (menu.columns() != 1 || menu.item_count() != 7 || std::string(menu.title()) != "Bluetooth > Discover") return 41;
    const MarauderMenuAction bluetooth_actions[] = {
        MarauderMenuAction::OpenBluetoothDevices,
        MarauderMenuAction::OpenBluetoothFindMy,
        MarauderMenuAction::OpenBluetoothIBeacon,
        MarauderMenuAction::OpenBluetoothFlipper,
        MarauderMenuAction::OpenBluetoothFlock,
        MarauderMenuAction::OpenBluetoothMeta,
        MarauderMenuAction::OpenBluetoothSerial,
    };
    for (std::size_t i = 0; i < 7; ++i) {
        if (!select_index(menu, i) || menu.activate() != bluetooth_actions[i]) return 42 + static_cast<int>(i);
    }
    menu.move(0, 1);
    if (menu.selected_index() != 0) return 50;
    menu.move(0, -1);
    if (menu.selected_index() != 6) return 51;
    menu.back();
    if (menu.selected_index() != 0) return 52;
    select_index(menu, 1);
    menu.activate();
    if (menu.item_count() != 1 || menu.activate() != MarauderMenuAction::OpenBluetoothRaw) return 53;
    menu.back();
    select_index(menu, 2);
    menu.activate();
    if (menu.item_count() != 2 || menu.item(0).available || menu.item(1).available) return 54;
    menu.back();
    select_index(menu, 3);
    menu.activate();
    if (menu.item_count() != 2 || menu.activate() != MarauderMenuAction::OpenBluetoothAdapters || menu.item(1).available) return 55;
    menu.back();
    menu.back();

    // Cross-radio and GPS/map workflows live together under Location.
    select_index(menu, 2);
    menu.activate();
    if (menu.columns() != 1 || menu.item_count() != 6 || std::string(menu.title()) != "Location") return 60;
    const MarauderMenuAction location_actions[] = {
        MarauderMenuAction::OpenRemoteId,
        MarauderMenuAction::OpenWardrive,
        MarauderMenuAction::OpenGpsTrack,
        MarauderMenuAction::OpenGpsInfo,
        MarauderMenuAction::OpenMapPacks,
    };
    for (std::size_t i = 0; i < 5; ++i) {
        if (!select_index(menu, i) || menu.activate() != location_actions[i]) return 61 + static_cast<int>(i);
    }
    select_index(menu, 5);
    if (menu.activate() != MarauderMenuAction::None || menu.depth() != 3 || menu.item_count() != 3) return 66;
    const MarauderMenuAction wardrive_actions[] = {
        MarauderMenuAction::CycleWardriveDwell,
        MarauderMenuAction::CycleWardrivePriority,
        MarauderMenuAction::CycleWardriveAdapters,
    };
    for (std::size_t i = 0; i < 3; ++i) {
        if (!select_index(menu, i) || menu.activate() != wardrive_actions[i]) return 67 + static_cast<int>(i);
    }
    menu.back();
    if (menu.selected_index() != 5) return 70;
    menu.back();

    // Device contains hardware, presentation/privacy, files, accounts, and diagnostics.
    select_index(menu, 3);
    menu.activate();
    if (menu.columns() != 1 || menu.item_count() != 5) return 80;
    if (menu.activate() != MarauderMenuAction::OpenRadioSetup) return 81;
    select_index(menu, 1);
    menu.activate();
    int failure = 0;
    if (!expect_actions(menu, {MarauderMenuAction::Brightness, MarauderMenuAction::DimDelay,
                               MarauderMenuAction::CycleBatteryDisplay, MarauderMenuAction::CycleCensor},
                        failure, 82)) return failure;
    menu.back();
    select_index(menu, 2);
    menu.activate();
    if (!expect_actions(menu, {MarauderMenuAction::CycleStorage, MarauderMenuAction::OpenPcapFiles,
                               MarauderMenuAction::OpenWardriveFiles, MarauderMenuAction::Reserve,
                               MarauderMenuAction::CycleCache}, failure, 90)) return failure;
    menu.back();
    select_index(menu, 3);
    menu.activate();
    if (!expect_actions(menu, {MarauderMenuAction::UploadPolicy, MarauderMenuAction::WigleName,
                               MarauderMenuAction::WigleToken, MarauderMenuAction::WdgKey,
                               MarauderMenuAction::CheckWigle, MarauderMenuAction::CheckWdg},
                        failure, 100)) return failure;
    menu.back();
    select_index(menu, 4);
    if (menu.activate() != MarauderMenuAction::OpenDiagnostics) return 110;

    // Back restores the selected row in every parent frame.
    if (!menu.back() || menu.selected_index() != 3 || menu.depth() != 1 || menu.back()) return 120;
    return 0;
}
