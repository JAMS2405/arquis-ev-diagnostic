#!/usr/bin/env python3
"""Fails if any BLE write/subscribe call exists outside the one allowed module.

Policy: the project's BLE code is read-only, with a single audited exception:
src/bms/jbd_bms.cpp (3 fixed JBD register-read requests + notification
subscription). Run from the repository root:  python3 tools/check_ble_write_policy.py
"""
import pathlib
import re
import sys

ALLOWED = {pathlib.Path("src/bms/jbd_bms.cpp")}
FORBIDDEN = re.compile(r"\b(writeValue|writeDescriptor|subscribe|setNotify|secureConnection|startSecurity)\s*\(")

def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)

bad = []
for path in sorted(pathlib.Path("src").rglob("*")):
    if path.suffix not in {".cpp", ".h", ".hpp", ".c"}:
        continue
    if path in ALLOWED:
        continue
    for n, line in enumerate(strip_comments(path.read_text(encoding="utf-8")).splitlines(), 1):
        if FORBIDDEN.search(line):
            bad.append(f"{path}:{n}: {line.strip()}")

if bad:
    print("BLE write policy VIOLATION:")
    print("\n".join(bad))
    sys.exit(1)
print("BLE write policy OK: no write/subscribe calls outside", ", ".join(map(str, ALLOWED)))
