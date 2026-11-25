// SPDX-License-Identifier: AGPL-3.0-only
#include "radio_power.h"
#include <cstdio>
#include <sstream>

namespace platform {
RadioInventory read_radios(std::string id, bool enable) {
#if !defined(__linux__) || USE_DESKTOP
    return {"Device radios unavailable", {}};
#else
    std::string command = "/usr/bin/timeout 20 /usr/bin/python3 -I /usr/lib/marauder/radio-inventory";
    if (!id.empty()) {
        if (id.size() != 64 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
            return {"Invalid device", {}};
        command += " " + id + (enable ? " on" : " off");
    }
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {"Inventory unavailable", {}};
    RadioInventory result{"Inventory unavailable", {}};
    char buffer[2048];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        line.erase(line.find_last_not_of("\r\n") + 1);
        std::istringstream row(line);
        std::vector<std::string> fields;
        for (std::string field; std::getline(row, field, '\t');) fields.push_back(field);
        if (fields.size() == 2 && fields[0] == "status") result.message = fields[1];
        if (fields.size() == 9 && fields[0] == "radio")
            result.devices.push_back({fields[1], fields[2], fields[3], fields[4],
                                      fields[5], fields[6], fields[7], fields[8]});
    }
    if (pclose(pipe) != 0) return {"Inventory unavailable", {}};
    return result;
#endif
}
}
