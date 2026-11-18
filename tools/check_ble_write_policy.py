#!/usr/bin/env python3
"""Enforces the BLE write policy.

The project's BLE code is read-only, with audited exceptions, each limited to the
calls listed here:

  src/bms/jbd_bms.cpp    : writeValue + subscribe  (3 fixed JBD register-read
                           requests, built only by jbd::buildReadRequest())
  src/ble/ble_listen.cpp : subscribe ONLY          (listen-only: enables
                           notifications; never sends data to a characteristic)

Any other file calling these functions fails the check. Run from the repo root:
    python3 tools/check_ble_write_policy.py
"""
import pathlib
import re
import sys

ALL_GUARDED = ["writeValue", "writeDescriptor", "subscribe", "unsubscribe",
               "setNotify", "secureConnection", "startSecurity"]
ALLOWED = {
    pathlib.Path("src/bms/jbd_bms.cpp"): {"writeValue", "subscribe", "unsubscribe"},
    pathlib.Path("src/ble/ble_listen.cpp"): {"subscribe"},
}

def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)

bad = []
for path in sorted(pathlib.Path("src").rglob("*")):
    if path.suffix not in {".cpp", ".h", ".hpp", ".c"}:
        continue
    allowed = ALLOWED.get(path, set())
    forbidden = [n for n in ALL_GUARDED if n not in allowed]
    pattern = re.compile(r"\b(" + "|".join(forbidden) + r")\s*\(")
    for n, line in enumerate(strip_comments(path.read_text(encoding="utf-8")).splitlines(), 1):
        if pattern.search(line):
            bad.append(f"{path}:{n}: {line.strip()}")

if bad:
    print("BLE write policy VIOLATION:")
    print("\n".join(bad))
    sys.exit(1)
print("BLE write policy OK. Exceptions: " + "; ".join(
    f"{p} -> {sorted(a)}" for p, a in ALLOWED.items()))
