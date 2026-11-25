// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace platform::wireless {

struct SecurityInfo {
    std::string summary;
    std::string akm;
    std::string pairwise;
    std::string group;
    std::string group_management;
    std::string pmf;
    std::string rsnxe;
    std::string note;
    std::string legacy_wpa;
};

// Interprets advertised beacon/probe-response IEs, not the security negotiated by a client.
SecurityInfo interpret_security(bool privacy, const uint8_t* ies, std::size_t length);

} // namespace platform::wireless
