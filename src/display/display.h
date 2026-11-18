#pragma once
/**
 * Display module — ILI9341 2.4" TFT (240x320, SPI, no touch).
 *
 * Stage 1 scope: initialization, boot screen and a read-only system status
 * screen. No navigation/input handling (input hardware is not defined yet).
 *
 * Pins come exclusively from config/pins_config.h (provisional, unverified).
 * Not validated on physical hardware — see docs/TESTING.md.
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

/// Initializes SPI and the ILI9341 controller. Returns false on failure.
bool begin();

/// Draws the boot/splash screen.
void showBoot(const char* project_name, const char* fw_version);

/// Draws the static layout of the status screen (call once).
void showStatusLayout(const StatusData& data);

/// Updates only the dynamic values of the status screen (cheap, no flicker).
void updateStatus(const StatusData& data);

}  // namespace display
