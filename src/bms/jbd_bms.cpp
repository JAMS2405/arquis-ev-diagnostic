#include "bms/jbd_bms.h"

#include <NimBLEDevice.h>

#include <string.h>

#include "ble/ble_analyzer.h"

namespace jbd_bms {

namespace {

constexpr uint32_t kConnectTimeoutMs = 10000;
constexpr uint32_t kReplyTimeoutMs   = 1500;
constexpr int kAttemptsPerCommand    = 2;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t g_rx[jbd::kMaxFrameLen];
size_t g_rx_len = 0;

void setError(Report& r, const char* msg) {
    strncpy(r.error, msg, sizeof(r.error) - 1);
    r.error[sizeof(r.error) - 1] = '\0';
}

// Runs in the NimBLE host task: only appends bytes, no parsing.
void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    portENTER_CRITICAL(&g_mux);
    if (g_rx_len + len <= sizeof(g_rx)) {
        memcpy(g_rx + g_rx_len, data, len);
        g_rx_len += len;
    } else {
        g_rx_len = 0;  // overflow: drop and resync
    }
    portEXIT_CRITICAL(&g_mux);
}

void clearRx() {
    portENTER_CRITICAL(&g_mux);
    g_rx_len = 0;
    portEXIT_CRITICAL(&g_mux);
}

/// Sends one READ request and waits for its reply. The request is built ONLY by
/// jbd::buildReadRequest(); `cmd` is the read-only enum.
bool exchange(NimBLERemoteCharacteristic* writeChar, jbd::ReadCommand cmd, uint8_t* data_out,
              size_t& data_len) {
    uint8_t request[jbd::kRequestLen];
    const size_t n = jbd::buildReadRequest(cmd, request, sizeof(request));
    if (n == 0) return false;
    const bool needResponse = !writeChar->canWriteNoResponse();

    for (int attempt = 0; attempt < kAttemptsPerCommand; attempt++) {
        clearRx();
        if (!writeChar->writeValue(request, n, needResponse)) continue;

        const uint32_t t0 = millis();
        while (millis() - t0 < kReplyTimeoutMs) {
            uint8_t local[jbd::kMaxFrameLen];
            size_t len;
            portENTER_CRITICAL(&g_mux);
            len = g_rx_len;
            memcpy(local, g_rx, len);
            portEXIT_CRITICAL(&g_mux);

            if (len > 0) {
                jbd::Frame f;
                const jbd::FrameState st = jbd::parseFrame(local, len, f);
                if (st == jbd::FrameState::Complete) {
                    if (f.command == static_cast<uint8_t>(cmd) && f.status == 0x00) {
                        memcpy(data_out, f.data, f.data_len);
                        data_len = f.data_len;
                        return true;
                    }
                    clearRx();  // other command or error status: keep waiting / retry
                } else if (st == jbd::FrameState::Invalid) {
                    clearRx();  // resync
                }
            }
            delay(10);
        }
    }
    return false;
}

void finish(NimBLEClient* client) {
    if (client->isConnected()) client->disconnect();
    NimBLEDevice::deleteClient(client);
}

}  // namespace

bool readReport(int index, Report& out) {
    memset(&out, 0, sizeof(out));

    const ble_analyzer::DeviceInfo* info = ble_analyzer::device(index);
    if (!info) {
        setError(out, "invalid device index");
        return false;
    }
    strncpy(out.address, info->address, sizeof(out.address) - 1);
    strncpy(out.device_name, info->name, sizeof(out.device_name) - 1);
    if (!info->connectable) {
        setError(out, "device is not connectable");
        return false;
    }

    NimBLEClient* client = NimBLEDevice::createClient();
    client->setConnectTimeout(kConnectTimeoutMs);
    const NimBLEAddress addr(std::string(info->address), info->address_type);
    if (!client->connect(addr)) {
        setError(out, "connection failed");
        NimBLEDevice::deleteClient(client);
        return false;
    }

    NimBLERemoteService* svc = client->getService(NimBLEUUID(static_cast<uint16_t>(0xFF00)));
    if (!svc) {
        setError(out, "service 0xFF00 not found (not a JBD BMS?)");
        finish(client);
        return false;
    }
    NimBLERemoteCharacteristic* notifyChar = svc->getCharacteristic(NimBLEUUID(static_cast<uint16_t>(0xFF01)));
    NimBLERemoteCharacteristic* writeChar  = svc->getCharacteristic(NimBLEUUID(static_cast<uint16_t>(0xFF02)));
    if (!notifyChar || !writeChar || !notifyChar->canNotify() ||
        !(writeChar->canWrite() || writeChar->canWriteNoResponse())) {
        setError(out, "characteristics 0xFF01/0xFF02 missing or wrong properties");
        finish(client);
        return false;
    }
    if (!notifyChar->subscribe(true, onNotify)) {
        setError(out, "could not subscribe to notifications");
        finish(client);
        return false;
    }

    uint8_t buf[jbd::kMaxFrameLen];
    size_t len = 0;

    if (exchange(writeChar, jbd::ReadCommand::HardwareName, buf, len)) {
        jbd::parseHardwareName(buf, len, out.hardware_name, sizeof(out.hardware_name));
        out.have_name = true;
    }
    if (exchange(writeChar, jbd::ReadCommand::BasicInfo, buf, len)) {
        out.have_basic = jbd::parseBasicInfo(buf, len, out.basic);
    }
    if (exchange(writeChar, jbd::ReadCommand::CellVoltages, buf, len)) {
        out.have_cells = jbd::parseCellVoltages(buf, len, out.cells);
    }

    notifyChar->unsubscribe();
    finish(client);

    if (!out.have_basic) setError(out, "no valid reply to the basic-info request");
    return out.have_basic;
}

namespace {

void appendJsonString(String& s, const char* text) {
    s += '"';
    for (; *text; text++) {
        const unsigned char c = static_cast<unsigned char>(*text);
        if (c == '"' || c == '\\') {
            s += '\\';
            s += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            s += '?';
        } else {
            s += static_cast<char>(c);
        }
    }
    s += '"';
}

String versionString(uint8_t raw) { return String(raw >> 4) + "." + String(raw & 0x0F); }

}  // namespace

String toJson(const Report& r) {
    String j;
    j.reserve(1500);
    j += "{\"ok\":";
    j += r.have_basic ? "true" : "false";
    j += ",\"error\":";
    appendJsonString(j, r.error);
    j += ",\"address\":";
    appendJsonString(j, r.address);
    j += ",\"device_name\":";
    appendJsonString(j, r.device_name);
    j += ",\"hardware_name\":";
    appendJsonString(j, r.have_name ? r.hardware_name : "");

    if (r.have_basic) {
        const jbd::BasicInfo& b = r.basic;
        j += ",\"basic\":{";
        j += "\"total_voltage_cv\":" + String(b.total_voltage_cv);
        j += ",\"current_ca\":" + String(b.current_ca);
        j += ",\"residual_cah\":" + String(b.residual_cah);
        j += ",\"nominal_cah\":" + String(b.nominal_cah);
        j += ",\"cycles\":" + String(b.cycles);
        j += ",\"prod_year\":" + String(b.prod_year);
        j += ",\"prod_month\":" + String(b.prod_month);
        j += ",\"prod_day\":" + String(b.prod_day);
        j += ",\"rsoc\":" + String(b.rsoc_percent);
        j += ",\"sw_version\":";
        appendJsonString(j, versionString(b.sw_version).c_str());
        j += ",\"fet_charge\":";
        j += b.fet_charge_on ? "true" : "false";
        j += ",\"fet_discharge\":";
        j += b.fet_discharge_on ? "true" : "false";
        j += ",\"cell_count\":" + String(b.cell_count);

        j += ",\"protections\":[";
        bool first = true;
        for (unsigned bit = 0; bit < jbd::kProtectionBitCount; bit++) {
            if (b.protection_flags & (1u << bit)) {
                if (!first) j += ',';
                first = false;
                appendJsonString(j, jbd::protectionKey(bit));
            }
        }
        j += "],\"protection_raw\":" + String(b.protection_flags);

        j += ",\"balancing\":[";
        first = true;
        for (unsigned i = 0; i < jbd::kMaxCells; i++) {
            if (b.balance_mask & (1UL << i)) {
                if (!first) j += ',';
                first = false;
                j += String(i + 1);
            }
        }
        j += "],\"temps_dc\":[";
        for (uint8_t i = 0; i < b.ntc_count; i++) {
            if (i) j += ',';
            j += String(b.temp_dc[i]);
        }
        j += "]}";
    }

    if (r.have_cells && r.cells.count > 0) {
        const jbd::CellStats s = jbd::computeCellStats(r.cells);
        j += ",\"cells_mv\":[";
        for (uint8_t i = 0; i < r.cells.count; i++) {
            if (i) j += ',';
            j += String(r.cells.mv[i]);
        }
        j += "],\"cell_stats\":{";
        j += "\"min_mv\":" + String(s.min_mv) + ",\"max_mv\":" + String(s.max_mv);
        j += ",\"avg_mv\":" + String(s.avg_mv) + ",\"delta_mv\":" + String(s.delta_mv);
        j += ",\"min_cell\":" + String(s.min_index + 1) + ",\"max_cell\":" + String(s.max_index + 1);
        j += "}";
    }
    j += "}";
    return j;
}

void printText(const Report& r, Print& out) {
    out.println("=== JBD BMS REPORT (as reported by the BMS) ===");
    out.printf("BLE name : %s\n", r.device_name[0] ? r.device_name : "(none)");
    out.printf("Address  : %s\n", r.address);
    if (!r.have_basic) {
        out.printf("ERROR    : %s\n", r.error[0] ? r.error : "no data");
        return;
    }
    const jbd::BasicInfo& b = r.basic;
    out.printf("Hardware : %s\n", r.have_name ? r.hardware_name : "(not reported)");
    out.printf("SW ver   : %s\n", versionString(b.sw_version).c_str());
    out.printf("Produced : %04u-%02u-%02u\n", b.prod_year, b.prod_month, b.prod_day);
    out.printf("Cycles   : %u\n", b.cycles);
    out.printf("Voltage  : %.2f V\n", b.total_voltage_cv / 100.0f);
    out.printf("Current  : %.2f A (%s)\n", b.current_ca / 100.0f,
               b.current_ca > 0 ? "charging" : (b.current_ca < 0 ? "discharging" : "idle"));
    out.printf("Capacity : %.2f Ah remaining of %.2f Ah nominal (BMS RSOC %u%%)\n",
               b.residual_cah / 100.0f, b.nominal_cah / 100.0f, b.rsoc_percent);
    out.printf("MOSFETs  : charge %s, discharge %s\n", b.fet_charge_on ? "ON" : "off",
               b.fet_discharge_on ? "ON" : "off");
    out.printf("Cells    : %u reported\n", b.cell_count);
    for (uint8_t i = 0; i < b.ntc_count; i++) {
        out.printf("Temp %u   : %.1f C\n", i + 1, b.temp_dc[i] / 10.0f);
    }
    if (b.protection_flags == 0) {
        out.println("Protect. : none active");
    } else {
        out.print("Protect. :");
        for (unsigned bit = 0; bit < jbd::kProtectionBitCount; bit++) {
            if (b.protection_flags & (1u << bit)) out.printf(" %s", jbd::protectionKey(bit));
        }
        out.println();
    }
    if (r.have_cells && r.cells.count > 0) {
        const jbd::CellStats s = jbd::computeCellStats(r.cells);
        for (uint8_t i = 0; i < r.cells.count; i++) {
            out.printf("Cell %2u  : %u mV%s\n", i + 1, r.cells.mv[i],
                       (b.balance_mask & (1UL << i)) ? "  (balancing)" : "");
        }
        out.printf("Cell stats: min %u mV (#%u), max %u mV (#%u), avg %u mV, delta %u mV\n", s.min_mv,
                   s.min_index + 1, s.max_mv, s.max_index + 1, s.avg_mv, s.delta_mv);
    } else {
        out.println("Cells    : no voltage reply");
    }
    out.println("Note: values are what the BMS reports; they are not a capacity test.");
}

}  // namespace jbd_bms
