#pragma once
/**
 * BLE Analyzer — Stage 3.
 *
 * READ-ONLY by design:
 *   - Scans advertising devices (passive observation of what they broadcast).
 *   - Optionally connects to ONE chosen device and enumerates its GATT
 *     services/characteristics, reading ONLY characteristics that declare the
 *     READ property.
 *   - It NEVER writes characteristics or descriptors, NEVER subscribes to
 *     notifications/indications (that writes the CCCD), and NEVER pairs.
 *
 * Nothing here is validated on hardware yet — see docs/TESTING.md.
 */

#include <Print.h>
#include <stdint.h>

namespace ble_analyzer {

constexpr int kMaxDevices = 40;

struct DeviceInfo {
    char address[18];       // "aa:bb:cc:dd:ee:ff"
    uint8_t address_type;   // 0 = public, 1 = random (NimBLE convention)
    char name[33];          // empty if the device does not advertise a name
    int rssi;
    bool connectable;
    char services[97];      // advertised service UUIDs, comma separated (truncated)
    char mfr_data[65];      // manufacturer data as hex (truncated)
};

/// Initializes the BLE stack. Call once.
bool begin();

/// Active scan for `duration_ms`. Replaces the stored list (sorted by RSSI,
/// strongest first). Blocking. Returns the number of devices found.
int scan(uint32_t duration_ms);

/// Non-blocking scan: starts an active scan and returns immediately.
/// Call pollScan() regularly; the stored list is replaced when it finishes.
bool startScan(uint32_t duration_ms);
bool isScanning();
/// Stops a running scan (results so far are kept). Safe if no scan is running.
void stopScan();
/// Finalizes a finished async scan. Returns true once, when new results landed.
bool pollScan();

int deviceCount();
const DeviceInfo* device(int index);

/// Prints the stored device list.
void printList(Print& out);

/// Connects to device `index` of the stored list, prints its GATT tree and the
/// values of readable characteristics, then disconnects. READ-ONLY.
/// Returns false if the connection failed.
bool exploreGatt(int index, Print& out);

}  // namespace ble_analyzer
