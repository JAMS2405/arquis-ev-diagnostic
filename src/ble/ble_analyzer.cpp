#include "ble/ble_analyzer.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <algorithm>
#include <string>

namespace ble_analyzer {

namespace {

DeviceInfo g_devices[kMaxDevices];
int g_count = 0;
bool g_initialized = false;

constexpr size_t kMaxReadBytes = 64;       // bytes of a characteristic value we keep
constexpr int kMaxCharsPerDevice = 64;     // safety bound while exploring
constexpr uint32_t kConnectTimeoutMs = 10000;

void copyStr(char* dst, size_t dst_len, const std::string& src) {
    strncpy(dst, src.c_str(), dst_len - 1);
    dst[dst_len - 1] = '\0';
}

void hexOf(const uint8_t* data, size_t len, char* out, size_t out_len) {
    size_t pos = 0;
    for (size_t i = 0; i < len && pos + 3 < out_len; i++) {
        pos += snprintf(out + pos, out_len - pos, "%02X", data[i]);
    }
    out[pos < out_len ? pos : out_len - 1] = '\0';
}

void printHexSpaced(Print& out, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) out.printf("%02X ", data[i]);
}

void printAscii(Print& out, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) out.print((data[i] >= 32 && data[i] < 127) ? (char)data[i] : '.');
}

}  // namespace

bool begin() {
    if (g_initialized) return true;
    NimBLEDevice::init("ARQUIS-EV-DIAG");
    g_initialized = true;
    return true;
}

namespace {

bool g_asyncScanActive = false;

void configureScanner(NimBLEScan* scanner) {
    scanner->setActiveScan(true);  // request scan responses (names, extra data)
    scanner->setInterval(100);
    scanner->setWindow(99);
}

// Copies the scanner's current results into g_devices (sorted by RSSI).
void collectResults(NimBLEScan* scanner) {
    NimBLEScanResults results = scanner->getResults();
    g_count = 0;

    const int found = results.getCount();
    for (int i = 0; i < found && g_count < kMaxDevices; i++) {
        const NimBLEAdvertisedDevice* d = results.getDevice(i);
        DeviceInfo& info = g_devices[g_count];
        memset(&info, 0, sizeof(info));

        copyStr(info.address, sizeof(info.address), d->getAddress().toString());
        info.address_type = d->getAddress().getType();
        if (d->haveName()) copyStr(info.name, sizeof(info.name), d->getName());
        info.rssi = d->getRSSI();
        info.connectable = d->isConnectable();

        std::string services;
        for (size_t s = 0; s < d->getServiceUUIDCount(); s++) {
            if (!services.empty()) services += ",";
            services += d->getServiceUUID(s).toString();
        }
        copyStr(info.services, sizeof(info.services), services);

        if (d->haveManufacturerData()) {
            const std::string m = d->getManufacturerData();
            hexOf(reinterpret_cast<const uint8_t*>(m.data()), m.size(), info.mfr_data,
                  sizeof(info.mfr_data));
        }
        g_count++;
    }
    scanner->clearResults();

    std::sort(g_devices, g_devices + g_count,
              [](const DeviceInfo& a, const DeviceInfo& b) { return a.rssi > b.rssi; });
}

}  // namespace

int scan(uint32_t duration_ms) {
    if (!g_initialized) return 0;
    NimBLEScan* scanner = NimBLEDevice::getScan();
    configureScanner(scanner);
    scanner->clearResults();
    scanner->getResults(duration_ms, false);  // blocking
    collectResults(scanner);
    return g_count;
}

bool startScan(uint32_t duration_ms) {
    if (!g_initialized) return false;
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner->isScanning()) return false;
    configureScanner(scanner);
    scanner->clearResults();
    g_asyncScanActive = scanner->start(duration_ms, false);
    return g_asyncScanActive;
}

bool isScanning() { return g_initialized && NimBLEDevice::getScan()->isScanning(); }

void stopScan() {
    if (!g_initialized) return;
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner->isScanning()) scanner->stop();
}

bool pollScan() {
    if (!g_initialized || !g_asyncScanActive) return false;
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner->isScanning()) return false;
    g_asyncScanActive = false;
    collectResults(scanner);
    return true;
}

int deviceCount() { return g_count; }

const DeviceInfo* device(int index) {
    return (index >= 0 && index < g_count) ? &g_devices[index] : nullptr;
}

void printList(Print& out) {
    out.printf("--- %d device(s), strongest first ---\n", g_count);
    for (int i = 0; i < g_count; i++) {
        const DeviceInfo& d = g_devices[i];
        out.printf("[%2d] %s  RSSI %4d  %s  \"%s\"\n", i, d.address, d.rssi,
                   d.connectable ? "conn " : "noconn", d.name[0] ? d.name : "(no name)");
        if (d.services[0]) out.printf("      svc: %s\n", d.services);
        if (d.mfr_data[0]) out.printf("      mfr: %s\n", d.mfr_data);
    }
}

bool exploreGatt(int index, Print& out) {
    const DeviceInfo* info = device(index);
    if (!info) {
        out.println("GATT: invalid device index");
        return false;
    }
    if (!info->connectable) {
        out.println("GATT: device is not connectable");
        return false;
    }

    out.printf("GATT: connecting to %s \"%s\" (read-only)...\n", info->address, info->name);

    NimBLEClient* client = NimBLEDevice::createClient();
    client->setConnectTimeout(kConnectTimeoutMs);

    const NimBLEAddress addr(std::string(info->address), info->address_type);
    if (!client->connect(addr)) {
        out.println("GATT: connection FAILED");
        NimBLEDevice::deleteClient(client);
        return false;
    }
    out.println("GATT: connected. Enumerating (no writes, no subscriptions)...");

    int chars_seen = 0;
    for (NimBLERemoteService* svc : client->getServices(true)) {
        out.printf("SERVICE %s\n", svc->getUUID().toString().c_str());

        for (NimBLERemoteCharacteristic* ch : svc->getCharacteristics(true)) {
            if (++chars_seen > kMaxCharsPerDevice) {
                out.println("  ... characteristic limit reached, stopping");
                goto done;
            }
            out.printf("  CHAR %s  props:%s%s%s%s%s\n", ch->getUUID().toString().c_str(),
                       ch->canRead() ? " READ" : "", ch->canWrite() ? " WRITE" : "",
                       ch->canWriteNoResponse() ? " WRITE_NR" : "",
                       ch->canNotify() ? " NOTIFY" : "", ch->canIndicate() ? " INDICATE" : "");

            // Only READ-capable characteristics are read. Nothing is ever written.
            if (ch->canRead()) {
                const NimBLEAttValue value = ch->readValue();
                const size_t len = std::min<size_t>(value.size(), kMaxReadBytes);
                out.printf("    value (%u B): ", (unsigned)value.size());
                printHexSpaced(out, value.data(), len);
                out.print(" | ");
                printAscii(out, value.data(), len);
                out.println();
            }
        }
    }
done:
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    out.println("GATT: done, disconnected.");
    return true;
}

}  // namespace ble_analyzer
