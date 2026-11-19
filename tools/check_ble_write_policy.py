#!/usr/bin/env python3
"""Enforces the project's safety policies by static checks on src/.

POLICY 1 - BLE writes (below). POLICY 2 - the UART analyzer is passive:
src/uart/ must never transmit: no write/print on a port, no output pins, no pull
resistors, no direct IDF UART/GPIO output calls, and every UART begin() must pass
txPin = -1 so the TX pad is never attached.

Policy 1: BLE writes.

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

# ---------------------------------------------------------------- policy 2 ----
UART_FORBIDDEN = [
    (re.compile(r"(->|\.)\s*(write|print|println|printf|flush|writeBytes)\s*\("),
     "transmit call on a port"),
    (re.compile(r"\bdigitalWrite\s*\("), "digitalWrite"),
    (re.compile(r"\bpinMode\s*\([^;]*\b(OUTPUT|OPEN_DRAIN|INPUT_PULLUP|INPUT_PULLDOWN|OUTPUT_OPEN_DRAIN)\b"),
     "pin configured as output or with a pull resistor"),
    (re.compile(r"\b(uart_write_bytes\w*|uart_tx_chars|uart_set_pin|gpio_set_level|gpio_set_direction|"
                r"gpio_pullup_en|gpio_pulldown_en|pinMatrixOutAttach)\s*\("),
     "direct IDF call that can drive or bias a pin"),
]
UART_BEGIN = re.compile(r"(->|\.)\s*begin\s*\(([^;]*)\)\s*;")

uart_bad = []
for path in sorted(pathlib.Path("src/uart").rglob("*")):
    if path.suffix not in {".cpp", ".h", ".hpp", ".c"}:
        continue
    code = strip_comments(path.read_text(encoding="utf-8"))
    for n, line in enumerate(code.splitlines(), 1):
        for rx, why in UART_FORBIDDEN:
            if rx.search(line):
                uart_bad.append(f"{path}:{n}: {why}: {line.strip()}")
        m = UART_BEGIN.search(line)
        if m and not re.search(r",\s*-1\s*$", m.group(2).strip()):
            uart_bad.append(f"{path}:{n}: UART begin() must end with txPin = -1: {line.strip()}")

failed = False
if bad:
    print("BLE write policy VIOLATION:")
    print("\n".join(bad))
    failed = True
if uart_bad:
    print("UART passive policy VIOLATION:")
    print("\n".join(uart_bad))
    failed = True
if failed:
    sys.exit(1)
print("BLE write policy OK. Exceptions: " + "; ".join(
    f"{p} -> {sorted(a)}" for p, a in ALLOWED.items()))
print("UART passive policy OK: src/uart has no transmit, output-pin or pull-resistor calls; begin() uses txPin = -1.")
