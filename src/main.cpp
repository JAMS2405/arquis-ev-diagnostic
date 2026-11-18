/**
 * ARQUIS EV DIAGNOSTIC — Stage 1: firmware base and display.
 *
 * Boots, reports flash/PSRAM over serial, and shows a boot screen followed by
 * a read-only system status screen on the ILI9341 TFT.
 *
 * UART / BLE / electrical / storage / diagnostics modules are NOT implemented
 * yet (Stages 2-5). Nothing here transmits to or measures any vehicle signal.
 */

#include <Arduino.h>

#include "config/console.h"
#include "config/project_config.h"
#include "display/display.h"

namespace {

constexpr unsigned long kBootScreenMs      = 1500;
constexpr unsigned long kStatusRefreshMs   = 1000;
constexpr unsigned long kHeartbeatMs       = 5000;
unsigned long g_lastRefresh   = 0;
unsigned long g_lastHeartbeat = 0;

display::StatusData collectStatus() {
    display::StatusData d;
    d.project_name      = ARQUIS_PROJECT_NAME;
    d.fw_version        = ARQUIS_FW_VERSION;
    d.uptime_s          = millis() / 1000UL;
    d.heap_free_bytes   = ESP.getFreeHeap();
    d.psram_total_bytes = psramFound() ? ESP.getPsramSize() : 0;
    d.flash_size_bytes  = ESP.getFlashChipSize();
    return d;
}

void reportSystemOverSerial(const display::StatusData& d) {
    console::out().printf("%s v%s\n", d.project_name, d.fw_version);
    console::out().printf("Chip:  %s rev %d, %d core(s)\n", ESP.getChipModel(), ESP.getChipRevision(),
                  ESP.getChipCores());
    console::out().printf("Flash: %lu bytes\n", (unsigned long)d.flash_size_bytes);
    if (d.psram_total_bytes == 0) {
        console::out().println("PSRAM: NOT FOUND");
    } else {
        console::out().printf("PSRAM: %lu bytes\n", (unsigned long)d.psram_total_bytes);
    }
    console::out().printf("Heap:  %lu bytes free\n", (unsigned long)d.heap_free_bytes);
}

}  // namespace

void setup() {
    console::out().begin(config::SERIAL_BAUD);
    delay(500);

    const display::StatusData status = collectStatus();
    reportSystemOverSerial(status);

    if (!display::begin()) {
        console::out().println("Display: init FAILED");
        return;
    }
    display::showBoot(ARQUIS_PROJECT_NAME, ARQUIS_FW_VERSION);
    delay(kBootScreenMs);
    display::showStatusLayout(status);
}

void loop() {
    if (millis() - g_lastRefresh >= kStatusRefreshMs) {
        g_lastRefresh = millis();
        display::updateStatus(collectStatus());
    }
    if (millis() - g_lastHeartbeat >= kHeartbeatMs) {
        g_lastHeartbeat = millis();
        reportSystemOverSerial(collectStatus());
    }
}
