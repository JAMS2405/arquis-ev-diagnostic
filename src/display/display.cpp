#include "display/display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <SPI.h>

#include "config/pins_config.h"

namespace display {

namespace {

constexpr int16_t kWidth  = 240;
constexpr int16_t kHeight = 320;

// Colors (RGB565)
constexpr uint16_t kBg     = ILI9341_BLACK;
constexpr uint16_t kFg     = ILI9341_WHITE;
constexpr uint16_t kAccent = ILI9341_CYAN;
constexpr uint16_t kWarn   = ILI9341_YELLOW;

// Layout of the status screen
constexpr int16_t kValueX    = 88;  // fits 12 chars at size 2 (e.g. "0.1.0-stage1")
constexpr int16_t kRowHeight = 24;
constexpr int16_t kRowsTop   = 60;

char g_netSsid[24] = "";
char g_netPass[24] = "";
char g_netUrl[32]  = "";

SPIClass g_spi(FSPI);
Adafruit_ILI9341 g_tft(&g_spi, pins::TFT_DC, pins::TFT_CS, pins::TFT_RST);

void formatBytes(char* out, size_t out_len, uint32_t bytes) {
    if (bytes >= 1024UL * 1024UL) {
        snprintf(out, out_len, "%lu.%02lu MB", (unsigned long)(bytes / (1024UL * 1024UL)),
                 (unsigned long)((bytes % (1024UL * 1024UL)) * 100UL / (1024UL * 1024UL)));
    } else {
        snprintf(out, out_len, "%lu KB", (unsigned long)(bytes / 1024UL));
    }
}

void formatUptime(char* out, size_t out_len, uint32_t s) {
    snprintf(out, out_len, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600UL),
             (unsigned long)((s / 60UL) % 60UL), (unsigned long)(s % 60UL));
}

// Draws a value on a status row, overwriting the previous one (bg color set).
void drawValue(int row, const char* text, uint16_t color) {
    const int16_t y = kRowsTop + row * kRowHeight;
    g_tft.fillRect(kValueX, y, kWidth - kValueX, 16, kBg);
    g_tft.setTextColor(color, kBg);
    g_tft.setTextSize(2);
    g_tft.setCursor(kValueX, y);
    g_tft.print(text);
}

void drawLabel(int row, const char* text) {
    const int16_t y = kRowsTop + row * kRowHeight;
    g_tft.setTextColor(kFg, kBg);
    g_tft.setTextSize(2);
    g_tft.setCursor(4, y);
    g_tft.print(text);
}

}  // namespace

bool begin() {
    pinMode(pins::TFT_BL, OUTPUT);
    digitalWrite(pins::TFT_BL, LOW);  // keep backlight off during init

    g_spi.begin(pins::TFT_SCLK, pins::TFT_MISO, pins::TFT_MOSI, pins::TFT_CS);
    g_tft.begin();
    g_tft.setRotation(0);  // portrait, 240x320
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

void showStatusLayout(const StatusData& data) {
    g_tft.fillScreen(kBg);
    g_tft.setTextColor(kAccent, kBg);
    g_tft.setTextSize(2);
    g_tft.setCursor(4, 8);
    g_tft.print("ARQUIS EV DIAG");  // short title: the full name does not fit at size 2
    g_tft.drawFastHLine(0, 30, kWidth, kAccent);

    g_tft.setTextColor(kWarn, kBg);
    g_tft.setTextSize(1);
    g_tft.setCursor(4, 40);
    g_tft.print("STAGE 1 - SYSTEM STATUS");

    if (g_netSsid[0]) {
        g_tft.drawFastHLine(0, 178, kWidth, kAccent);
        g_tft.setTextColor(kWarn, kBg);
        g_tft.setTextSize(1);
        g_tft.setCursor(4, 188);
        g_tft.print("PHONE WEB - connect to WiFi:");
        g_tft.setTextColor(kFg, kBg);
        g_tft.setTextSize(2);
        g_tft.setCursor(4, 204);
        g_tft.print(g_netSsid);
        g_tft.setCursor(4, 228);
        g_tft.print("Key ");
        g_tft.print(g_netPass);
        g_tft.setTextSize(1);
        g_tft.setTextColor(kAccent, kBg);
        g_tft.setCursor(4, 256);
        g_tft.print(g_netUrl);
    }

    drawLabel(0, "FW");
    drawLabel(1, "Uptime");
    drawLabel(2, "Heap");
    drawLabel(3, "PSRAM");
    drawLabel(4, "Flash");

    updateStatus(data);
}

void updateStatus(const StatusData& data) {
    char buf[24];

    drawValue(0, data.fw_version, kFg);

    formatUptime(buf, sizeof(buf), data.uptime_s);
    drawValue(1, buf, kFg);

    formatBytes(buf, sizeof(buf), data.heap_free_bytes);
    drawValue(2, buf, kFg);

    if (data.psram_total_bytes == 0) {
        drawValue(3, "NOT FOUND", ILI9341_RED);
    } else {
        formatBytes(buf, sizeof(buf), data.psram_total_bytes);
        drawValue(3, buf, kFg);
    }

    formatBytes(buf, sizeof(buf), data.flash_size_bytes);
    drawValue(4, buf, kFg);
}

void setNetworkInfo(const char* ssid, const char* password, const char* url) {
    strncpy(g_netSsid, ssid, sizeof(g_netSsid) - 1);
    strncpy(g_netPass, password, sizeof(g_netPass) - 1);
    strncpy(g_netUrl, url, sizeof(g_netUrl) - 1);
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
