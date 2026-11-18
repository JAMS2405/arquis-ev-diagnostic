#include <unity.h>

#include <string.h>

#include "jbd_protocol.h"

using namespace jbd;

// Response frame from the public JBD protocol documentation (basic info, cmd 0x03).
static const uint8_t kDocBasicInfo[] = {
    0xDD, 0x03, 0x00, 0x1B, 0x17, 0x00, 0x00, 0x00, 0x02, 0xD0, 0x03, 0xE8, 0x00, 0x00,
    0x20, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x48, 0x03, 0x0F, 0x02, 0x0B,
    0x76, 0x0B, 0x82, 0xFB, 0xFF, 0x77};

void setUp() {}
void tearDown() {}

void test_requests_match_known_frames() {
    uint8_t buf[kRequestLen];
    // The 0x04 frame is exactly what was observed in the FF02 buffer of a real JBD BMS.
    const uint8_t cell[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
    const uint8_t basic[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
    const uint8_t name[] = {0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77};

    TEST_ASSERT_EQUAL(7, buildReadRequest(ReadCommand::CellVoltages, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(cell, buf, 7);
    TEST_ASSERT_EQUAL(7, buildReadRequest(ReadCommand::BasicInfo, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(basic, buf, 7);
    TEST_ASSERT_EQUAL(7, buildReadRequest(ReadCommand::HardwareName, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(name, buf, 7);
}

void test_requests_always_use_read_opcode_and_no_payload() {
    // Safety property: every buildable request is a READ (0xA5) with length 0.
    const ReadCommand all[] = {ReadCommand::BasicInfo, ReadCommand::CellVoltages,
                               ReadCommand::HardwareName};
    for (ReadCommand c : all) {
        uint8_t buf[kRequestLen];
        TEST_ASSERT_EQUAL(7, buildReadRequest(c, buf, sizeof(buf)));
        TEST_ASSERT_EQUAL_HEX8(0xA5, buf[1]);
        TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);
    }
}

void test_request_buffer_too_small() {
    uint8_t buf[6];
    TEST_ASSERT_EQUAL(0, buildReadRequest(ReadCommand::BasicInfo, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(0, buildReadRequest(ReadCommand::BasicInfo, nullptr, 7));
}

void test_parse_documented_basic_info_frame() {
    Frame f;
    TEST_ASSERT_EQUAL(FrameState::Complete,
                      parseFrame(kDocBasicInfo, sizeof(kDocBasicInfo), f));
    TEST_ASSERT_EQUAL_HEX8(0x03, f.command);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.status);
    TEST_ASSERT_EQUAL(27, f.data_len);
    TEST_ASSERT_EQUAL(sizeof(kDocBasicInfo), f.frame_len);

    BasicInfo b{};
    TEST_ASSERT_TRUE(parseBasicInfo(f.data, f.data_len, b));
    TEST_ASSERT_EQUAL_UINT16(5888, b.total_voltage_cv);  // 58.88 V
    TEST_ASSERT_EQUAL_INT16(0, b.current_ca);
    TEST_ASSERT_EQUAL_UINT16(720, b.residual_cah);       // 7.20 Ah
    TEST_ASSERT_EQUAL_UINT16(1000, b.nominal_cah);       // 10.00 Ah
    TEST_ASSERT_EQUAL_UINT16(0, b.cycles);
    TEST_ASSERT_EQUAL_UINT16(2016, b.prod_year);
    TEST_ASSERT_EQUAL_UINT8(3, b.prod_month);
    TEST_ASSERT_EQUAL_UINT8(24, b.prod_day);
    TEST_ASSERT_EQUAL_UINT16(0, b.protection_flags);
    TEST_ASSERT_EQUAL_HEX8(0x10, b.sw_version);
    TEST_ASSERT_EQUAL_UINT8(72, b.rsoc_percent);
    TEST_ASSERT_TRUE(b.fet_charge_on);
    TEST_ASSERT_TRUE(b.fet_discharge_on);
    TEST_ASSERT_EQUAL_UINT8(15, b.cell_count);
    TEST_ASSERT_EQUAL_UINT8(2, b.ntc_count);
    TEST_ASSERT_EQUAL_INT16(203, b.temp_dc[0]);  // 20.3 °C
    TEST_ASSERT_EQUAL_INT16(215, b.temp_dc[1]);  // 21.5 °C
}

void test_split_notifications_are_reassembled() {
    // BLE delivers 20-byte chunks: parseFrame must report Incomplete until all arrive.
    Frame f;
    TEST_ASSERT_EQUAL(FrameState::Incomplete, parseFrame(kDocBasicInfo, 1, f));
    TEST_ASSERT_EQUAL(FrameState::Incomplete, parseFrame(kDocBasicInfo, 4, f));
    TEST_ASSERT_EQUAL(FrameState::Incomplete, parseFrame(kDocBasicInfo, 20, f));
    TEST_ASSERT_EQUAL(FrameState::Incomplete, parseFrame(kDocBasicInfo, sizeof(kDocBasicInfo) - 1, f));
    TEST_ASSERT_EQUAL(FrameState::Complete, parseFrame(kDocBasicInfo, sizeof(kDocBasicInfo), f));
    TEST_ASSERT_EQUAL(FrameState::Incomplete, parseFrame(kDocBasicInfo, 0, f));
}

void test_corrupted_frames_are_rejected() {
    uint8_t buf[sizeof(kDocBasicInfo)];
    Frame f;

    memcpy(buf, kDocBasicInfo, sizeof(buf));
    buf[10] ^= 0x01;  // flip a data bit -> checksum mismatch
    TEST_ASSERT_EQUAL(FrameState::Invalid, parseFrame(buf, sizeof(buf), f));

    memcpy(buf, kDocBasicInfo, sizeof(buf));
    buf[sizeof(buf) - 1] = 0x00;  // wrong end byte
    TEST_ASSERT_EQUAL(FrameState::Invalid, parseFrame(buf, sizeof(buf), f));

    memcpy(buf, kDocBasicInfo, sizeof(buf));
    buf[0] = 0x00;  // wrong start byte
    TEST_ASSERT_EQUAL(FrameState::Invalid, parseFrame(buf, sizeof(buf), f));
}

void test_basic_info_rejects_short_payload() {
    BasicInfo b{};
    uint8_t d[22] = {0};
    TEST_ASSERT_FALSE(parseBasicInfo(d, sizeof(d), b));
    TEST_ASSERT_FALSE(parseBasicInfo(nullptr, 30, b));
    // Claims 4 sensors but only carries bytes for 1.
    uint8_t e[25] = {0};
    e[22] = 4;
    TEST_ASSERT_FALSE(parseBasicInfo(e, sizeof(e), b));
}

void test_negative_current_is_discharge() {
    uint8_t d[23] = {0};
    d[2] = 0xFE;  // 0xFE0C as int16 = -500 -> -5.00 A (discharging)
    d[3] = 0x0C;
    BasicInfo b{};
    TEST_ASSERT_TRUE(parseBasicInfo(d, sizeof(d), b));
    TEST_ASSERT_EQUAL_INT16(-500, b.current_ca);
}

void test_cell_voltages_and_stats() {
    // 4 cells: 3300, 3310, 3290, 3305 mV
    const uint8_t d[] = {0x0C, 0xE4, 0x0C, 0xEE, 0x0C, 0xDA, 0x0C, 0xE9};
    CellVoltages c{};
    TEST_ASSERT_TRUE(parseCellVoltages(d, sizeof(d), c));
    TEST_ASSERT_EQUAL_UINT8(4, c.count);
    TEST_ASSERT_EQUAL_UINT16(3300, c.mv[0]);
    CellStats s = computeCellStats(c);
    TEST_ASSERT_EQUAL_UINT16(3290, s.min_mv);
    TEST_ASSERT_EQUAL_UINT16(3310, s.max_mv);
    TEST_ASSERT_EQUAL_UINT16(20, s.delta_mv);
    TEST_ASSERT_EQUAL_UINT16(3301, s.avg_mv);  // 13205 / 4 = 3301
    TEST_ASSERT_EQUAL_UINT8(2, s.min_index);
    TEST_ASSERT_EQUAL_UINT8(1, s.max_index);
}

void test_cell_voltages_reject_bad_lengths() {
    CellVoltages c{};
    const uint8_t odd[] = {0x0C, 0xE4, 0x0C};
    TEST_ASSERT_FALSE(parseCellVoltages(odd, sizeof(odd), c));
    TEST_ASSERT_FALSE(parseCellVoltages(odd, 0, c));
    uint8_t big[(kMaxCells + 1) * 2] = {0};
    TEST_ASSERT_FALSE(parseCellVoltages(big, sizeof(big), c));
}

void test_hardware_name_is_sanitized() {
    const uint8_t d[] = {'S', 'P', '1', '4', 0x00, 0x01, 'S', 0xFF, '2', '0', 'A'};
    char out[16];
    const size_t n = parseHardwareName(d, sizeof(d), out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SP14S20A", out);
    TEST_ASSERT_EQUAL(8, n);
    char tiny[4];
    parseHardwareName(d, sizeof(d), tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_STRING("SP1", tiny);
}

void test_protection_keys() {
    TEST_ASSERT_EQUAL_STRING("cell_overvoltage", protectionKey(0));
    TEST_ASSERT_EQUAL_STRING("short_circuit", protectionKey(10));
    TEST_ASSERT_EQUAL_STRING("mos_software_lock", protectionKey(12));
    TEST_ASSERT_NULL(protectionKey(13));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_requests_match_known_frames);
    RUN_TEST(test_requests_always_use_read_opcode_and_no_payload);
    RUN_TEST(test_request_buffer_too_small);
    RUN_TEST(test_parse_documented_basic_info_frame);
    RUN_TEST(test_split_notifications_are_reassembled);
    RUN_TEST(test_corrupted_frames_are_rejected);
    RUN_TEST(test_basic_info_rejects_short_payload);
    RUN_TEST(test_negative_current_is_discharge);
    RUN_TEST(test_cell_voltages_and_stats);
    RUN_TEST(test_cell_voltages_reject_bad_lengths);
    RUN_TEST(test_hardware_name_is_sanitized);
    RUN_TEST(test_protection_keys);
    return UNITY_END();
}
