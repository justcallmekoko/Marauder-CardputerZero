#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
from pathlib import Path
import ast
import re
import time

source = Path(__file__).resolve().parents[1] / "src/platform/helpers/gps-session"
tree = ast.parse(source.read_text())
parts = [node for node in tree.body if isinstance(node, (ast.ClassDef, ast.FunctionDef)) and
         node.name in ("FixStatus", "nmea_coordinate", "fix")]
helper = {"re": re, "time": time}
exec(compile(ast.Module(body=parts, type_ignores=[]), str(source), "exec"), helper)
status = helper["FixStatus"]()

def sentence(body):
    checksum = 0
    for byte in body.encode("ascii"):
        checksum ^= byte
    return f"${body}*{checksum:02X}".encode("ascii")

rmc = sentence("GPRMC,123519,A,4807.038,N,01131.000,E,0,0,010126,,,A")
value = helper["fix"](rmc, status)
assert value == "Fix\t481173000\t115166667", value
assert helper["fix"](sentence("GPRMC,123520,V,4807.038,N,01131.000,E,0,0,010126,,,A"), status) == "No fix"
assert status.position is None
gga = sentence("GPGGA,123521,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")
assert helper["fix"](gga, status) == "8 sat\t481173000\t115166667"
bad = bytearray(rmc)
bad[-1] = ord("0") if bad[-1] != ord("0") else ord("1")
assert helper["fix"](bytes(bad), status) is None
