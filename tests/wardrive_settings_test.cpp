// SPDX-License-Identifier: AGPL-3.0-only
#include "base_model.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

int main() {
    char path[] = "/tmp/marauder-wardrive-XXXXXX";
    const auto* root = mkdtemp(path);
    assert(root && setenv("XDG_CONFIG_HOME", root, 1) == 0);
    {
        model::BaseModel settings;
        assert(settings.wardrive_dwell_ms() == 650);
        assert(settings.wardrive_priority() == 0);
        assert(settings.wardrive_all_idle_adapters());
        assert(settings.cycle_wardrive_dwell() && settings.wardrive_dwell_ms() == 300);
        assert(settings.cycle_wardrive_priority() && settings.wardrive_priority() == 1);
        assert(settings.cycle_wardrive_adapters() && !settings.wardrive_all_idle_adapters());
        assert(settings.set_monitor_preference("usb:radio-a", true));
        assert(settings.set_monitor_preference("usb:radio-b", true));
        assert(!settings.set_monitor_preference("ifindex:2", true));
    }
    {
        model::BaseModel reopened;
        assert(reopened.wardrive_dwell_ms() == 300);
        assert(reopened.wardrive_priority() == 1);
        assert(!reopened.wardrive_all_idle_adapters());
        assert(reopened.monitor_preferences().size() == 2);
        assert(reopened.set_monitor_preference("usb:radio-a", false));
    }
    assert(model::BaseModel{}.monitor_preferences().count("usb:radio-b") == 1);
    std::filesystem::remove_all(root);
}
