/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace model {

enum class BatteryDisplay { Both, Icon, Percent };
const char* battery_display_name(BatteryDisplay mode);

enum class AppPage {
    Dashboard = 0,
    Adapters = 1,
    WifiScan = 2,
    Diagnostics = 3,
    RadioSetup,
};

enum class Credential { WigleName, WigleToken, WdgKey };
bool credential_saved(Credential field);
bool store_credential(Credential field, const std::string& value);

class BaseModel {
public:
    BaseModel();
    BatteryDisplay battery_display() const { return battery_display_; }
    void cycle_battery_display();
    bool screenshots_in_documents() const { return documents_; }
    unsigned cache_kib() const { return cache_kib_; }
    bool cycle_storage();
    bool cycle_cache();
    const char* app_title() const;
    bool dark_mode() const;
    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();
    const std::string& selected_adapter_id() const;
    void set_selected_adapter_id(std::string stable_id);

private:
    bool documents_{false};
    unsigned cache_kib_{0};
    BatteryDisplay battery_display_{BatteryDisplay::Both};
    AppPage current_page_{AppPage::Dashboard};
    std::string selected_adapter_id_;
};

} // namespace model
