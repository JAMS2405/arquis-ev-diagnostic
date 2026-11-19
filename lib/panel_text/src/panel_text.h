#pragma once
/**
 * Text for the TFT dashboard (ILI9341 240x320, default 6x8 font, ASCII only).
 *
 * Titles are drawn at text size 2 (12 px/char -> 19 chars from x = 4);
 * details at text size 1 (6 px/char -> 39 chars from x = 4). Every builder
 * truncates safely and NUL-terminates, and the tests check that extreme inputs
 * still fit, so text can never overflow the screen.
 */

#include <stddef.h>
#include <stdint.h>

namespace panel_text {

// Fields start at x = 4 px on a 240 px screen: (240 - 4) / 12 = 19 chars at size 2,
// (240 - 4) / 6 = 39 chars at size 1.
constexpr size_t kTitleMax  = 19;  // size-2 characters per line
constexpr size_t kDetailMax = 39;  // size-1 characters per line

struct BleState {
    bool scanning;
    bool listening;
    uint32_t listen_secs_left;
    uint32_t notifications;
    uint32_t bytes;
    int devices;  // devices found by the last scan
};

struct UartState {
    int mode;  // 0 idle, 1 detecting, 2 capturing
    int channel;  // 0 = A, 1 = B
    uint32_t baud;
    uint32_t secs_left;
    uint32_t frames;
    uint32_t bytes;
    uint32_t last_detect_baud;  // 0 if none
};

struct BmsState {
    bool have;
    uint32_t voltage_cv;  // 0.01 V
    uint16_t cycles;
    uint8_t rsoc;
    uint16_t delta_mv;
    uint16_t protection_flags;
};

struct HintState {
    int clients;
    bool ble_busy;
    bool uart_busy;
};

void bleTitle(char* out, size_t n, const BleState& s);
void bleDetail(char* out, size_t n, const BleState& s);
void uartTitle(char* out, size_t n, const UartState& s);
void uartDetail(char* out, size_t n, const UartState& s);
void bmsTitle(char* out, size_t n, const BmsState& s);
void bmsDetail(char* out, size_t n, const BmsState& s);
void hint(char* out, size_t n, const HintState& s);

/// "HH:MM:SS" (hours are not limited to two digits).
void uptime(char* out, size_t n, uint32_t secs);

/// "512 B", "285 KB" or "7.99 MB".
void bytesText(char* out, size_t n, uint32_t bytes);

}  // namespace panel_text
