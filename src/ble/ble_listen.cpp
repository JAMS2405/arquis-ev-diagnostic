#include "ble/ble_listen.h"

#include <NimBLEDevice.h>

#include <string.h>

#include "ble/ble_analyzer.h"
#include "notify_log.h"

namespace ble_listen {

namespace {

constexpr size_t kLogCap              = 32 * 1024;
constexpr uint32_t kMinSecs           = 5;
constexpr uint32_t kMaxSecs           = 120;
constexpr uint32_t kConnectTimeoutMs  = 10000;
constexpr size_t kMaxBytesShown       = 64;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
char g_log[kLogCap];
size_t g_len = 0;
bool g_truncated = false;
notify_log::Stats g_stats;

NimBLEClient* g_client = nullptr;
bool g_active = false;
uint32_t g_t0 = 0;
uint32_t g_endMs = 0;

void setError(char* error, size_t len, const char* msg) {
    if (error && len) {
        strncpy(error, msg, len - 1);
        error[len - 1] = '\0';
    }
}

// Appends text to the log; marks truncation when the buffer is full.
void appendLog(const char* text, size_t n) {
    portENTER_CRITICAL(&g_mux);
    if (g_len + n < kLogCap) {
        memcpy(g_log + g_len, text, n);
        g_len += n;
        g_log[g_len] = '\0';
    } else {
        g_truncated = true;
    }
    portEXIT_CRITICAL(&g_mux);
}

void appendLogStr(const char* text) { appendLog(text, strlen(text)); }

// Runs in the NimBLE host task. Only records what the device sent.
void onNotify(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool) {
    const uint32_t t = millis() - g_t0;
    char tag[12];
    const std::string uuid = chr->getUUID().toString();
    strncpy(tag, uuid.c_str(), sizeof(tag) - 1);
    tag[sizeof(tag) - 1] = '\0';

    char line[kMaxBytesShown * 4 + 64];
    const size_t n = notify_log::formatLine(line, sizeof(line), t, tag, data, len, kMaxBytesShown);

    portENTER_CRITICAL(&g_mux);
    notify_log::add(g_stats, data, len, t);
    portEXIT_CRITICAL(&g_mux);
    appendLog(line, n);
}

void finish(const char* reason) {
    if (!g_active) return;
    g_active = false;

    char buf[160];
    notify_log::Stats s;
    portENTER_CRITICAL(&g_mux);
    s = g_stats;
    portEXIT_CRITICAL(&g_mux);

    snprintf(buf, sizeof(buf), "--- end: %s ---\n", reason);
    appendLogStr(buf);
    snprintf(buf, sizeof(buf), "notifications: %lu, bytes: %lu, length min/max: %u/%u B, avg interval: %lu ms\n",
             (unsigned long)s.count, (unsigned long)s.bytes, s.min_len, s.max_len,
             (unsigned long)notify_log::averageIntervalMs(s));
    appendLogStr(buf);

    notify_log::TopByte top[3];
    const size_t n = notify_log::topFirstBytes(s, top, 3);
    if (n > 0) {
        appendLogStr("most frequent first bytes:");
        for (size_t i = 0; i < n; i++) {
            snprintf(buf, sizeof(buf), " %02X(x%u)", top[i].value, top[i].count);
            appendLogStr(buf);
        }
        appendLogStr("\n");
    } else {
        appendLogStr("The device sent nothing while we listened. It may only talk after a request.\n");
    }

    if (g_client) {
        if (g_client->isConnected()) g_client->disconnect();  // drops the subscriptions
        NimBLEDevice::deleteClient(g_client);
        g_client = nullptr;
    }
}

}  // namespace

bool start(int index, uint32_t secs, char* error, size_t error_len) {
    if (g_active) {
        setError(error, error_len, "already listening");
        return false;
    }
    const ble_analyzer::DeviceInfo* info = ble_analyzer::device(index);
    if (!info) {
        setError(error, error_len, "invalid device index");
        return false;
    }
    if (!info->connectable) {
        setError(error, error_len, "device is not connectable");
        return false;
    }
    if (secs < kMinSecs) secs = kMinSecs;
    if (secs > kMaxSecs) secs = kMaxSecs;

    portENTER_CRITICAL(&g_mux);
    g_len = 0;
    g_log[0] = '\0';
    g_truncated = false;
    notify_log::reset(g_stats);
    portEXIT_CRITICAL(&g_mux);

    NimBLEClient* client = NimBLEDevice::createClient();
    client->setConnectTimeout(kConnectTimeoutMs);
    const NimBLEAddress addr(std::string(info->address), info->address_type);
    if (!client->connect(addr)) {
        setError(error, error_len, "connection failed");
        NimBLEDevice::deleteClient(client);
        return false;
    }

    char buf[160];
    snprintf(buf, sizeof(buf), "LISTEN-ONLY: no data is sent to the device. Target: \"%s\"\n",
             info->name[0] ? info->name : "(no name)");
    appendLogStr(buf);

    int subscribed = 0;
    g_t0 = millis();  // set before subscribing: notifications may arrive immediately
    for (NimBLERemoteService* svc : client->getServices(true)) {
        const std::string svcUuid = svc->getUUID().toString();
        if (strcasecmp(svcUuid.c_str(), "0x1801") == 0) continue;  // Generic Attribute
        for (NimBLERemoteCharacteristic* ch : svc->getCharacteristics(true)) {
            if (!ch->canNotify()) continue;
            if (ch->subscribe(true, onNotify)) {
                subscribed++;
                snprintf(buf, sizeof(buf), "subscribed: %s / %s\n", svcUuid.c_str(),
                         ch->getUUID().toString().c_str());
                appendLogStr(buf);
            }
        }
    }
    if (subscribed == 0) {
        appendLogStr("no NOTIFY characteristics could be subscribed\n");
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        setError(error, error_len, "no notify characteristics");
        return false;
    }

    g_client = client;
    g_t0 = millis();
    g_endMs = g_t0 + secs * 1000UL;
    g_active = true;
    snprintf(buf, sizeof(buf), "listening for %lu s...\n", (unsigned long)secs);
    appendLogStr(buf);
    return true;
}

void poll() {
    if (!g_active) return;
    if (g_client == nullptr || !g_client->isConnected()) {
        finish("device disconnected");
    } else if ((int32_t)(millis() - g_endMs) >= 0) {
        finish("time elapsed");
    }
}

void stop() { finish("stopped by user"); }

bool isActive() { return g_active; }

Status status() {
    Status st{};
    st.active = g_active;
    if (g_active) {
        const int32_t left = (int32_t)(g_endMs - millis());
        st.secs_left = left > 0 ? (uint32_t)(left / 1000) : 0;
    }
    portENTER_CRITICAL(&g_mux);
    st.notifications = g_stats.count;
    st.bytes = g_stats.bytes;
    st.truncated = g_truncated;
    portEXIT_CRITICAL(&g_mux);
    return st;
}

String textFrom(size_t offset, size_t max_chars) {
    String out;
    portENTER_CRITICAL(&g_mux);
    if (offset < g_len) {
        size_t n = g_len - offset;
        if (n > max_chars) n = max_chars;
        out.reserve(n);
        for (size_t i = 0; i < n; i++) out += g_log[offset + i];
    }
    portEXIT_CRITICAL(&g_mux);
    return out;
}

size_t textSize() { return g_len; }

}  // namespace ble_listen
