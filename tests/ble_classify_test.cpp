// SPDX-License-Identifier: AGPL-3.0-only
#include "ble_classify.h"
#include <cassert>

int main() {
    using namespace platform;
    using namespace platform::ble;
    BluetoothObservation item{"AA:BB:CC:DD:EE:FF", "", "random", -64};
    item.manufacturer_data = "004C:021500112233445566778899AABBCCDDEEFF00010002C5";
    auto result = classify(item);
    assert(result.matches & bit(Kind::IBeacon));
    assert(!(result.matches & bit(Kind::FindMy)));
    assert(result.vendor == "Apple");
    assert(result.detail == "iBeacon 1:2");
    item.manufacturer_data = "004C:12AABBCC";
    result = classify(item);
    assert(result.matches & bit(Kind::FindMy));
    item.manufacturer_data.clear();
    item.uuids = "0000FD44-0000-1000-8000-00805F9B34FB";
    assert(classify(item).matches & bit(Kind::FindMy));
    item.uuids.clear();
    item.advertising_data = "0026:8103";
    assert(!(classify(item).matches & bit(Kind::Flipper)));
    item.advertising_data = "0026:8130";
    assert(classify(item).matches & bit(Kind::Flipper));
    item.advertising_data.clear();
    item.manufacturer_data = "09C8:00";
    assert(classify(item).matches & bit(Kind::Flock));
    item.manufacturer_data = "004C:00,FD5F:00";
    assert(!(classify(item).matches & bit(Kind::Meta)));
    item.manufacturer_data = "FD5F:00";
    assert(classify(item).matches & bit(Kind::Meta));
    item.manufacturer_data.clear();
    item.name = "HC-05";
    assert(classify(item).matches & bit(Kind::SerialModule));
    const auto raw = decode_report("AA:BB:CC:DD:EE:FF", "random", -52,
        "0201061AFF4C00021500112233445566778899AABBCCDDEEFF00010002C5");
    assert(raw.raw_ad.size() == 60);
    assert(raw.manufacturer_data == "004C:021500112233445566778899AABBCCDDEEFF00010002C5");
    assert(classify(raw).matches & bit(Kind::IBeacon));
    const auto named = decode_report("AA:BB:CC:DD:EE:FF", "public", -61, "0709426561636F6E");
    assert(named.name == "Beacon");
    return 0;
}
