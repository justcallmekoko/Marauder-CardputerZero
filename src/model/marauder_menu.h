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
    WifiScanners,
    WifiSniffers,
    WifiAttacks,
    WifiGeneral,
    Bluetooth,
    Gps,
    Device,
    Settings,
};

enum class MarauderMenuAction {
    None,
    OpenWifiScan,
    OpenAdapters,
    OpenDiagnostics,
    ToggleTheme,
};

struct MarauderMenuItem {
    const char* label;
    MarauderMenuAction action{MarauderMenuAction::None};
    MarauderMenuId submenu{MarauderMenuId::Main};
    bool opens_submenu{false};
    bool available{true};
};

class MarauderMenuModel {
public:
    MarauderMenuModel();

    const char* title() const;
    MarauderMenuId current_menu() const;
    std::size_t depth() const;
    std::size_t selected_index() const;
    std::size_t item_count() const;
    const MarauderMenuItem& item(std::size_t index) const;
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
