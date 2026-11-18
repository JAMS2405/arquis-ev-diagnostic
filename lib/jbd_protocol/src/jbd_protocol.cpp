#include "jbd_protocol.h"

namespace jbd {

namespace {

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

}  // namespace

size_t buildReadRequest(ReadCommand cmd, uint8_t* out, size_t out_len) {
    if (out == nullptr || out_len < kRequestLen) return 0;
    const uint8_t c = static_cast<uint8_t>(cmd);
    const uint16_t chk = static_cast<uint16_t>(0x10000 - (c + 0x00));
    out[0] = kFrameStart;
    out[1] = 0xA5;  // read
    out[2] = c;
    out[3] = 0x00;  // no payload
    out[4] = static_cast<uint8_t>(chk >> 8);
    out[5] = static_cast<uint8_t>(chk & 0xFF);
    out[6] = kFrameEnd;
    return kRequestLen;
}

FrameState parseFrame(const uint8_t* buf, size_t len, Frame& out) {
    if (len == 0) return FrameState::Incomplete;
    if (buf[0] != kFrameStart) return FrameState::Invalid;
    if (len < 4) return FrameState::Incomplete;

    const uint8_t data_len = buf[3];
    const size_t total = static_cast<size_t>(data_len) + 7;
    if (len < total) return FrameState::Incomplete;
    if (buf[total - 1] != kFrameEnd) return FrameState::Invalid;

    uint16_t sum = static_cast<uint16_t>(buf[2] + buf[3]);  // status + len
    for (size_t i = 0; i < data_len; i++) sum = static_cast<uint16_t>(sum + buf[4 + i]);
    const uint16_t expected = static_cast<uint16_t>(0x10000 - sum);
    if (be16(&buf[4 + data_len]) != expected) return FrameState::Invalid;

    out.command   = buf[1];
    out.status    = buf[2];
    out.data      = &buf[4];
    out.data_len  = data_len;
    out.frame_len = total;
    return FrameState::Complete;
}

bool parseBasicInfo(const uint8_t* d, size_t len, BasicInfo& out) {
    if (d == nullptr || len < 23) return false;

    out.total_voltage_cv = be16(&d[0]);
    out.current_ca       = static_cast<int16_t>(be16(&d[2]));
    out.residual_cah     = be16(&d[4]);
    out.nominal_cah      = be16(&d[6]);
    out.cycles           = be16(&d[8]);

    const uint16_t date = be16(&d[10]);
    out.prod_day   = static_cast<uint8_t>(date & 0x1F);
    out.prod_month = static_cast<uint8_t>((date >> 5) & 0x0F);
    out.prod_year  = static_cast<uint16_t>(2000 + (date >> 9));

    out.balance_mask     = static_cast<uint32_t>(be16(&d[12])) | (static_cast<uint32_t>(be16(&d[14])) << 16);
    out.protection_flags = be16(&d[16]);
    out.sw_version       = d[18];
    out.rsoc_percent     = d[19];
    out.fet_charge_on    = (d[20] & 0x01) != 0;
    out.fet_discharge_on = (d[20] & 0x02) != 0;
    out.cell_count       = d[21];

    const uint8_t ntc_in_frame = d[22];
    if (len < static_cast<size_t>(23) + 2u * ntc_in_frame) return false;
    out.ntc_count = ntc_in_frame < kMaxTemps ? ntc_in_frame : static_cast<uint8_t>(kMaxTemps);
    for (uint8_t i = 0; i < out.ntc_count; i++) {
        // Raw value is in 0.1 K; convert to 0.1 °C.
        out.temp_dc[i] = static_cast<int16_t>(static_cast<int>(be16(&d[23 + 2 * i])) - 2731);
    }
    return true;
}

bool parseCellVoltages(const uint8_t* d, size_t len, CellVoltages& out) {
    if (d == nullptr || len == 0 || (len % 2) != 0) return false;
    const size_t n = len / 2;
    if (n > kMaxCells) return false;
    out.count = static_cast<uint8_t>(n);
    for (size_t i = 0; i < n; i++) out.mv[i] = be16(&d[2 * i]);
    return true;
}

CellStats computeCellStats(const CellVoltages& cells) {
    CellStats s{};
    if (cells.count == 0) return s;
    s.min_mv = s.max_mv = cells.mv[0];
    uint32_t sum = 0;
    for (uint8_t i = 0; i < cells.count; i++) {
        const uint16_t v = cells.mv[i];
        sum += v;
        if (v < s.min_mv) { s.min_mv = v; s.min_index = i; }
        if (v > s.max_mv) { s.max_mv = v; s.max_index = i; }
    }
    s.avg_mv   = static_cast<uint16_t>(sum / cells.count);
    s.delta_mv = static_cast<uint16_t>(s.max_mv - s.min_mv);
    return s;
}

size_t parseHardwareName(const uint8_t* data, size_t len, char* out, size_t out_len) {
    if (out == nullptr || out_len == 0) return 0;
    size_t n = 0;
    for (size_t i = 0; i < len && n + 1 < out_len; i++) {
        if (data[i] >= 0x20 && data[i] < 0x7F) out[n++] = static_cast<char>(data[i]);
    }
    out[n] = '\0';
    return n;
}

const char* protectionKey(unsigned bit) {
    static const char* const kKeys[kProtectionBitCount] = {
        "cell_overvoltage",       // bit 0
        "cell_undervoltage",      // bit 1
        "pack_overvoltage",       // bit 2
        "pack_undervoltage",      // bit 3
        "charge_overtemp",        // bit 4
        "charge_undertemp",       // bit 5
        "discharge_overtemp",     // bit 6
        "discharge_undertemp",    // bit 7
        "charge_overcurrent",     // bit 8
        "discharge_overcurrent",  // bit 9
        "short_circuit",          // bit 10
        "frontend_ic_error",      // bit 11
        "mos_software_lock",      // bit 12
    };
    return bit < kProtectionBitCount ? kKeys[bit] : nullptr;
}

}  // namespace jbd
