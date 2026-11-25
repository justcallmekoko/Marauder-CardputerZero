// SPDX-License-Identifier: AGPL-3.0-only
#include "wifi_security.h"
#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace platform::wireless {
namespace {
using Suite = uint32_t;
constexpr Suite rsn(unsigned type) { return 0x000fac00u | type; }
constexpr Suite wpa(unsigned type) { return 0x0050f200u | type; }
constexpr Suite dpp = 0x506f9a02u;

struct Cursor {
    const uint8_t* data;
    std::size_t left;
    bool take(std::size_t count, const uint8_t*& value) {
        if (count > left) return false;
        value = data;
        data += count;
        left -= count;
        return true;
    }
    bool u16(unsigned& value) {
        const uint8_t* bytes;
        if (!take(2, bytes)) return false;
        value = unsigned(bytes[0]) | (unsigned(bytes[1]) << 8);
        return true;
    }
    bool suite(Suite& value) {
        const uint8_t* bytes;
        if (!take(4, bytes)) return false;
        value = (Suite(bytes[0]) << 24) | (Suite(bytes[1]) << 16) |
                (Suite(bytes[2]) << 8) | bytes[3];
        return true;
    }
    bool suites(std::vector<Suite>& values) {
        unsigned count;
        if (!u16(count) || count == 0 || count > left / 4) return false;
        values.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            Suite value;
            if (!suite(value)) return false;
            values.push_back(value);
        }
        return true;
    }
};

struct Advertisement {
    bool seen{};
    bool valid{};
    Suite group{};
    Suite management{};
    std::vector<Suite> pairwise;
    std::vector<Suite> akms;
    unsigned capabilities{};
    bool has_capabilities{};
};

bool parse_advertisement(const uint8_t* bytes, std::size_t length,
                         bool legacy, Advertisement& result) {
    result.seen = true;
    Cursor cursor{bytes, length};
    unsigned version;
    if (!cursor.u16(version) || version != 1 || !cursor.suite(result.group) ||
        !cursor.suites(result.pairwise) || !cursor.suites(result.akms)) return false;
    if (cursor.left) {
        if (!cursor.u16(result.capabilities)) return false;
        result.has_capabilities = true;
    }
    if (!legacy && cursor.left) {
        unsigned pmkid_count;
        const uint8_t* ignored;
        if (!cursor.u16(pmkid_count) || pmkid_count > cursor.left / 16 ||
            !cursor.take(pmkid_count * 16, ignored)) return false;
        if (cursor.left && !cursor.suite(result.management)) return false;
    }
    return cursor.left == 0;
}

bool contains(const std::vector<Suite>& suites, Suite value) {
    return std::find(suites.begin(), suites.end(), value) != suites.end();
}

const char* known_akm(Suite value) {
    switch (value) {
        case rsn(1): return "802.1X";
        case rsn(2): return "PSK";
        case rsn(3): return "FT-802.1X";
        case rsn(4): return "FT-PSK";
        case rsn(5): return "802.1X-SHA256";
        case rsn(6): return "PSK-SHA256";
        case rsn(7): return "TDLS";
        case rsn(8): return "SAE";
        case rsn(9): return "FT-SAE";
        case rsn(11): return "Suite-B";
        case rsn(12): return "Suite-B-192";
        case rsn(13): return "FT-802.1X-SHA384";
        case rsn(14): return "FILS-SHA256";
        case rsn(15): return "FILS-SHA384";
        case rsn(16): return "FT-FILS-SHA256";
        case rsn(17): return "FT-FILS-SHA384";
        case rsn(18): return "OWE";
        case rsn(19): return "FT-PSK-SHA384";
        case rsn(20): return "PSK-SHA384";
        case rsn(21): return "PASN";
        case rsn(22): return "FT-802.1X-SHA384 unrestricted";
        case rsn(23): return "802.1X-SHA384";
        case rsn(24): return "SAE-EXT-KEY";
        case rsn(25): return "FT-SAE-EXT-KEY";
        case rsn(29): return "EPPKE";
        case dpp: return "DPP";
        case wpa(0): return "WPA-none";
        case wpa(1): return "WPA-802.1X";
        case wpa(2): return "WPA-PSK";
        default: return nullptr;
    }
}

const char* known_cipher(Suite value) {
    switch (value) {
        case rsn(0): case wpa(0): return "None";
        case rsn(1): case wpa(1): return "WEP40";
        case rsn(2): case wpa(2): return "TKIP";
        case rsn(4): case wpa(4): return "CCMP-128";
        case rsn(5): case wpa(5): return "WEP104";
        case rsn(6): return "BIP-CMAC-128";
        case rsn(7): return "No group data";
        case rsn(8): return "GCMP-128";
        case rsn(9): return "GCMP-256";
        case rsn(10): return "CCMP-256";
        case rsn(11): return "BIP-GMAC-128";
        case rsn(12): return "BIP-GMAC-256";
        case rsn(13): return "BIP-CMAC-256";
        default: return nullptr;
    }
}

std::string suite_name(Suite value, bool akm) {
    if (const auto* name = akm ? known_akm(value) : known_cipher(value)) return name;
    char raw[12];
    std::snprintf(raw, sizeof(raw), "%02X:%02X:%02X:%02X",
                  (value >> 24) & 0xff, (value >> 16) & 0xff,
                  (value >> 8) & 0xff, value & 0xff);
    return raw;
}

std::string list_names(const std::vector<Suite>& values, bool akm) {
    std::string names;
    std::vector<Suite> seen;
    for (const auto value : values) {
        if (contains(seen, value)) continue;
        seen.push_back(value);
        if (!names.empty()) names += ", ";
        names += suite_name(value, akm);
    }
    return names;
}

bool any(const std::vector<Suite>& suites, std::initializer_list<Suite> choices) {
    return std::any_of(choices.begin(), choices.end(), [&](Suite value) { return contains(suites, value); });
}

std::string rsn_class(const Advertisement& ad, bool mfpr, bool mfpc) {
    const auto& a = ad.akms;
    if (std::any_of(a.begin(), a.end(), [](Suite value) { return known_akm(value) == nullptr; }))
        return "RSN with unknown AKM";
    const bool sae = any(a, {rsn(8), rsn(9), rsn(24), rsn(25)});
    const bool psk = any(a, {rsn(2), rsn(4), rsn(6), rsn(19), rsn(20)});
    const bool enterprise = any(a, {rsn(1), rsn(3), rsn(5), rsn(11), rsn(13),
                                    rsn(14), rsn(15), rsn(16), rsn(17), rsn(22), rsn(23)});
    const bool suite_b_192 = contains(a, rsn(12));
    const bool owe = contains(a, rsn(18));
    const bool has_dpp = contains(a, dpp);
    const unsigned classes = unsigned(sae || psk) + unsigned(enterprise || suite_b_192) +
                             unsigned(owe) + unsigned(has_dpp);
    if (classes > 1) return "Mixed RSN modes";
    if (suite_b_192) {
        if (mfpr && contains(ad.pairwise, rsn(9)) && ad.group == rsn(9) && ad.management == rsn(12))
            return "WPA3-Enterprise 192";
        return "Suite-B-192 (profile mismatch)";
    }
    if (enterprise) return mfpr ? "802.1X RSN (PMF required)" : "WPA2 Enterprise";
    if (sae && psk)
        return !mfpc ? "PSK/SAE (PMF unavailable)" :
               mfpr ? "PSK/SAE (PMF required)" : "WPA2/WPA3 Personal";
    if (sae) {
        if (any(ad.pairwise, {rsn(1), rsn(2), rsn(5)}) ||
            ad.group == rsn(1) || ad.group == rsn(2) || ad.group == rsn(5))
            return "SAE with legacy cipher";
        return mfpr ? "WPA3 Personal (SAE)" : "SAE Personal (PMF optional)";
    }
    if (psk) return "WPA2 Personal";
    if (owe) return "Enhanced Open (OWE)";
    if (has_dpp) return "DPP";
    return "RSN (other/unknown AKM)";
}

std::string legacy_class(const Advertisement& ad) {
    const bool psk = contains(ad.akms, wpa(2));
    const bool enterprise = contains(ad.akms, wpa(1));
    if (psk && enterprise) return "WPA1 mixed auth";
    if (psk) return "WPA1 Personal";
    if (enterprise) return "WPA1 Enterprise";
    return "WPA1 (other/unknown AKM)";
}
} // namespace

SecurityInfo interpret_security(bool privacy, const uint8_t* ies, std::size_t length) {
    SecurityInfo info;
    Advertisement rsn_ad, wpa_ad;
    bool duplicate = false, truncated = false, wps = false;
    const uint8_t* rsnxe = nullptr;
    std::size_t rsnxe_length{};
    if (!ies && length) { info.summary = "Invalid security IEs"; return info; }
    for (std::size_t pos = 0; pos < length;) {
        if (length - pos < 2) { truncated = true; break; }
        const unsigned tag = ies[pos], size = ies[pos + 1];
        pos += 2;
        if (size > length - pos) { truncated = true; break; }
        const auto* body = ies + pos;
        if (tag == 48) {
            if (rsn_ad.seen) duplicate = true;
            else rsn_ad.valid = parse_advertisement(body, size, false, rsn_ad);
        } else if (tag == 221 && size >= 4 &&
                   body[0] == 0x00 && body[1] == 0x50 && body[2] == 0xf2) {
            if (body[3] == 1) {
                if (wpa_ad.seen) duplicate = true;
                else wpa_ad.valid = parse_advertisement(body + 4, size - 4, true, wpa_ad);
            } else if (body[3] == 4) wps = true;
        } else if (tag == 244) {
            if (rsnxe) duplicate = true;
            else { rsnxe = body; rsnxe_length = size; }
        }
        pos += size;
    }
    if (rsn_ad.seen && !rsn_ad.valid) info.summary = "Malformed RSN";
    else if (wpa_ad.seen && !wpa_ad.valid) info.summary = "Malformed WPA IE";
    else if (duplicate) info.summary = "Conflicting security IEs";
    else if ((rsn_ad.seen || wpa_ad.seen) && !privacy) info.summary = "Inconsistent privacy flag";
    else if (rsn_ad.seen) {
        const bool mfpr = rsn_ad.has_capabilities && (rsn_ad.capabilities & 0x40);
        const bool mfpc = rsn_ad.has_capabilities && (rsn_ad.capabilities & 0x80);
        if (mfpr && !mfpc) info.summary = "Invalid RSN PMF flags";
        else {
            info.summary = rsn_class(rsn_ad, mfpr, mfpc);
            if (wpa_ad.seen) info.summary = legacy_class(wpa_ad) + " + " + info.summary;
        }
        info.pmf = mfpr ? "Required" : mfpc ? "Capable" : "Not advertised";
        info.akm = list_names(rsn_ad.akms, true);
        info.pairwise = list_names(rsn_ad.pairwise, false);
        info.group = suite_name(rsn_ad.group, false);
        info.group_management = rsn_ad.management ? suite_name(rsn_ad.management, false) : "Not advertised";
        if (known_cipher(rsn_ad.group) == nullptr ||
            std::any_of(rsn_ad.pairwise.begin(), rsn_ad.pairwise.end(),
                        [](Suite value) { return known_cipher(value) == nullptr; }) ||
            (rsn_ad.management && known_cipher(rsn_ad.management) == nullptr))
            info.note = "Unknown cipher selector";
    } else if (wpa_ad.seen) info.summary = legacy_class(wpa_ad);
    else info.summary = privacy ? "Legacy privacy (WEP?)" : "Open";

    if (wpa_ad.seen && wpa_ad.valid) {
        info.legacy_wpa = legacy_class(wpa_ad) + " [" + list_names(wpa_ad.akms, true) +
                          "; " + list_names(wpa_ad.pairwise, false) + "]";
        if (!rsn_ad.seen) {
            info.akm = list_names(wpa_ad.akms, true);
            info.pairwise = list_names(wpa_ad.pairwise, false);
            info.group = suite_name(wpa_ad.group, false);
            info.pmf = "Not advertised";
        }
    }
    if (rsnxe) {
        if (!rsnxe_length || (rsnxe[0] & 0x0f) + 1 != rsnxe_length) info.rsnxe = "Malformed RSNXE";
        else {
            const bool h2e = (rsnxe[0] & 0x20) != 0;
            const bool pk = (rsnxe[0] & 0x40) != 0;
            info.rsnxe = h2e ? "SAE H2E" : "No SAE H2E flag";
            if (pk) info.rsnxe += ", SAE-PK";
        }
    } else info.rsnxe = "Not advertised";
    if (wps) info.note += (info.note.empty() ? "" : "; ") + std::string("WPS IE present");
    if (truncated) info.note += (info.note.empty() ? "" : "; ") + std::string("Truncated IEs");
    if (duplicate) info.note += (info.note.empty() ? "" : "; ") + std::string("Duplicate security IE");
    return info;
}
} // namespace platform::wireless
