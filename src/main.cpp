/**
 * ARQUIS EV DIAGNOSTIC — Stage 0 boot skeleton.
 *
 * Intentionally minimal: prints an identification banner only.
 * No module (display, UART, BLE, electrical, storage, diagnostics) is
 * implemented yet. Implementation starts at Stage 1 with explicit approval.
 */

#include <Arduino.h>

#include "config/project_config.h"

void setup() {
    Serial.begin(config::SERIAL_BAUD);
    delay(500);
    Serial.printf("%s v%s\n", ARQUIS_PROJECT_NAME, ARQUIS_FW_VERSION);
    Serial.println("Stage 0: skeleton only. No modules implemented.");
}

void loop() {
    delay(1000);
}
