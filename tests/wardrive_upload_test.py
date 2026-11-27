import csv
import os
from pathlib import Path
import runpy
import tempfile
from unittest import mock

module = runpy.run_path(str(Path(__file__).resolve().parents[1] / "src/platform/helpers/wardrive-upload"))


def test_conversion():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        source, target = root / "wardrive_1.csv", root / "output.csv"
        source.write_text("utc,radio,address,name,channel,rssi_dbm,security,address_type,latitude,longitude,frequency_mhz\n"
                          '2026-11-27T12:00:00Z,WiFi,"AA:BB:CC:DD:EE:FF","Cafe, East",5,-42,WPA3,,40.5,-73.5,5975\n'
                          '2026-11-27T12:00:01Z,Bluetooth,"11:22:33:44:55:66",Sensor,,-70,,random,40.5,-73.5,\n'
                          '2026-11-27T12:00:02Z,WiFi,"AA:BB:CC:DD:EE:00",Invalid,6,-50,WPA2,,999,-73.5,2437\n')
        assert module["convert"](source, target) == 2
        with target.open(newline="") as file:
            assert file.readline().startswith("WigleWifi-1.6,")
            rows = list(csv.DictReader(file))
        assert rows[0]["SSID"] == "Cafe, East" and rows[0]["Frequency"] == "5975"
        assert rows[0]["Type"] == "WIFI" and rows[1]["Type"] == "BLE"

        legacy = root / "legacy.csv"
        legacy.write_text("utc,radio,address,name,channel,rssi_dbm,security,address_type,latitude,longitude\n"
                          '2026-11-27T12:00:00Z,WiFi,"AA:BB:CC:DD:EE:FF",Home,6,-42,WPA2,,40.5,-73.5\n')
        assert module["convert"](legacy, target) == 1
        with target.open(newline="") as file:
            file.readline()
            row = next(csv.DictReader(file))
        assert row["Frequency"] == "2437"

        oldest = root / "oldest.csv"
        oldest.write_text("utc,bssid,ssid,channel,rssi_dbm,security,latitude,longitude\n"
                          '2026-11-27T12:00:00Z,"AA:BB:CC:DD:EE:FF",Old,11,-60,WPA2,40.5,-73.5\n')
        assert module["convert"](oldest, target) == 1
        with target.open(newline="") as file:
            file.readline()
            row = next(csv.DictReader(file))
        assert row["SSID"] == "Old" and row["Frequency"] == "2462"


def test_server_status():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        config = root / ".config" / "marauder-cardputerzero"
        config.mkdir(parents=True)
        (config / "wigle-api-name").write_text("tester")
        (config / "wigle-api-token").write_text("token")
        (config / "wdgwars-api-key").write_text("key")
        source = root / "upload.csv"
        source.write_text("data")

        class Response:
            status = 200
            def read(self, _): return b'{"success":true}'

        class Connection:
            def __init__(self, host, timeout): self.host = host
            def putrequest(self, *_): pass
            def putheader(self, *_): pass
            def endheaders(self): pass
            def send(self, *_): pass
            def getresponse(self): return Response()
            def close(self): pass

        with mock.patch.dict(os.environ, {"XDG_CONFIG_HOME": str(root / ".config")}), \
             mock.patch.object(module["http"].client, "HTTPSConnection", Connection):
            assert module["upload_file"](source, "wigle", root) == "uploaded"
            Response.status = 202
            Response.read = lambda self, _: b'{"ok":true,"job_id":"123"}'
            assert module["upload_file"](source, "wdg", root) == "accepted"


def test_failed_status():
    with tempfile.TemporaryDirectory() as directory:
        home = Path(directory)
        root = home / "Documents" / "Marauder" / "gps"
        root.mkdir(parents=True)
        source = root / "wardrive_20261127_120000.csv"
        source.write_text("utc,radio,address,name,channel,rssi_dbm,security,address_type,latitude,longitude\n"
                          '2026-11-27T12:00:00Z,WiFi,"AA:BB:CC:DD:EE:FF",Example,6,-42,WPA2,,40.5,-73.5\n')
        with mock.patch.object(Path, "home", return_value=home), \
             mock.patch.dict(module["main"].__globals__, {"upload_file": mock.Mock(side_effect=ValueError("Rejected"))}):
            try:
                module["main"](["wardrive-upload", str(source), "wigle"])
                assert False, "Expected rejection"
            except ValueError:
                pass
        assert (root / (source.name + ".upload")).read_text().split()[2:] == ["failed", "pending"]


if __name__ == "__main__":
    test_conversion()
    test_server_status()
    test_failed_status()
