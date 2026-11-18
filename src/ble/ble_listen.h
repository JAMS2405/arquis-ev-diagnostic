#pragma once
/**
 * BLE listen-only mode.
 *
 * Connects to ONE chosen device, subscribes to its NOTIFY characteristics and
 * logs everything the device transmits, WITHOUT sending it any data.
 *
 * Narrow, user-approved exception to "BLE is read-only":
 *   - The only write is the standard CCCD write that enables notifications
 *     (what any app does to receive data).
 *   - It NEVER calls writeValue(): no byte is ever sent to a characteristic.
 *   - Triggered only by an explicit user action, for a device the user owns.
 *
 * tools/check_ble_write_policy.py allows `subscribe` here and forbids
 * `writeValue` in this file.
 *
 * Not validated on hardware yet — see docs/TESTING.md.
 */

#include <Arduino.h>

namespace ble_listen {

struct Status {
    bool active;
    uint32_t secs_left;
    uint32_t notifications;
    uint32_t bytes;
    bool truncated;
};

/// Connects to scanned device `index` and starts listening for `secs` seconds
/// (clamped to 5..120). Returns false and fills `error` on failure. The connection
/// step blocks for a few seconds; capturing itself is non-blocking.
bool start(int index, uint32_t secs, char* error, size_t error_len);

/// Must be called regularly (loop): ends the session on timeout or disconnect.
void poll();

/// Ends the session now.
void stop();

bool isActive();
Status status();

/// Captured log text from byte offset `offset`, at most `max_chars` characters.
String textFrom(size_t offset, size_t max_chars);
size_t textSize();

}  // namespace ble_listen
