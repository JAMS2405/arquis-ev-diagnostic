#pragma once
/**
 * UART Analyzer — PASSIVE, receive-only (Stage 2).
 *
 * Observes a serial line without ever driving it:
 *   - Only RX pins exist in config/pins_config.h; no TX pin is assigned.
 *   - UARTs are opened with txPin = -1, so the TX pad is never attached (checked in
 *     the Arduino-ESP32 2.0.17 core source).
 *   - Pins are configured as INPUT only. This module never calls write/print on a
 *     port, never sets a pin as output and never calls digitalWrite
 *     (tools/check_ble_write_policy.py enforces it).
 *
 * ELECTRICAL: the input must go through the protected divider described in
 * docs/UART_TAP.md. Never connect a vehicle signal directly to a GPIO.
 *
 * Not validated on hardware yet — see docs/TESTING.md.
 */

#include <Arduino.h>

namespace uart_analyzer {

enum class Mode : uint8_t { Idle, Detecting, Capturing };

enum Format : uint8_t { k8N1 = 0, k8E1 = 1, k8O1 = 2 };

struct Status {
    Mode mode;
    uint32_t secs_left;
    uint32_t bytes;
    uint32_t frames;
    bool truncated;
    int channel;                // 0 = A, 1 = B (last used)
    uint32_t baud;              // baud of the last/current capture
    uint32_t last_detect_baud;  // best candidate of the last detection (0 if none)
};

/// Allocates buffers (PSRAM when available) and starts the capture task. Call once.
bool begin();

/// Measures edge timing on channel 0 (A) or 1 (B) for `window_ms` and estimates the baud rate.
bool startDetect(int channel, uint32_t window_ms, char* error, size_t error_len);

/// Captures bytes on the channel at `baud`, splits frames by idle gap and, at the end,
/// appends an analysis (dominant frame, varying bytes, checksum).
bool startCapture(int channel, uint32_t baud, int format, uint32_t secs, uint32_t gap_ms, char* error,
                  size_t error_len);

/// Ends a detection or capture now.
void stop();

/// Call from loop(): finishes detections and timed-out captures.
void poll();

Status status();
bool isActive();

String textFrom(size_t offset, size_t max_chars);
size_t textSize();

}  // namespace uart_analyzer
