#pragma once
/**
 * ARQUIS EV DIAGNOSTIC — Central pin configuration.
 *
 * ***********************************************************************
 * STATUS: PROVISIONAL / UNVERIFIED
 * These assignments are a proposal derived from ESP32-S3 general
 * constraints. They MUST be verified against the physical board's
 * silkscreen/schematic before wiring or flashing anything that drives
 * these pins. Do not treat as final. See docs/PINOUT.md.
 * ***********************************************************************
 *
 * Pins deliberately avoided on ESP32-S3-N16R8 (octal PSRAM + 16 MB flash):
 *   GPIO26..37  : flash / octal PSRAM (33..37 octal PSRAM on N16R8)
 *   GPIO19, 20  : native USB D-/D+
 *   GPIO0, 3, 45, 46 : strapping pins
 *   GPIO43, 44  : default UART0 TX/RX (console)
 *
 * Passive UART analyzer: ONLY RX pins are defined on purpose. No TX pin is
 * assigned in v1, so the analyzer cannot transmit toward the vehicle.
 */

namespace pins {

// ---- TFT ILI9341 2.4" (SPI, no touch) — PROVISIONAL ----
constexpr int TFT_SCLK = 12;
constexpr int TFT_MOSI = 11;
constexpr int TFT_MISO = 13;  // Optional; many ILI9341 modules do not need readback
constexpr int TFT_CS   = 10;
constexpr int TFT_DC   = 9;
constexpr int TFT_RST  = 8;
constexpr int TFT_BL   = 14;  // Backlight control (module-dependent)

// ---- UART Analyzer (RX only, passive) — PROVISIONAL ----
// Signals must be level-checked (multimeter) and adapted to 3.3 V before use.
constexpr int UART_SNIFF_A_RX = 15;
constexpr int UART_SNIFF_B_RX = 16;

// ---- Electrical Monitor (ADC1 only, behind protection) — PROVISIONAL ----
// ADC1 channels chosen so they remain usable with radio (BLE) active.
// Inputs must come through protected conditioning; never raw battery voltage.
constexpr int ADC_IN_0 = 1;
constexpr int ADC_IN_1 = 2;
constexpr int ADC_IN_2 = 4;

}  // namespace pins
