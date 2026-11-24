/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {

const char* BaseModel::app_title() const { return "Marauder"; }
bool BaseModel::dark_mode() const { return true; }
AppPage BaseModel::current_page() const { return current_page_; }
void BaseModel::set_current_page(AppPage page) { current_page_ = page; }
void BaseModel::toggle_page() {
    switch (current_page_) {
        case AppPage::Dashboard:
            current_page_ = AppPage::Adapters;
            break;
        case AppPage::Adapters:
            current_page_ = AppPage::WifiScan;
            break;
        case AppPage::WifiScan:
            current_page_ = AppPage::Diagnostics;
            break;
        case AppPage::Diagnostics:
            current_page_ = AppPage::Dashboard;
            break;
    }
}
const std::string& BaseModel::selected_adapter_id() const { return selected_adapter_id_; }
void BaseModel::set_selected_adapter_id(std::string stable_id) {
    selected_adapter_id_ = std::move(stable_id);
}

} // namespace model
