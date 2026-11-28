/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <vector>

namespace model {

enum class MarauderMenuId {
    Main,
    Wifi,
    WifiNetworks,
    WifiActivity,
    WifiAttacks,
    WifiGeneral,
    Bluetooth,
    BluetoothRecon,
    BluetoothCollection,
    BluetoothAttacks,
    BluetoothGeneral,
    BluetoothDevices,
    Gps,
    Device,
    Accounts,
    Radios,
    WardriveSettings,
    Display,
    FilesAndMemory,
};

enum class MarauderMenuAction {
    None,
    OpenWifiScan,
    OpenRawCapture,
    OpenBluetoothRaw,
    OpenBluetoothDevices,
    OpenBluetoothAdapters,
    OpenBluetoothFindMy,
    OpenBluetoothIBeacon,
    OpenBluetoothFlipper,
    OpenBluetoothFlock,
    OpenBluetoothMeta,
    OpenBluetoothSerial,
    OpenRemoteId,
    OpenGpsInfo,
    OpenWardrive,
    OpenGpsTrack,
    OpenPineapple,
    OpenTraffic,
    OpenPcapFiles,
    OpenWardriveFiles,
    OpenMapPacks,
    CycleTrafficSource,
    OpenAdapters,
    OpenDiagnostics,
    CycleBatteryDisplay,
    CycleCensor,
    CycleWardriveDwell,
    CycleWardrivePriority,
    CycleWardriveAdapters,
    OpenRadioSetup,
    CycleStorage,
    CycleCache,
    Brightness,
    DimDelay,
    Reserve,
    UploadPolicy,
    WigleName,
    WigleToken,
    WdgKey,
    CheckWigle,
    CheckWdg,
};

enum class MarauderMenuColor {
    White,
    Cyan,
    Blue,
    Red,
    Green,
    LightGrey,
    Magenta,
    Purple,
    Orange,
    Yellow,
};

enum class RadioRequirement { None, Selected, Managed, Monitor };

struct MarauderMenuItem {
    const char* label;
    MarauderMenuColor color{MarauderMenuColor::White};
    MarauderMenuAction action{MarauderMenuAction::None};
    MarauderMenuId submenu{MarauderMenuId::Main};
    bool opens_submenu{false};
    bool available{true};
    RadioRequirement radio{RadioRequirement::None};
};

class MarauderMenuModel {
public:
    MarauderMenuModel();

    const char* title() const;
    MarauderMenuId current_menu() const;
    std::size_t depth() const;
    bool can_go_back() const;
    std::size_t selected_index() const;
    std::size_t item_count() const;
    const MarauderMenuItem& item(std::size_t index) const;
    std::size_t columns() const;
    void move(int dx, int dy);
    void previous();
    void next();
    MarauderMenuAction activate();
    bool back();

private:
    struct Frame {
        MarauderMenuId menu;
        std::size_t selected;
    };

    std::vector<Frame> stack_;
};

} // namespace model
