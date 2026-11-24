/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include "system_info_model.h"

#include <algorithm>

namespace model {
namespace {
std::size_t page_number(SystemInfoPage page) { return static_cast<std::size_t>(page); }
}

SystemInfoPage SystemInfoModel::page() const { return page_; }
std::size_t SystemInfoModel::page_index() const { return page_number(page_); }
std::size_t SystemInfoModel::subpage_index() const { return subpage_; }
std::size_t SystemInfoModel::subpage_count() const { return subpage_count(page_); }
bool SystemInfoModel::details_expanded() const { return details_expanded_; }

void SystemInfoModel::refresh_counts(std::size_t interfaces,
                                     std::size_t usb_devices,
                                     std::size_t peripherals,
                                     std::size_t tools) {
    item_counts_ = {{6,
                     6,
                     std::max<std::size_t>(1, interfaces),
                     std::max<std::size_t>(1, usb_devices + peripherals),
                     std::max<std::size_t>(1, tools + 1)}};
    subpage_ = std::min(subpage_, subpage_count() - 1);
}

void SystemInfoModel::previous_page() {
    if (subpage_ > 0) {
        --subpage_;
        return;
    }
    if (page_index() == 0) return;
    const auto previous = static_cast<SystemInfoPage>(page_index() - 1);
    set_page(previous, subpage_count(previous) - 1);
}

void SystemInfoModel::next_page() {
    if (subpage_ + 1 < subpage_count()) {
        ++subpage_;
        return;
    }
    if (page_index() + 1 >= kPageCount) return;
    set_page(static_cast<SystemInfoPage>(page_index() + 1), 0);
}

void SystemInfoModel::toggle_details() { details_expanded_ = !details_expanded_; }

std::size_t SystemInfoModel::item_count(SystemInfoPage page) const {
    return item_counts_[page_number(page)];
}

std::size_t SystemInfoModel::subpage_count(SystemInfoPage page) const {
    return std::max<std::size_t>(1, (item_count(page) + kRowsPerSubpage - 1) / kRowsPerSubpage);
}

void SystemInfoModel::set_page(SystemInfoPage page, std::size_t subpage) {
    page_ = page;
    subpage_ = std::min(subpage, subpage_count(page) - 1);
    details_expanded_ = false;
}

const char* system_info_page_title(SystemInfoPage page) {
    switch (page) {
        case SystemInfoPage::Summary: return "SUMMARY";
        case SystemInfoPage::System: return "SYSTEM";
        case SystemInfoPage::Interfaces: return "INTERFACES";
        case SystemInfoPage::UsbPeripherals: return "USB / PERIPHERALS";
        case SystemInfoPage::Tools: return "TOOLS";
    }
    return "SUMMARY";
}

} // namespace model
