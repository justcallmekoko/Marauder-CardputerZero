// SPDX-License-Identifier: AGPL-3.0-only
#include "radio_power.h"

#include <cstdlib>

namespace platform {
std::string enable_radio(Radio radio) {
#if !defined(__linux__) || USE_DESKTOP
    (void)radio;
    return "Device controls need Cardputer";
#else
    // Fixed commands only. The caller runs this on a worker thread.
    auto succeeds = [](const char* command) { return std::system(command) == 0; };
    switch (radio) {
        case Radio::Wifi: {
            if (!succeeds("LC_ALL=C timeout 4 nmcli -t -f TYPE device status 2>/dev/null | grep -Fxq wifi"))
                return "No WiFi device / NM offline";
            const char* ready = "LC_ALL=C timeout 4 nmcli radio wifi 2>/dev/null | grep -Fxq enabled";
            if (succeeds(ready)) return "WiFi enabled";
            if (succeeds("timeout 5 nmcli radio wifi on >/dev/null 2>&1") && succeeds(ready))
                return "WiFi enabled";
            return "WiFi enable failed";
        }
        case Radio::Bluetooth: {
            const char* ready = "LC_ALL=C timeout 4 bluetoothctl show 2>/dev/null | grep -Eq 'Powered: yes'";
            if (succeeds(ready)) return "Bluetooth powered on";
            if (!succeeds("LC_ALL=C timeout 4 bluetoothctl list 2>/dev/null | grep -q Controller"))
                return "No Bluetooth controller";
            if (succeeds("timeout 5 bluetoothctl power on >/dev/null 2>&1") && succeeds(ready))
                return "Bluetooth powered on";
            return "Bluetooth enable failed";
        }
        case Radio::Gps: {
            const char* ready = "timeout 4 systemctl is-active --quiet gpsd.socket";
            if (succeeds(ready)) return "GPS socket ready";
            if (succeeds("timeout 6 systemctl --no-ask-password start gpsd.socket >/dev/null 2>&1") &&
                succeeds(ready)) return "GPS socket ready";
            return "GPS socket start failed";
        }
    }
    return "Unknown radio";
#endif
}
}
