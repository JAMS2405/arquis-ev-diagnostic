#pragma once
/**
 * JBD (Xiaoxiang) smart-BMS protocol — READ-ONLY subset.
 *
 * Pure C++ (no Arduino/BLE): can be unit-tested on the host.
 *
 * SAFETY BY CONSTRUCTION: the only request builder is buildReadRequest(), which
 * takes the ReadCommand enum. That enum contains exactly the three read
 * registers below, so this library cannot produce a write/config/MOSFET-control
 * frame. Do not add other commands here without an explicit scope decision.
 *
 * Frame layout (public JBD protocol documentation):
 *   request : DD A5 <cmd> <len=0> <chk_hi> <chk_lo> 77     chk = 0x10000 - (cmd+len)
 *   response: DD <cmd> <status> <len> <data...> <chk_hi> <chk_lo> 77
 *             chk = 0x10000 - (status + len + sum(data)); status 0x00 = OK
 */

#include <stddef.h>
#include <stdint.h>

namespace jbd {

constexpr uint8_t kFrameStart = 0xDD;
constexpr uint8_t kFrameEnd   = 0x77;
constexpr size_t kMaxCells    = 32;
constexpr size_t kMaxTemps    = 8;
constexpr size_t kRequestLen  = 7;
constexpr size_t kMaxFrameLen = 255 + 7;

/// The ONLY commands this project can send. All are register READS.
enum class ReadCommand : uint8_t {
    BasicInfo    = 0x03,  // pack voltage/current/capacity/cycles/protection/temps
    CellVoltages = 0x04,  // individual cell voltages
    HardwareName = 0x05,  // hardware/model string
};

/// Builds a 7-byte read request into `out`. Returns 7, or 0 if `out_len` < 7.
size_t buildReadRequest(ReadCommand cmd, uint8_t* out, size_t out_len);

enum class FrameState { Incomplete, Invalid, Complete };

struct Frame {
    uint8_t command;          // echoed command byte
    uint8_t status;           // 0x00 = OK, 0x80 = error
    const uint8_t* data;      // points into the caller's buffer
    uint8_t data_len;
    size_t frame_len;         // total bytes consumed by this frame
};

/// Checks a (possibly partial) receive buffer for a complete valid response.
///  - Incomplete: more bytes are needed.
///  - Invalid: bad start byte, bad end byte or bad checksum.
FrameState parseFrame(const uint8_t* buf, size_t len, Frame& out);

struct BasicInfo {
    uint16_t total_voltage_cv;   // 0.01 V units
    int16_t current_ca;          // 0.01 A units; positive = charging
    uint16_t residual_cah;       // 0.01 Ah units
    uint16_t nominal_cah;        // 0.01 Ah units
    uint16_t cycles;
    uint16_t prod_year;          // full year (e.g. 2016)
    uint8_t prod_month;
    uint8_t prod_day;
    uint32_t balance_mask;       // bit n = cell n+1 is balancing
    uint16_t protection_flags;   // see protectionKey()
    uint8_t sw_version;          // raw, e.g. 0x10 = 1.0
    uint8_t rsoc_percent;        // remaining state of charge reported by the BMS
    bool fet_charge_on;
    bool fet_discharge_on;
    uint8_t cell_count;
    uint8_t ntc_count;           // sensors present in the frame
    int16_t temp_dc[kMaxTemps];  // 0.1 °C units
};

bool parseBasicInfo(const uint8_t* data, size_t len, BasicInfo& out);

struct CellVoltages {
    uint8_t count;
    uint16_t mv[kMaxCells];
};

bool parseCellVoltages(const uint8_t* data, size_t len, CellVoltages& out);

struct CellStats {
    uint16_t min_mv, max_mv, avg_mv, delta_mv;
    uint8_t min_index, max_index;  // 0-based
};

/// Requires cells.count >= 1.
CellStats computeCellStats(const CellVoltages& cells);

/// Copies printable ASCII from the hardware-name payload (NUL-terminated).
/// Returns the number of characters written.
size_t parseHardwareName(const uint8_t* data, size_t len, char* out, size_t out_len);

constexpr unsigned kProtectionBitCount = 13;
/// Stable machine key for protection bit `bit` (0..12), or nullptr.
const char* protectionKey(unsigned bit);

}  // namespace jbd
