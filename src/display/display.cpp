#include "display/display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>

#include <string.h>

#include "config/pins_config.h"

namespace display {

namespace {

constexpr int16_t kWidth = 240;

// Colors (RGB565)
constexpr uint16_t kBg     = ILI9341_BLACK;
constexpr uint16_t kFg     = ILI9341_WHITE;
constexpr uint16_t kAccent = ILI9341_CYAN;
constexpr uint16_t kWarn   = ILI9341_YELLOW;
constexpr uint16_t kGood   = ILI9341_GREEN;
constexpr uint16_t kBad    = ILI9341_RED;
constexpr uint16_t kDim    = ILI9341_DARKGREY;

SPIClass g_spi(FSPI);
Adafruit_ILI9341 g_tft(&g_spi, pins::TFT_DC, pins::TFT_CS, pins::TFT_RST);

// ---- dashboard fields: repaint only on change ------------------------------

enum Field {
    F_CLIENTS,
    F_SSID,
    F_PASS,
    F_URL,
    F_BLE_T,
    F_BLE_D,
    F_UART_T,
    F_UART_D,
    F_BMS_T,
    F_BMS_D,
    F_HINT,
    F_SYS1,
    F_SYS2,
    F_COUNT
};

char g_text[F_COUNT][48];
uint16_t g_color[F_COUNT];
bool g_valid[F_COUNT];

// Draws `text` in the box (x, y, w, h) unless it is already there with that color.
void drawField(Field f, int16_t x, int16_t y, int16_t w, uint8_t size, uint16_t color, const char* text) {
    if (text == nullptr) text = "";
    if (g_valid[f] && g_color[f] == color && strncmp(g_text[f], text, sizeof(g_text[f]) - 1) == 0) return;

    g_tft.fillRect(x, y, w, 8 * size, kBg);
    g_tft.setTextSize(size);
    g_tft.setTextColor(color, kBg);
    g_tft.setCursor(x, y);
    g_tft.print(text);

    strncpy(g_text[f], text, sizeof(g_text[f]) - 1);
    g_text[f][sizeof(g_text[f]) - 1] = '\0';
    g_color[f] = color;
    g_valid[f] = true;
}

void label(int16_t x, int16_t y, uint16_t color, const char* text) {
    g_tft.setTextSize(1);
    g_tft.setTextColor(color, kBg);
    g_tft.setCursor(x, y);
    g_tft.print(text);
}

}  // namespace

bool begin() {
    pinMode(pins::TFT_BL, OUTPUT);
    digitalWrite(pins::TFT_BL, LOW);  // keep backlight off during init

    g_spi.begin(pins::TFT_SCLK, pins::TFT_MISO, pins::TFT_MOSI, pins::TFT_CS);
    g_tft.begin();
    g_tft.setRotation(0);      // portrait, 240x320
    g_tft.setTextWrap(false);  // never let long text wrap onto the next row
    g_tft.fillScreen(kBg);

    digitalWrite(pins::TFT_BL, HIGH);  // module-dependent; see docs/HARDWARE.md
    return true;                       // ILI9341 write-only: no readback of init success
}

void showBoot(const char* project_name, const char* fw_version) {
    g_tft.fillScreen(kBg);
    g_tft.setTextColor(kAccent, kBg);
    g_tft.setTextSize(3);
    g_tft.setCursor(10, 110);
    g_tft.print("ARQUIS");
    g_tft.setCursor(10, 140);
    g_tft.print("EV DIAG");
    g_tft.setTextSize(2);
    g_tft.setTextColor(kFg, kBg);
    g_tft.setCursor(10, 190);
    g_tft.print(fw_version);
    g_tft.setTextColor(kWarn, kBg);
    g_tft.setTextSize(1);
    g_tft.setCursor(10, 300);
    g_tft.print(project_name);
}

void showDashboardLayout(const char* fw_version) {
    g_tft.fillScreen(kBg);
    memset(g_valid, 0, sizeof(g_valid));  // force a full repaint of the dynamic fields

    // Title (size 2: 14 chars = 168 px)
    g_tft.setTextSize(2);
    g_tft.setTextColor(kAccent, kBg);
    g_tft.setCursor(4, 4);
    g_tft.print("ARQUIS EV DIAG");
    g_tft.drawFastHLine(0, 24, kWidth, kAccent);

    label(4, 28, kWarn, "WIFI");
    g_tft.drawFastHLine(0, 90, kWidth, kDim);

    g_tft.drawFastHLine(0, 210, kWidth, kDim);
    label(4, 216, kWarn, "SISTEMA");

    label(4, 292, kGood, "No envia ordenes a la scooter");
    char fw[40];
    snprintf(fw, sizeof(fw), "FW %s", fw_version ? fw_version : "?");
    label(4, 306, kDim, fw);
}

void updateDashboard(const Dashboard& d) {
    char buf[48];

    // ---- Wi-Fi ----
    snprintf(buf, sizeof(buf), "clientes: %d", d.clients);
    drawField(F_CLIENTS, 120, 28, 116, 1, d.clients > 0 ? kGood : kFg, buf);
    drawField(F_SSID, 4, 40, 232, 2, kFg, d.ssid);

    snprintf(buf, sizeof(buf), "Clave %s", d.password ? d.password : "");
    // Size 2 fits 20 characters; longer passwords drop to size 1 so they never overflow.
    const bool bigKey = strlen(buf) <= 19;
    drawField(F_PASS, 4, 58, 232, bigKey ? 2 : 1, kFg, buf);
    drawField(F_URL, 4, 78, 232, 1, kAccent, d.url);

    // ---- Activity ----
    drawField(F_BLE_T, 4, 96, 232, 2, d.ble_active ? kGood : kFg, d.ble_title);
    drawField(F_BLE_D, 4, 114, 232, 1, kDim, d.ble_detail);
    drawField(F_UART_T, 4, 130, 232, 2, d.uart_active ? kGood : kFg, d.uart_title);
    drawField(F_UART_D, 4, 148, 232, 1, kDim, d.uart_detail);
    drawField(F_BMS_T, 4, 164, 232, 2, d.bms_alert ? kBad : kFg, d.bms_title);
    drawField(F_BMS_D, 4, 182, 232, 1, d.bms_alert ? kBad : kDim, d.bms_detail);
    drawField(F_HINT, 4, 198, 232, 1, kWarn, d.hint);

    // ---- System ----
    drawField(F_SYS1, 4, 228, 232, 1, kFg, d.sys1);
    drawField(F_SYS2, 4, 240, 232, 1, kFg, d.sys2);
}

void showTextList(const char* title, const char* const* lines, int count) {
    constexpr int kMaxLines = 27;
    g_tft.fillScreen(kBg);
    g_tft.setTextColor(kAccent, kBg);
    g_tft.setTextSize(2);
    g_tft.setCursor(4, 4);
    g_tft.print(title);
    g_tft.drawFastHLine(0, 24, kWidth, kAccent);

    g_tft.setTextColor(kFg, kBg);
    g_tft.setTextSize(1);
    const int n = count < kMaxLines ? count : kMaxLines;
    for (int i = 0; i < n; i++) {
        g_tft.setCursor(2, 30 + i * 10);
        g_tft.print(lines[i]);
    }
}

}  // namespace display
