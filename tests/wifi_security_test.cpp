// SPDX-License-Identifier: AGPL-3.0-only
#include "wifi_security.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using platform::wireless::interpret_security;
using Bytes = std::vector<uint8_t>;

static void u16(Bytes& bytes, unsigned value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}
static void suite(Bytes& bytes, unsigned oui, unsigned type) {
    bytes.push_back(static_cast<uint8_t>(oui >> 16));
    bytes.push_back(static_cast<uint8_t>(oui >> 8));
    bytes.push_back(static_cast<uint8_t>(oui));
    bytes.push_back(static_cast<uint8_t>(type));
}
static void ie(Bytes& bytes, unsigned tag, const Bytes& value) {
    assert(value.size() <= 255);
    bytes.push_back(static_cast<uint8_t>(tag));
    bytes.push_back(static_cast<uint8_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}
static Bytes rsn(const std::vector<unsigned>& akms, unsigned cipher = 4,
                 unsigned caps = 0, unsigned group = 4, unsigned management = 0) {
    Bytes bytes;
    u16(bytes, 1);
    suite(bytes, 0x000fac, group);
    u16(bytes, 1); suite(bytes, 0x000fac, cipher);
    u16(bytes, static_cast<unsigned>(akms.size()));
    for (const auto type : akms) suite(bytes, 0x000fac, type);
    u16(bytes, caps);
    if (management) { u16(bytes, 0); suite(bytes, 0x000fac, management); }
    return bytes;
}
static Bytes wpa1(unsigned akm) {
    Bytes bytes{0, 0x50, 0xf2, 1};
    u16(bytes, 1); suite(bytes, 0x0050f2, 2);
    u16(bytes, 1); suite(bytes, 0x0050f2, 2);
    u16(bytes, 1); suite(bytes, 0x0050f2, akm);
    return bytes;
}
static platform::wireless::SecurityInfo parse(bool privacy, const Bytes& ies) {
    return interpret_security(privacy, ies.data(), ies.size());
}

int main() {
    assert(parse(false, {}).summary == "Open");
    assert(parse(true, {}).summary == "Legacy privacy (WEP?)");
    Bytes bytes;
    ie(bytes, 48, rsn({2}));
    auto info = parse(true, bytes);
    assert(info.summary == "WPA2 Personal" && info.akm == "PSK");
    assert(info.pairwise == "CCMP-128" && info.group == "CCMP-128");
    assert(info.pmf == "Not advertised");
    assert(parse(false, bytes).summary == "Inconsistent privacy flag");

    bytes.clear(); ie(bytes, 48, rsn({8}, 4, 0xc0));
    ie(bytes, 244, {0x20});
    info = parse(true, bytes);
    assert(info.summary == "WPA3 Personal (SAE)" && info.pmf == "Required");
    assert(info.rsnxe == "SAE H2E");

    bytes.clear(); ie(bytes, 48, rsn({2, 8}, 4, 0x80));
    assert(parse(true, bytes).summary == "WPA2/WPA3 Personal");
    bytes.clear(); ie(bytes, 48, rsn({2, 8}, 4, 0));
    assert(parse(true, bytes).summary == "PSK/SAE (PMF unavailable)");
    bytes.clear(); ie(bytes, 48, rsn({8}, 2, 0xc0, 2));
    assert(parse(true, bytes).summary == "SAE with legacy cipher");

    bytes.clear(); ie(bytes, 48, rsn({18}, 4, 0xc0));
    assert(parse(true, bytes).summary == "Enhanced Open (OWE)");
    bytes.clear(); ie(bytes, 48, rsn({1}));
    assert(parse(true, bytes).summary == "WPA2 Enterprise");
    bytes.clear(); ie(bytes, 48, rsn({12}, 9, 0xc0, 9, 12));
    info = parse(true, bytes);
    assert(info.summary == "WPA3-Enterprise 192" && info.group_management == "BIP-GMAC-256");
    bytes.clear(); ie(bytes, 48, rsn({12}, 4, 0xc0));
    assert(parse(true, bytes).summary == "Suite-B-192 (profile mismatch)");

    bytes.clear();
    auto dpp = rsn({2});
    dpp[14] = 0x50; dpp[15] = 0x6f; dpp[16] = 0x9a; dpp[17] = 2;
    ie(bytes, 48, dpp);
    assert(parse(true, bytes).summary == "DPP");
    bytes.clear(); ie(bytes, 221, wpa1(2));
    info = parse(true, bytes);
    assert(info.summary == "WPA1 Personal" && info.pairwise == "TKIP");
    ie(bytes, 48, rsn({2}));
    assert(parse(true, bytes).summary == "WPA1 Personal + WPA2 Personal");

    bytes.clear(); ie(bytes, 48, rsn({254}));
    info = parse(true, bytes);
    assert(info.summary == "RSN with unknown AKM");
    assert(info.akm == "00:0F:AC:FE");
    bytes.clear(); ie(bytes, 48, rsn({2}, 254));
    info = parse(true, bytes);
    assert(info.pairwise == "00:0F:AC:FE" && info.note == "Unknown cipher selector");
    bytes.clear(); ie(bytes, 48, {1, 0});
    assert(parse(true, bytes).summary == "Malformed RSN");
    bytes.clear(); ie(bytes, 48, rsn({2})); ie(bytes, 48, rsn({8}));
    assert(parse(true, bytes).summary == "Conflicting security IEs");
    bytes.clear(); ie(bytes, 48, rsn({2}, 4, 0x40));
    assert(parse(true, bytes).summary == "Invalid RSN PMF flags");
    bytes.clear(); ie(bytes, 48, rsn({2})); ie(bytes, 221, {0, 0x50, 0xf2, 4});
    assert(parse(true, bytes).note == "WPS IE present");
    bytes.push_back(48); bytes.push_back(200);
    assert(parse(true, bytes).note.find("Truncated IEs") != std::string::npos);

    // The parser runs on untrusted radio input. Exercise bounded lengths under sanitizers.
    uint32_t seed = 0x5a1726u;
    for (unsigned sample = 0; sample < 2000; ++sample) {
        Bytes random(sample % 257);
        for (auto& byte : random) {
            seed = seed * 1664525u + 1013904223u;
            byte = static_cast<uint8_t>(seed >> 24);
        }
        (void)parse((sample & 1) != 0, random);
    }
}
