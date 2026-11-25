/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#pragma once

#include <array>
#include <cstddef>

namespace model {

enum class SystemInfoPage { Summary = 0, System, Interfaces, UsbPeripherals, Tools };

class SystemInfoModel {
public:
    static constexpr std::size_t kPageCount = 5;
    static constexpr std::size_t kRowsPerSubpage = 4;

    SystemInfoPage page() const;
    std::size_t page_index() const;
    std::size_t subpage_index() const;
    std::size_t subpage_count() const;
    bool details_expanded() const;

    void refresh_counts(std::size_t interfaces,
                        std::size_t usb_devices,
                        std::size_t peripherals,
                        std::size_t tools);
    void previous_page();
    void next_page();
    void toggle_details();

private:
    std::size_t item_count(SystemInfoPage page) const;
    std::size_t subpage_count(SystemInfoPage page) const;
    void set_page(SystemInfoPage page, std::size_t subpage);

    SystemInfoPage page_{SystemInfoPage::Summary};
    std::size_t subpage_{0};
    bool details_expanded_{false};
    std::array<std::size_t, kPageCount> item_counts_{{6, 6, 1, 1, 1}};
};

const char* system_info_page_title(SystemInfoPage page);

} // namespace model
