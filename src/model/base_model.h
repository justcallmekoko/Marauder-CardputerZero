/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace model {

enum class AppPage {
    Dashboard = 0,
    Adapters = 1,
    Diagnostics = 2,
};

class BaseModel {
public:
    const char* app_title() const;
    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();
    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();
    const std::string& selected_adapter_id() const;
    void set_selected_adapter_id(std::string stable_id);

private:
    bool dark_mode_{true};
    AppPage current_page_{AppPage::Dashboard};
    std::string selected_adapter_id_;
};

} // namespace model
