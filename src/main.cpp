/**
 * ARQUIS EV DIAGNOSTIC
 *
 * Stage 1: boot, system report, ILI9341 status screen.
 * Stage 3 (early): BLE analyzer controlled from the serial monitor.
 *
 * Serial commands (115200 baud, either USB connector):
 *   help           list commands
 *   info           print the system report
 *   scan [secs]    BLE scan (default 8 s, max 30 s), list on serial and TFT
 *   list           reprint the last scan result
 *   gatt <index>   connect to device <index> of the last scan and print its GATT
 *                  tree, reading ONLY readable characteristics. NEVER writes.
 *   listen <index> [secs]  LISTEN-ONLY: subscribe to the device's notifications and log
 *                  what it sends. Sends NO data to the device. Own devices only.
 *   stoplisten     end a listen session
 *   uartdetect [A|B] [ms]            estimate the baud of a UART line (RECEIVE-ONLY)
 *   uartcap <A|B> <baud> [secs] [8N1|8E1|8O1] [gap_ms]   capture + analyze frames (RECEIVE-ONLY)
 *   uartstop       end a UART detection/capture
 *   jbd <index>    JBD BMS report. Sends 3 fixed standard READ requests (opt-in,
 *                  src/bms). Only for batteries you own.
 *
 * UART analyzer / electrical monitor / storage / diagnostics are NOT
 * implemented yet. Nothing here transmits to or measures any vehicle signal;
 * BLE is the only radio activity and it is read-only.
 */

#include <Arduino.h>

#include "ble/ble_analyzer.h"
#include "ble/ble_listen.h"
#include "uart/uart_analyzer.h"
#include "bms/jbd_bms.h"
#include "config/console.h"
#include "config/project_config.h"
#include "display/display.h"
#include "panel_text.h"
#include "web/web_server.h"

#if defined(SET_LOOP_TASK_STACK_SIZE)
SET_LOOP_TASK_STACK_SIZE(16 * 1024);  // GATT exploration may run inside an HTTP handler
#endif

namespace {

constexpr unsigned long kBootScreenMs    = 1500;
constexpr unsigned long kStatusRefreshMs = 1000;
constexpr unsigned long kHeartbeatMs     = 30000;
constexpr unsigned long kScanListMs     = 15000;
constexpr uint32_t kDefaultScanSecs      = 8;
constexpr uint32_t kMaxScanSecs          = 30;

bool g_webOk                  = false;
bool g_listenStream           = false;
bool g_uartStream             = false;
size_t g_uartOffset           = 0;
size_t g_listenOffset         = 0;
unsigned long g_scanListUntil = 0;  // while > 0 a serial-triggered scan list is on screen
unsigned long g_lastRefresh   = 0;
unsigned long g_lastHeartbeat = 0;
bool g_displayOk              = false;
bool g_bleOk                  = false;
String g_line;

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
    auto& out = console::out();
    out.printf("%s v%s\n", d.project_name, d.fw_version);
    out.printf("Chip:  %s rev %d, %d core(s)\n", ESP.getChipModel(), ESP.getChipRevision(),
               ESP.getChipCores());
    out.printf("Flash: %lu bytes\n", (unsigned long)d.flash_size_bytes);
    if (d.psram_total_bytes == 0) {
        out.println("PSRAM: NOT FOUND");
    } else {
        out.printf("PSRAM: %lu bytes\n", (unsigned long)d.psram_total_bytes);
    }
    out.printf("Heap:  %lu bytes free\n", (unsigned long)d.heap_free_bytes);
}

void printHelp() {
    auto& out = console::out();
    out.println("Commands:");
    out.println("  help          this list");
    out.println("  info          system report");
    out.println("  scan [secs]   BLE scan (default 8, max 30)");
    out.println("  list          reprint last scan");
    out.println("  gatt <index>  read-only GATT exploration of a scanned device");
    out.println("  listen <i> [s] listen-only: log what the device sends (no data is sent to it)");
    out.println("  stoplisten    end the listen session");
    out.println("  uartdetect [A|B] [ms]  estimate baud on a UART line (receive-only)");
    out.println("  uartcap <A|B> <baud> [secs] [8N1|8E1|8O1] [gap_ms]  capture + analyze (receive-only)");
    out.println("  uartstop      end a UART detection/capture");
    out.println("  jbd <index>   JBD BMS report (sends 3 standard READ requests; own batteries only)");
    out.println("  wifi          show the web UI access (SSID, password, URL)");
}

void showScanOnDisplay() {
    if (!g_displayOk) return;
    static char buf[27][41];
    static const char* lines[27];
    int n = 0;
    const int total = ble_analyzer::deviceCount();
    for (int i = 0; i < total && n < 27; i++) {
        const ble_analyzer::DeviceInfo* d = ble_analyzer::device(i);
        snprintf(buf[n], sizeof(buf[n]), "%2d %4d %.30s", i, d->rssi,
                 d->name[0] ? d->name : d->address);
        lines[n] = buf[n];
        n++;
    }
    char title[24];
    snprintf(title, sizeof(title), "BLE: %d dev", total);
    display::showTextList(title, lines, n);
    g_scanListUntil = millis() + kScanListMs;  // then the dashboard comes back by itself
}

// Builds the dashboard from the live state of every module and repaints what changed.
void refreshDashboard() {
    if (!g_displayOk) return;
    const display::StatusData sys = collectStatus();

    panel_text::BleState ble{};
    const ble_listen::Status ls = ble_listen::status();
    ble.listening        = ls.active;
    ble.listen_secs_left = ls.secs_left;
    ble.notifications    = ls.notifications;
    ble.bytes            = ls.bytes;
    ble.scanning         = ble_analyzer::isScanning();
    ble.devices          = ble_analyzer::deviceCount();

    const uart_analyzer::Status us = uart_analyzer::status();
    panel_text::UartState uart{};
    uart.mode = us.mode == uart_analyzer::Mode::Detecting ? 1
                : (us.mode == uart_analyzer::Mode::Capturing ? 2 : 0);
    uart.channel          = us.channel;
    uart.baud             = us.baud;
    uart.secs_left        = us.secs_left;
    uart.frames           = us.frames;
    uart.bytes            = us.bytes;
    uart.last_detect_baud = us.last_detect_baud;

    panel_text::BmsState bms{};
    const jbd_bms::Report* r = jbd_bms::last();
    if (r && r->have_basic) {
        bms.have             = true;
        bms.voltage_cv       = r->basic.total_voltage_cv;
        bms.cycles           = r->basic.cycles;
        bms.rsoc             = r->basic.rsoc_percent;
        bms.protection_flags = r->basic.protection_flags;
        if (r->have_cells && r->cells.count > 0) bms.delta_mv = jbd::computeCellStats(r->cells).delta_mv;
    }

    const int clients = g_webOk ? web::clients() : 0;
    panel_text::HintState hint{};
    hint.clients   = clients;
    hint.ble_busy  = ble.scanning || ble.listening;
    hint.uart_busy = uart.mode != 0;

    static char bleT[32], bleD[64], uartT[32], uartD[64], bmsT[32], bmsD[64], hintS[64];
    static char sys1[64], sys2[64], url[40];
    char up[16], heap[16], psram[16], flash[16];
    panel_text::bleTitle(bleT, sizeof(bleT), ble);
    panel_text::bleDetail(bleD, sizeof(bleD), ble);
    panel_text::uartTitle(uartT, sizeof(uartT), uart);
    panel_text::uartDetail(uartD, sizeof(uartD), uart);
    panel_text::bmsTitle(bmsT, sizeof(bmsT), bms);
    panel_text::bmsDetail(bmsD, sizeof(bmsD), bms);
    panel_text::hint(hintS, sizeof(hintS), hint);

    panel_text::uptime(up, sizeof(up), sys.uptime_s);
    panel_text::bytesText(heap, sizeof(heap), sys.heap_free_bytes);
    panel_text::bytesText(psram, sizeof(psram), sys.psram_total_bytes);
    panel_text::bytesText(flash, sizeof(flash), sys.flash_size_bytes);
    snprintf(sys1, sizeof(sys1), "Activo %s  Heap %s", up, heap);
    snprintf(sys2, sizeof(sys2), "PSRAM %s  Flash %s", psram, flash);
    if (g_webOk) snprintf(url, sizeof(url), "%s", web::url().c_str());
    else snprintf(url, sizeof(url), "Wi-Fi apagado");

    display::Dashboard d{};
    d.ssid        = g_webOk ? web::ssid() : "(sin Wi-Fi)";
    d.password    = g_webOk ? web::password() : "";
    d.url         = url;
    d.clients     = clients;
    d.ble_title   = bleT;
    d.ble_detail  = bleD;
    d.ble_active  = ble.scanning || ble.listening;
    d.uart_title  = uartT;
    d.uart_detail = uartD;
    d.uart_active = uart.mode != 0;
    d.bms_title   = bmsT;
    d.bms_detail  = bmsD;
    d.bms_alert   = bms.have && bms.protection_flags != 0;
    d.hint        = hintS;
    d.sys1        = sys1;
    d.sys2        = sys2;
    d.fw_version  = ARQUIS_FW_VERSION;
    display::updateDashboard(d);
}

void restoreStatusScreen() {
    if (!g_displayOk) return;
    g_scanListUntil = 0;
    display::showDashboardLayout(ARQUIS_FW_VERSION);
    refreshDashboard();
}

void cmdScan(uint32_t secs) {
    auto& out = console::out();
    if (!g_bleOk) {
        out.println("BLE not available");
        return;
    }
    out.printf("Scanning %lu s...\n", (unsigned long)secs);
    ble_analyzer::scan(secs * 1000UL);
    ble_analyzer::printList(out);
    showScanOnDisplay();
}

void handleCommand(const String& line) {
    auto& out = console::out();
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0) return;

    if (ble_listen::isActive() && cmd != "stoplisten" && cmd != "help" && cmd != "info" &&
        cmd != "wifi") {
        out.println("A listen session is running. Type 'stoplisten' first.");
        out.println("> ready");
        return;
    }

    if (cmd == "help") {
        printHelp();
    } else if (cmd == "info") {
        reportSystemOverSerial(collectStatus());
    } else if (cmd.startsWith("scan")) {
        uint32_t secs = kDefaultScanSecs;
        const String arg = cmd.substring(4);
        if (arg.length() > 0) {
            const long v = arg.toInt();
            if (v > 0) secs = (v > (long)kMaxScanSecs) ? kMaxScanSecs : (uint32_t)v;
        }
        cmdScan(secs);
    } else if (cmd == "uartstop") {
        uart_analyzer::stop();
    } else if (cmd.startsWith("uartdetect") || cmd.startsWith("uartcap")) {
        const bool isCap = cmd.startsWith("uartcap");
        String rest = cmd.substring(isCap ? 7 : 10);
        rest.trim();
        // tokenise by spaces
        String tok[6];
        int nt = 0;
        while (rest.length() > 0 && nt < 6) {
            const int sp = rest.indexOf(' ');
            tok[nt++] = sp < 0 ? rest : rest.substring(0, sp);
            rest = sp < 0 ? String("") : rest.substring(sp + 1);
            rest.trim();
        }
        const int ch = (nt > 0 && (tok[0] == "B" || tok[0] == "b")) ? 1 : 0;
        char err[64] = "";
        bool ok = false;
        if (!isCap) {
            const long ms = nt > 1 ? tok[1].toInt() : 2000;
            ok = uart_analyzer::startDetect(ch, ms > 0 ? (uint32_t)ms : 2000, err, sizeof(err));
        } else if (nt < 2) {
            out.println("usage: uartcap <A|B> <baud> [secs] [8N1|8E1|8O1] [gap_ms]");
        } else {
            const long baud = tok[1].toInt();
            const long secs = nt > 2 ? tok[2].toInt() : 30;
            int fmt = 0;
            if (nt > 3 && (tok[3] == "8E1" || tok[3] == "8e1")) fmt = 1;
            if (nt > 3 && (tok[3] == "8O1" || tok[3] == "8o1")) fmt = 2;
            const long gap = nt > 4 ? tok[4].toInt() : 5;
            ok = uart_analyzer::startCapture(ch, baud > 0 ? (uint32_t)baud : 0, fmt,
                                             secs > 0 ? (uint32_t)secs : 30, gap > 0 ? (uint32_t)gap : 5,
                                             err, sizeof(err));
        }
        if (ok) {
            g_uartStream = true;
            g_uartOffset = 0;
            return;  // loop streams the log and prints '> ready' at the end
        }
        if (err[0]) out.printf("uart failed: %s\n", err);
    } else if (cmd == "stoplisten") {
        ble_listen::stop();
    } else if (cmd.startsWith("listen")) {
        const String arg = cmd.substring(6);
        int idx = -1;
        long secs = 30;
        const int sp = arg.indexOf(' ', 1);
        if (arg.length() > 0) {
            idx = (int)arg.toInt();
            if (sp > 0) secs = arg.substring(sp).toInt();
        }
        if (arg.length() == 0) {
            out.println("usage: listen <index> [secs]  (own devices only; sends NO data)");
        } else if (!g_bleOk) {
            out.println("BLE not available");
        } else {
            char err[48] = "";
            if (ble_listen::start(idx, secs > 0 ? (uint32_t)secs : 30, err, sizeof(err))) {
                g_listenStream = true;
                g_listenOffset = 0;
                out.println("Listening (type 'stoplisten' to end)...");
                return;  // the loop streams the log; it prints '> ready' at the end
            }
            out.printf("listen failed: %s\n", err);
        }
    } else if (cmd.startsWith("jbd")) {
        const String arg = cmd.substring(3);
        if (arg.length() == 0) {
            out.println("usage: jbd <index>  (see 'scan' / 'list'). Own batteries only.");
        } else if (!g_bleOk) {
            out.println("BLE not available");
        } else {
            static jbd_bms::Report report;
            jbd_bms::readReport((int)arg.toInt(), report);
            jbd_bms::printText(report, out);
        }
    } else if (cmd == "wifi") {
        if (g_webOk) {
            out.printf("Web UI: SSID=%s  password=%s  url=%s\n", web::ssid(), web::password(),
                       web::url().c_str());
        } else {
            out.println("Web UI not running");
        }
    } else if (cmd == "list") {
        ble_analyzer::printList(out);
    } else if (cmd.startsWith("gatt")) {
        const String arg = cmd.substring(4);
        if (arg.length() == 0) {
            out.println("usage: gatt <index>  (see 'scan' / 'list')");
        } else if (!g_bleOk) {
            out.println("BLE not available");
        } else {
            ble_analyzer::exploreGatt((int)arg.toInt(), out);
        }
    } else {
        out.printf("unknown command: %s (try 'help')\n", cmd.c_str());
    }
    out.println("> ready");
    restoreStatusScreen();
    // keep scan view if the user just scanned
    if (cmd.startsWith("scan") || cmd == "list") showScanOnDisplay();
}

// Streams the listen log to the console while a session runs, then prints the prompt.
void streamListen() {
    if (!g_listenStream) return;
    const String t = ble_listen::textFrom(g_listenOffset, 400);
    if (t.length() > 0) {
        console::out().print(t);
        g_listenOffset += t.length();
    } else if (!ble_listen::isActive()) {
        g_listenStream = false;
        console::out().println("> ready");
        restoreStatusScreen();
    }
}

// Streams the UART analyzer log to the console while it runs.
void streamUart() {
    if (!g_uartStream) return;
    const String t = uart_analyzer::textFrom(g_uartOffset, 400);
    if (t.length() > 0) {
        console::out().print(t);
        g_uartOffset += t.length();
    } else if (!uart_analyzer::isActive()) {
        g_uartStream = false;
        console::out().println("> ready");
    }
}

void pollSerialCommands() {
    auto& con = console::out();
    while (con.available() > 0) {
        const int c = con.read();
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (g_line.length() > 0) {
                handleCommand(g_line);
                g_line = "";
            }
        } else if (g_line.length() < 64) {
            g_line += (char)c;
        }
    }
}

}  // namespace

void setup() {
    console::out().begin(config::SERIAL_BAUD);
    delay(500);

    const display::StatusData status = collectStatus();
    reportSystemOverSerial(status);

    g_displayOk = display::begin();
    if (g_displayOk) {
        display::showBoot(ARQUIS_PROJECT_NAME, ARQUIS_FW_VERSION);
    } else {
        console::out().println("Display: init FAILED");
    }

    console::out().println(uart_analyzer::begin() ? "UART analyzer: ready (receive-only)" : "UART analyzer: init FAILED");
    g_bleOk = ble_analyzer::begin();
    console::out().println(g_bleOk ? "BLE: ready (read-only analyzer)" : "BLE: init FAILED");

    g_webOk = g_bleOk && web::begin();
    if (g_webOk) {
        console::out().printf("Web UI: SSID=%s  password=%s (%s)  url=%s\n", web::ssid(), web::password(),
                              web::passwordIsFixed() ? "fixed" : "random, changes every boot",
                              web::url().c_str());
    } else {
        console::out().println("Web UI: NOT started");
    }

    if (g_displayOk) {
        delay(kBootScreenMs);
        display::showDashboardLayout(ARQUIS_FW_VERSION);
        refreshDashboard();
    }
    printHelp();
    console::out().println("> ready");
}

void loop() {
    pollSerialCommands();
    web::loop();
    ble_listen::poll();
    uart_analyzer::poll();
    streamListen();
    streamUart();

    if (g_displayOk && millis() - g_lastRefresh >= kStatusRefreshMs) {
        g_lastRefresh = millis();
        if (g_scanListUntil != 0) {
            // A serial-triggered scan list is showing: bring the dashboard back after a while.
            if ((int32_t)(millis() - g_scanListUntil) >= 0) restoreStatusScreen();
        } else {
            refreshDashboard();
        }
    }
    if (millis() - g_lastHeartbeat >= kHeartbeatMs) {
        g_lastHeartbeat = millis();
        console::out().printf("[alive] uptime=%lus heap=%lu\n", millis() / 1000UL,
                              (unsigned long)ESP.getFreeHeap());
    }
}
