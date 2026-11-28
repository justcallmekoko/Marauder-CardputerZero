/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

#include <algorithm>
#include <stdexcept>

namespace model {
namespace {

using Action = MarauderMenuAction;
using Color = MarauderMenuColor;
using Id = MarauderMenuId;
using Item = MarauderMenuItem;

const std::vector<Item>& items_for(Id menu) {
    static const std::vector<Item> main = {
        {"WiFi", Color::Green, Action::None, Id::Wifi, true, true},
        {"Bluetooth", Color::Cyan, Action::None, Id::Bluetooth, true, true},
        {"Location", Color::Red, Action::None, Id::Location, true, true},
        {"Device", Color::Blue, Action::None, Id::Device, true, true},
    };
    static const std::vector<Item> wifi = {
        {"Discover", Color::Orange, Action::None, Id::WifiDiscover, true, true},
        {"Monitor", Color::Yellow, Action::None, Id::WifiMonitor, true, true},
        {"Attacks", Color::Red, Action::None, Id::WifiAttacks, true, true},
        {"Setup", Color::Purple, Action::None, Id::WifiSetup, true, true},
    };
    static const std::vector<Item> wifi_discover = {
        {"Access Points", Color::Orange, Action::OpenWifiScan, Id::Main, false, true, RadioRequirement::Monitor},
        {"Pineapple Detector", Color::Yellow, Action::OpenPineapple, Id::Main, false, true, RadioRequirement::Monitor},
    };
    static const std::vector<Item> wifi_monitor = {
        {"Raw Frames", Color::Orange, Action::OpenRawCapture, Id::Main, false, true, RadioRequirement::Monitor},
        {"Traffic Graph", Color::Green, Action::OpenTraffic, Id::Main, false, true, RadioRequirement::Selected},
    };
    static const std::vector<Item> wifi_attacks = {
        {"Deauth", Color::Red, Action::None, Id::Main, false, false},
        {"Beacon Spam", Color::Orange, Action::None, Id::Main, false, false},
        {"Evil Portal", Color::Magenta, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_setup = {
        {"WiFi Adapter", Color::Purple, Action::OpenAdapters, Id::Main, false, true},
        {"Traffic Source", Color::Yellow, Action::CycleTrafficSource, Id::Main, false, true},
        {"Saved Networks", Color::LightGrey, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> bluetooth = {
        {"Discover", Color::Orange, Action::None, Id::BluetoothDiscover, true, true},
        {"Monitor", Color::Yellow, Action::None, Id::BluetoothMonitor, true, true},
        {"Attacks", Color::Red, Action::None, Id::BluetoothAttacks, true, true},
        {"Setup", Color::Purple, Action::None, Id::BluetoothSetup, true, true},
    };
    static const std::vector<Item> bluetooth_discover = {
        {"Nearby Devices", Color::Cyan, Action::OpenBluetoothDevices, Id::Main, false, true},
        {"Find My", Color::Green, Action::OpenBluetoothFindMy, Id::Main, false, true},
        {"iBeacon", Color::Blue, Action::OpenBluetoothIBeacon, Id::Main, false, true},
        {"Flipper", Color::Magenta, Action::OpenBluetoothFlipper, Id::Main, false, true},
        {"Flock", Color::Orange, Action::OpenBluetoothFlock, Id::Main, false, true},
        {"Meta", Color::Purple, Action::OpenBluetoothMeta, Id::Main, false, true},
        {"Serial Modules", Color::Red, Action::OpenBluetoothSerial, Id::Main, false, true},
    };
    static const std::vector<Item> bluetooth_monitor = {
        {"Raw Advertisements", Color::Cyan, Action::OpenBluetoothRaw, Id::Main, false, true},
    };
    static const std::vector<Item> bluetooth_attacks = {
        {"Advertisement tools", Color::Orange, Action::None, Id::Main, false, false},
        {"Spoofing tools", Color::Red, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> bluetooth_setup = {
        {"Bluetooth Adapter", Color::Purple, Action::OpenBluetoothAdapters, Id::Main, false, true},
        {"Saved Devices", Color::LightGrey, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> location = {
        {"Remote ID", Color::Magenta, Action::OpenRemoteId, Id::Main, false, true},
        {"Wardrive", Color::Green, Action::OpenWardrive, Id::Main, false, true},
        {"GPS Track", Color::Orange, Action::OpenGpsTrack, Id::Main, false, true},
        {"GPS Status", Color::Red, Action::OpenGpsInfo, Id::Main, false, true},
        {"Offline Maps", Color::Cyan, Action::OpenMapPacks, Id::Main, false, true},
        {"Wardrive Settings", Color::Yellow, Action::None, Id::WardriveSettings, true, true},
    };
    static const std::vector<Item> device = {
        {"Radio Setup", Color::Cyan, Action::OpenRadioSetup, Id::Main, false, true},
        {"Display & Privacy", Color::Green, Action::None, Id::Display, true, true},
        {"Files & Storage", Color::Blue, Action::None, Id::FilesAndMemory, true, true},
        {"Accounts & Uploads", Color::Magenta, Action::None, Id::Accounts, true, true},
        {"System Info", Color::Purple, Action::OpenDiagnostics, Id::Main, false, true},
    };
    static const std::vector<Item> wardrive_settings = {
        {"Channel dwell", Color::Green, Action::CycleWardriveDwell, Id::Main, false, true},
        {"Priority channels", Color::Yellow, Action::CycleWardrivePriority, Id::Main, false, true},
        {"WiFi adapters", Color::Cyan, Action::CycleWardriveAdapters, Id::Main, false, true},
    };
    static const std::vector<Item> display = {
        {"Brightness", Color::Green, Action::Brightness, Id::Main, false, true},
        {"Dim after", Color::Green, Action::DimDelay, Id::Main, false, true},
        {"Battery display", Color::Green, Action::CycleBatteryDisplay, Id::Main, false, true},
        {"Censor display", Color::Magenta, Action::CycleCensor, Id::Main, false, true},
    };
    static const std::vector<Item> accounts = {
        {"Upload policy", Color::Purple, Action::UploadPolicy, Id::Main, false, true},
        {"WiGLE API name", Color::Cyan, Action::WigleName, Id::Main, false, true},
        {"WiGLE API token", Color::Cyan, Action::WigleToken, Id::Main, false, true},
        {"WDGWars API key", Color::Magenta, Action::WdgKey, Id::Main, false, true},
        {"Check WiGLE login", Color::Cyan, Action::CheckWigle, Id::Main, false, true},
        {"Check WDGWars key", Color::Magenta, Action::CheckWdg, Id::Main, false, true},
    };

    static const std::vector<Item> files_memory = {
        {"Screenshots", Color::Blue, Action::CycleStorage, Id::Main, false, true},
        {"Saved captures", Color::Red, Action::OpenPcapFiles, Id::Main, false, true},
        {"Wardrive logs", Color::Green, Action::OpenWardriveFiles, Id::Main, false, true},
        {"Keep free", Color::Blue, Action::Reserve, Id::Main, false, true},
        {"Image cache", Color::Purple, Action::CycleCache, Id::Main, false, true},
    };
    switch (menu) {
        case Id::FilesAndMemory: return files_memory;
        case Id::Main: return main;
        case Id::Wifi: return wifi;
        case Id::WifiDiscover: return wifi_discover;
        case Id::WifiMonitor: return wifi_monitor;
        case Id::WifiAttacks: return wifi_attacks;
        case Id::WifiSetup: return wifi_setup;
        case Id::Bluetooth: return bluetooth;
        case Id::BluetoothDiscover: return bluetooth_discover;
        case Id::BluetoothMonitor: return bluetooth_monitor;
        case Id::BluetoothAttacks: return bluetooth_attacks;
        case Id::BluetoothSetup: return bluetooth_setup;
        case Id::Location: return location;
        case Id::Device: return device;
        case Id::WardriveSettings: return wardrive_settings;
        case Id::Display: return display;
        case Id::Accounts: return accounts;
    }
    return main;
}

const char* title_for(Id menu) {
    switch (menu) {
        case Id::Main: return "MARAUDER";
        case Id::Wifi: return "WiFi";
        case Id::WifiDiscover: return "WiFi > Discover";
        case Id::WifiMonitor: return "WiFi > Monitor";
        case Id::WifiAttacks: return "WiFi > Attacks";
        case Id::WifiSetup: return "WiFi > Setup";
        case Id::Bluetooth: return "Bluetooth";
        case Id::BluetoothDiscover: return "Bluetooth > Discover";
        case Id::BluetoothMonitor: return "Bluetooth > Monitor";
        case Id::BluetoothAttacks: return "Bluetooth > Attacks";
        case Id::BluetoothSetup: return "Bluetooth > Setup";
        case Id::Location: return "Location";
        case Id::Device: return "Device";
        case Id::WardriveSettings: return "Wardrive Settings";
        case Id::Display: return "Display & Privacy";
        case Id::Accounts: return "Accounts & Uploads";
        case Id::FilesAndMemory: return "Files & Storage";
    }
    return "MARAUDER";
}

} // namespace

MarauderMenuModel::MarauderMenuModel() : stack_{{MarauderMenuId::Main, 0}} {}
const char* MarauderMenuModel::title() const { return title_for(current_menu()); }
MarauderMenuId MarauderMenuModel::current_menu() const { return stack_.back().menu; }
std::size_t MarauderMenuModel::depth() const { return stack_.size(); }
bool MarauderMenuModel::can_go_back() const { return stack_.size() > 1; }
std::size_t MarauderMenuModel::selected_index() const { return stack_.back().selected; }
std::size_t MarauderMenuModel::item_count() const { return items_for(current_menu()).size(); }
const MarauderMenuItem& MarauderMenuModel::item(std::size_t index) const {
    const auto& items = items_for(current_menu());
    if (index >= items.size()) throw std::out_of_range("Marauder menu item");
    return items[index];
}
std::size_t MarauderMenuModel::columns() const {
    switch (current_menu()) {
        case MarauderMenuId::WardriveSettings:
        case MarauderMenuId::Display:
        case MarauderMenuId::Device:
        case MarauderMenuId::FilesAndMemory:
        case MarauderMenuId::Accounts:
        case MarauderMenuId::BluetoothDiscover:
        case MarauderMenuId::Location: return 1;
        default: return 2;
    }
}

void MarauderMenuModel::move(int dx, int dy) {
    const int width = static_cast<int>(columns());
    const int count = static_cast<int>(item_count());
    if (!count) return;
    const int current = static_cast<int>(selected_index());
    const auto wrap = [](int value, int size) { return (value % size + size) % size; };
    if (width == 1) {
        if (dy) stack_.back().selected = wrap(current + dy, count);
        return;
    }
    if (dx) {
        const int first = current / width * width;
        const int cells = std::min(width, count - first);
        stack_.back().selected = first + wrap(current - first + dx, cells);
    } else if (dy) {
        const int rows = (count + width - 1) / width;
        const int column = current % width;
        for (int step = 1; step <= rows; ++step) {
            const int candidate = wrap(current / width + dy * step, rows) * width + column;
            if (candidate < count) {
                stack_.back().selected = candidate;
                break;
            }
        }
    }
}

void MarauderMenuModel::previous() {
    auto& frame = stack_.back();
    const auto count = item_count();
    frame.selected = frame.selected == 0 ? count - 1 : frame.selected - 1;
}
void MarauderMenuModel::next() {
    auto& frame = stack_.back();
    frame.selected = (frame.selected + 1) % item_count();
}
MarauderMenuAction MarauderMenuModel::activate() {
    const auto selected = item(selected_index());
    if (!selected.available) return MarauderMenuAction::None;
    if (selected.opens_submenu) {
        stack_.push_back({selected.submenu, 0});
        return MarauderMenuAction::None;
    }
    return selected.action;
}
bool MarauderMenuModel::back() {
    if (stack_.size() <= 1) return false;
    stack_.pop_back();
    return true;
}

} // namespace model
