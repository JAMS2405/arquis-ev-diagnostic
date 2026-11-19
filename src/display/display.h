#pragma once
/**
 * Display module — ILI9341 2.4" TFT (240x320, SPI, no touch).
 *
 * - begin(), showBoot(): initialization and splash.
 * - Dashboard: a live panel (Wi-Fi access, BLE / UART / BMS activity, system).
 *   The caller builds the strings (see lib/panel_text, which guarantees they fit);
 *   this module only draws them and repaints ONLY the fields that changed.
 * - showTextList(): a plain list (used for serial-triggered BLE scan results).
 *
 * Pins come exclusively from config/pins_config.h. Hardware-validated (init, text,
 * orientation) — see docs/TESTING.md. The dashboard layout itself is not yet
 * validated on hardware.
 */

#include <stdint.h>

namespace display {

struct StatusData {
    const char* project_name;
    const char* fw_version;
    uint32_t uptime_s;
    uint32_t heap_free_bytes;
    uint32_t psram_total_bytes;  // 0 when PSRAM is not detected
    uint32_t flash_size_bytes;
};

/// Everything the dashboard shows. Strings must stay valid during the call.
struct Dashboard {
    // Wi-Fi access
    const char* ssid;
    const char* password;
    const char* url;
    int clients;
    // Activity (title = size 2, <= 19 chars; detail = size 1, <= 39 chars)
    const char* ble_title;
    const char* ble_detail;
    bool ble_active;
    const char* uart_title;
    const char* uart_detail;
    bool uart_active;
    const char* bms_title;
    const char* bms_detail;
    bool bms_alert;
    const char* hint;  // <= 39 chars
    // System (size 1, <= 39 chars)
    const char* sys1;
    const char* sys2;
    const char* fw_version;
};

/// Initializes SPI and the ILI9341 controller. Returns false on failure.
bool begin();

/// Draws the boot/splash screen.
void showBoot(const char* project_name, const char* fw_version);

/// Clears the screen and draws the static parts of the dashboard. Forces the next
/// updateDashboard() to repaint every field.
void showDashboardLayout(const char* fw_version);

/// Repaints only the fields whose text or color changed since the last call.
void updateDashboard(const Dashboard& d);

/// Draws a titled list of short text lines (size 1 font, up to 40 chars/line,
/// up to 27 lines).
void showTextList(const char* title, const char* const* lines, int count);

}  // namespace display
