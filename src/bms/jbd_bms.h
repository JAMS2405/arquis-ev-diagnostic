#pragma once
/**
 * JBD BMS report over BLE — Stage 3c (opt-in, scope extended by the user).
 *
 * THIS IS THE ONLY MODULE THAT WRITES TO A BLE DEVICE. The exception to the
 * project's "BLE is read-only" rule is deliberately narrow:
 *   - Writes go ONLY to characteristic 0xFF02 of service 0xFF00, and ONLY frames
 *     produced by jbd::buildReadRequest() (3 fixed register READ requests:
 *     0x03 basic info, 0x04 cell voltages, 0x05 hardware name).
 *   - The only other write is the CCCD subscription on 0xFF01 (needed to receive
 *     the replies).
 *   - There is no API to send arbitrary bytes. Configuration, MOSFET control and
 *     factory-mode frames cannot be built (see lib/jbd_protocol).
 *   - It is triggered only by an explicit user action (serial `jbd <n>` or the
 *     "Reporte BMS" button) and should be used only on batteries the user owns.
 *
 * tools/check_ble_write_policy.py enforces that no other file calls
 * writeValue/subscribe.
 *
 * Not validated on a real BMS yet — see docs/TESTING.md.
 */

#include <Arduino.h>

#include "jbd_protocol.h"

namespace jbd_bms {

struct Report {
    char address[18];
    char device_name[33];
    char error[72];          // empty when no error
    bool have_name;
    bool have_basic;
    bool have_cells;
    char hardware_name[48];
    jbd::BasicInfo basic;
    jbd::CellVoltages cells;
};

/// Connects to scanned device `index`, sends the three read requests, and fills
/// `out`. Blocking (several seconds). Returns true if at least the basic info
/// was obtained.
bool readReport(int index, Report& out);

/// Last report obtained (successful or not), or nullptr if none yet.
const Report* last();

/// Human-readable report (serial console).
void printText(const Report& r, Print& out);

/// JSON for the web UI.
String toJson(const Report& r);

}  // namespace jbd_bms
