// SPDX-License-Identifier: AGPL-3.0-only
#include "censor_text.h"
#include <cassert>

int main() {
    using namespace model::censor;
    assert(name("HomeNetwork") == "HomXXXXXXXX");
    assert(name("Ab") == "AX");
    assert(name("A") == "X");
    assert(name("[hidden/unknown]") == "[hidden/unknown]");
    assert(name("Caf\xC3\xA9-Guest") == "CafXXXXXXX");
    assert(mac("12:34:56:AB:CD:EF") == "12:34:XX:XX:XX:XX");
    assert(identifier("12-34-56-AB-CD-EF") == "12-34-XX-XX-XX-XX");
    assert(identifier("WiFi-Home") == "WiFXXXXXX");
    assert(coordinate("40.712776") == "40.XXXXXX");
    assert(coordinate("-73.935242") == "-73.XXXXXX");
    assert(capture_name("ap_123456ABCDEF_20261126_150000.pcap") == "ap_1234XXXXXXXX_20261126_150000.pcap");
    assert(capture_name("capture_20261126_150000.pcap") == "capture_20261126_150000.pcap");
    assert(macs_in_text("Bluetooth 12:34:56:AB:CD:EF ready") == "Bluetooth 12:34:XX:XX:XX:XX ready");
}
