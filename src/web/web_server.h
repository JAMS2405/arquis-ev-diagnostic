#pragma once
/**
 * Web UI — Wi-Fi access point + small HTTP server for use from a phone.
 *
 * READ-ONLY with respect to the vehicle: the API only starts a BLE scan,
 * lists results and runs the read-only GATT exploration of ble_analyzer.
 * There is no endpoint that writes to the vehicle, no OTA, no file upload.
 *
 * Not validated on hardware yet — see docs/TESTING.md.
 */

#include <Arduino.h>

namespace web {

/// Starts the access point and the HTTP server. Call after ble_analyzer::begin().
bool begin();

/// Services HTTP clients and finalizes async BLE scans. Call from loop().
void loop();

const char* ssid();
const char* password();
String url();

}  // namespace web
