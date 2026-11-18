#include "web/web_server.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "ble/ble_analyzer.h"
#include "ble/ble_listen.h"
#include "bms/jbd_bms.h"
#include "config/project_config.h"
#include "web/web_page.h"

// Optional local secrets (git-ignored). If absent, a random password is used.
#if __has_include("config/secrets.h")
#include "config/secrets.h"
#define ARQUIS_HAVE_SECRETS 1
#endif

namespace web {

namespace {

constexpr const char* kSsid        = "ARQUIS-EV-DIAG";
constexpr int kChannel             = 6;
constexpr int kMaxClients          = 2;
constexpr uint32_t kMaxScanSecs    = 30;
constexpr uint32_t kDefaultScanSec = 8;
constexpr size_t kMaxGattTextBytes = 24 * 1024;

WebServer g_server(80);
char g_password[64];
bool g_started = false;

/// Print sink that captures text into a String (bounded).
class StringPrint : public Print {
public:
    String text;
    size_t write(uint8_t c) override {
        if (text.length() < kMaxGattTextBytes) text += static_cast<char>(c);
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) override {
        for (size_t i = 0; i < size; i++) write(buffer[i]);
        return size;
    }
};

void makePassword() {
#ifdef ARQUIS_HAVE_SECRETS
    const size_t n = strlen(secrets::WIFI_PASSWORD);
    if (n >= 8 && n <= 63) {
        strcpy(g_password, secrets::WIFI_PASSWORD);
        return;
    }
#endif
    // Random per-boot password (no ambiguous characters).
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    constexpr int kLen = 10;
    for (int i = 0; i < kLen; i++) {
        g_password[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    g_password[kLen] = '\0';
}

// JSON string escaping. BLE data is untrusted: anything that is not printable
// ASCII becomes '?', so the output is always valid JSON.
void appendJsonString(String& out, const char* s) {
    out += '"';
    for (; *s; s++) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c == '\n') {
            out += "\\n";  // valid JSON escape; the UI renders it with textContent
        } else if (c < 0x20 || c >= 0x7F) {
            out += '?';
        } else {
            out += static_cast<char>(c);
        }
    }
    out += '"';
}

void sendCommonHeaders() {
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.sendHeader("X-Content-Type-Options", "nosniff");
    g_server.sendHeader("Content-Security-Policy",
                        "default-src 'self'; script-src 'unsafe-inline'; "
                        "style-src 'unsafe-inline'; img-src data:");
}

void sendJson(int code, const String& body) {
    sendCommonHeaders();
    g_server.send(code, "application/json", body);
}

void handleIndex() {
    sendCommonHeaders();
    g_server.send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void handleStatus() {
    String j;
    j.reserve(160);
    j += "{\"fw\":";
    appendJsonString(j, ARQUIS_FW_VERSION);
    j += ",\"uptime_s\":" + String(millis() / 1000UL);
    j += ",\"heap\":" + String(ESP.getFreeHeap());
    j += ",\"scanning\":";
    j += ble_analyzer::isScanning() ? "true" : "false";
    j += ",\"devices\":" + String(ble_analyzer::deviceCount());
    j += "}";
    sendJson(200, j);
}

void handleScan() {
    if (g_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"POST required\"}");
        return;
    }
    uint32_t secs = kDefaultScanSec;
    if (g_server.hasArg("secs")) {
        const long v = g_server.arg("secs").toInt();
        if (v > 0) secs = (v > (long)kMaxScanSecs) ? kMaxScanSecs : (uint32_t)v;
    }
    if (ble_listen::isActive()) {
        sendJson(409, "{\"error\":\"listen session in progress\"}");
        return;
    }
    if (!ble_analyzer::startScan(secs * 1000UL)) {
        sendJson(409, "{\"error\":\"scan not started (already running or BLE unavailable)\"}");
        return;
    }
    sendJson(202, "{\"started\":true}");
}

void handleDevices() {
    String j;
    j.reserve(2048);
    j += "{\"devices\":[";
    const int n = ble_analyzer::deviceCount();
    for (int i = 0; i < n; i++) {
        const ble_analyzer::DeviceInfo* d = ble_analyzer::device(i);
        if (i) j += ',';
        j += "{\"i\":" + String(i);
        j += ",\"addr\":";
        appendJsonString(j, d->address);
        j += ",\"name\":";
        appendJsonString(j, d->name);
        j += ",\"rssi\":" + String(d->rssi);
        j += ",\"conn\":";
        j += d->connectable ? "true" : "false";
        j += ",\"svc\":";
        appendJsonString(j, d->services);
        j += ",\"mfr\":";
        appendJsonString(j, d->mfr_data);
        j += '}';
    }
    j += "]}";
    sendJson(200, j);
}

// Synchronous: the HTTP response is sent when the exploration finishes.
// READ-ONLY (see ble_analyzer::exploreGatt).
void handleGatt() {
    if (g_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"POST required\"}");
        return;
    }
    if (ble_analyzer::isScanning() || ble_listen::isActive()) {
        sendJson(409, "{\"error\":\"scan or listen session in progress\"}");
        return;
    }
    if (!g_server.hasArg("index")) {
        sendJson(400, "{\"error\":\"missing index\"}");
        return;
    }
    const int index = g_server.arg("index").toInt();
    if (index < 0 || index >= ble_analyzer::deviceCount()) {
        sendJson(400, "{\"error\":\"invalid index\"}");
        return;
    }

    StringPrint sink;
    ble_analyzer::exploreGatt(index, sink);
    sendCommonHeaders();
    g_server.send(200, "text/plain; charset=utf-8", sink.text);
}

// Synchronous. Sends 3 fixed READ requests to a JBD BMS (see bms/jbd_bms.h for the
// narrow write exception). Triggered only by an explicit button press in the UI.
void handleBms() {
    if (g_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"POST required\"}");
        return;
    }
    if (ble_analyzer::isScanning() || ble_listen::isActive()) {
        sendJson(409, "{\"error\":\"scan or listen session in progress\"}");
        return;
    }
    if (!g_server.hasArg("index")) {
        sendJson(400, "{\"error\":\"missing index\"}");
        return;
    }
    const int index = g_server.arg("index").toInt();
    if (index < 0 || index >= ble_analyzer::deviceCount()) {
        sendJson(400, "{\"error\":\"invalid index\"}");
        return;
    }
    static jbd_bms::Report report;
    jbd_bms::readReport(index, report);
    sendJson(200, jbd_bms::toJson(report));
}

// Listen-only mode (see ble/ble_listen.h): POST starts, GET polls, /stop ends.
void handleListen() {
    if (g_server.method() == HTTP_POST) {
        if (ble_analyzer::isScanning()) {
            sendJson(409, "{\"error\":\"scan in progress\"}");
            return;
        }
        if (!g_server.hasArg("index")) {
            sendJson(400, "{\"error\":\"missing index\"}");
            return;
        }
        const int index = g_server.arg("index").toInt();
        const long secs = g_server.hasArg("secs") ? g_server.arg("secs").toInt() : 30;
        char err[48] = "";
        if (!ble_listen::start(index, secs > 0 ? (uint32_t)secs : 30, err, sizeof(err))) {
            String j = "{\"error\":";
            appendJsonString(j, err);
            j += "}";
            sendJson(409, j);
            return;
        }
        sendJson(202, "{\"started\":true}");
        return;
    }
    if (g_server.method() != HTTP_GET) {
        sendJson(405, "{\"error\":\"GET or POST required\"}");
        return;
    }
    size_t offset = 0;
    if (g_server.hasArg("offset")) {
        const long v = g_server.arg("offset").toInt();
        if (v > 0) offset = (size_t)v;
    }
    const ble_listen::Status st = ble_listen::status();
    const String chunk = ble_listen::textFrom(offset, 3000);
    String j;
    j.reserve(chunk.length() + 160);
    j += "{\"active\":";
    j += st.active ? "true" : "false";
    j += ",\"secs_left\":" + String(st.secs_left);
    j += ",\"n\":" + String(st.notifications);
    j += ",\"bytes\":" + String(st.bytes);
    j += ",\"trunc\":";
    j += st.truncated ? "true" : "false";
    j += ",\"chunk\":";
    appendJsonString(j, chunk.c_str());
    j += "}";
    sendJson(200, j);
}

void handleListenStop() {
    if (g_server.method() != HTTP_POST) {
        sendJson(405, "{\"error\":\"POST required\"}");
        return;
    }
    ble_listen::stop();
    sendJson(200, "{\"stopped\":true}");
}

void handleNotFound() { sendJson(404, "{\"error\":\"not found\"}"); }

}  // namespace

bool begin() {
    makePassword();

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(kSsid, g_password, kChannel, 0, kMaxClients)) {
        return false;
    }

    g_server.on("/", HTTP_GET, handleIndex);
    g_server.on("/api/status", HTTP_GET, handleStatus);
    g_server.on("/api/scan", HTTP_ANY, handleScan);
    g_server.on("/api/devices", HTTP_GET, handleDevices);
    g_server.on("/api/gatt", HTTP_ANY, handleGatt);
    g_server.on("/api/bms", HTTP_ANY, handleBms);
    g_server.on("/api/listen", HTTP_ANY, handleListen);
    g_server.on("/api/listen/stop", HTTP_ANY, handleListenStop);
    g_server.onNotFound(handleNotFound);
    g_server.begin();

    g_started = true;
    return true;
}

void loop() {
    if (!g_started) return;
    g_server.handleClient();
    ble_analyzer::pollScan();
}

const char* ssid() { return kSsid; }
const char* password() { return g_password; }
String url() { return String("http://") + WiFi.softAPIP().toString(); }

}  // namespace web
