#include <unity.h>

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "uart_sniff.h"

using namespace uart_sniff;

void setUp() {}
void tearDown() {}

// ---- synthetic UART traces ------------------------------------------------

static uint32_t g_seed = 12345;
static uint32_t rnd() {
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed >> 8;
}
static float jitter(float amp) { return (static_cast<float>(rnd() % 2001) / 1000.0f - 1.0f) * amp; }

// Converts bytes (8N1, LSB first) into the widths of consecutive pulses, in us.
static std::vector<uint32_t> pulsesFor(uint32_t baud, const std::vector<uint8_t>& bytes,
                                       float jitter_us, int idle_bits) {
    const float T = 1.0e6f / static_cast<float>(baud);
    std::vector<int> bits;  // 1 = high, 0 = low
    for (size_t i = 0; i < bytes.size(); i++) {
        bits.push_back(0);  // start
        for (int b = 0; b < 8; b++) bits.push_back((bytes[i] >> b) & 1);
        bits.push_back(1);  // stop
        for (int k = 0; k < idle_bits; k++) bits.push_back(1);
    }
    std::vector<uint32_t> w;
    size_t i = 0;
    while (i < bits.size() && bits[i] == 1) i++;  // skip leading idle
    while (i < bits.size()) {
        size_t j = i;
        while (j < bits.size() && bits[j] == bits[i]) j++;
        float width = static_cast<float>(j - i) * T + jitter(jitter_us);
        if (width < 1.0f) width = 1.0f;
        if (j < bits.size()) w.push_back(static_cast<uint32_t>(width));  // drop the unterminated last run
        i = j;
    }
    return w;
}

static std::vector<uint8_t> randomBytes(size_t n) {
    std::vector<uint8_t> v;
    for (size_t i = 0; i < n; i++) v.push_back(static_cast<uint8_t>(rnd()));
    return v;
}

static std::vector<uint8_t> protocolBytes(size_t frames) {
    std::vector<uint8_t> v;
    for (size_t f = 0; f < frames; f++) {
        const uint8_t frame[] = {0x55, 0xAA, 0x08, 0x21, static_cast<uint8_t>(f * 7), 0x00, 0x3C, 0x00};
        v.insert(v.end(), frame, frame + sizeof(frame));
    }
    return v;
}

static void expectBaud(uint32_t baud, float jitter_us, bool protocol) {
    const std::vector<uint8_t> data = protocol ? protocolBytes(40) : randomBytes(200);
    const std::vector<uint32_t> w = pulsesFor(baud, data, jitter_us, 1);
    BaudEstimate e;
    estimateBaud(w.data(), w.size(), e);
    TEST_ASSERT_TRUE(e.top_count > 0);
    TEST_ASSERT_EQUAL_UINT32(baud, e.top[0].baud);
}

void test_baud_detected_for_common_rates() {
    expectBaud(1200, 3.0f, false);
    expectBaud(2400, 3.0f, false);
    expectBaud(4800, 3.0f, false);
    expectBaud(9600, 3.0f, false);
    expectBaud(19200, 2.0f, false);
    expectBaud(38400, 2.0f, false);
    expectBaud(57600, 2.0f, false);
    expectBaud(115200, 2.0f, false);
}

void test_baud_detected_for_repetitive_protocol_frames() {
    expectBaud(9600, 3.0f, true);
    expectBaud(115200, 2.0f, true);
}

void test_confident_flag_on_clean_trace() {
    const std::vector<uint32_t> w = pulsesFor(9600, randomBytes(300), 1.0f, 1);
    BaudEstimate e;
    estimateBaud(w.data(), w.size(), e);
    TEST_ASSERT_TRUE(e.confident);
    TEST_ASSERT_TRUE(e.top[0].score >= 0.9f);
}

void test_too_few_pulses_gives_no_candidate() {
    const uint32_t w[] = {104, 208, 104, 312, 104};
    BaudEstimate e;
    estimateBaud(w, 5, e);
    TEST_ASSERT_EQUAL(0, e.top_count);
    estimateBaud(nullptr, 100, e);
    TEST_ASSERT_EQUAL(0, e.top_count);
}

void test_random_noise_is_not_confident() {
    std::vector<uint32_t> w;
    for (int i = 0; i < 400; i++) w.push_back(5 + rnd() % 900);
    BaudEstimate e;
    estimateBaud(w.data(), w.size(), e);
    TEST_ASSERT_FALSE(e.confident);
}

void test_glitches_do_not_break_detection() {
    std::vector<uint32_t> w = pulsesFor(9600, randomBytes(300), 1.5f, 1);
    for (size_t i = 7; i < w.size(); i += 53) w[i] = 3;  // a few 3 us glitches
    BaudEstimate e;
    estimateBaud(w.data(), w.size(), e);
    TEST_ASSERT_TRUE(e.top_count > 0);
    TEST_ASSERT_EQUAL_UINT32(9600, e.top[0].baud);
}

// ---- frames ----------------------------------------------------------------

void test_collector_splits_frames_by_gap() {
    Frame storage[8];
    FrameCollector c(storage, 8, 3000);  // 3 ms gap
    // frame 1: bytes 1 ms apart, then 10 ms gap, frame 2
    c.addByte(0xAA, 1000);
    c.addByte(0xBB, 2000);
    c.addByte(0xCC, 3000);
    c.addByte(0x11, 13000);
    c.addByte(0x22, 14000);
    c.closeIfIdle(30000);  // trailing frame closes after idle
    TEST_ASSERT_EQUAL(2, c.count());
    TEST_ASSERT_EQUAL(3, c.at(0).len);
    TEST_ASSERT_EQUAL_HEX8(0xBB, c.at(0).data[1]);
    TEST_ASSERT_EQUAL(2, c.at(1).len);
    TEST_ASSERT_EQUAL_UINT32(12, c.at(1).t_ms);  // 13000 - 1000 us
    TEST_ASSERT_EQUAL_UINT32(5, c.totalBytes());
}

void test_collector_does_not_close_before_gap() {
    Frame storage[4];
    FrameCollector c(storage, 4, 3000);
    c.addByte(1, 0);
    c.closeIfIdle(2000);
    TEST_ASSERT_EQUAL(0, c.count());
    c.closeIfIdle(2000, true);  // forced
    TEST_ASSERT_EQUAL(1, c.count());
}

void test_collector_capacity_and_truncation() {
    Frame storage[2];
    FrameCollector c(storage, 2, 1000);
    for (int f = 0; f < 4; f++) c.addByte(static_cast<uint8_t>(f), static_cast<uint32_t>(f) * 10000);
    c.closeIfIdle(1000000, true);
    TEST_ASSERT_EQUAL(2, c.count());
    TEST_ASSERT_EQUAL(2, c.dropped());

    Frame one[1];
    FrameCollector t(one, 1, 1000);
    for (int i = 0; i < 100; i++) t.addByte(static_cast<uint8_t>(i), static_cast<uint32_t>(i) * 100);
    t.closeIfIdle(0, true);
    TEST_ASSERT_EQUAL(kMaxFrameBytes, t.at(0).len);
    TEST_ASSERT_TRUE(t.at(0).truncated);
}

void test_collector_survives_timer_wraparound() {
    Frame storage[4];
    FrameCollector c(storage, 4, 3000);
    c.addByte(1, 0xFFFFFF00u);
    c.addByte(2, 0xFFFFFF10u + 1000u);  // wraps past zero a little later: still same frame
    c.addByte(3, 1260u);                // 500 us after the previous byte: same frame
    c.closeIfIdle(0, true);
    TEST_ASSERT_EQUAL(1, c.count());
    TEST_ASSERT_EQUAL(3, c.at(0).len);
}

// ---- analysis --------------------------------------------------------------

static void addFrame(FrameCollector& c, uint32_t& t, const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        c.addByte(d[i], t);
        t += 100;
    }
    t += 20000;  // 20 ms gap
}

void test_analyze_finds_varying_positions_and_template() {
    Frame storage[32];
    FrameCollector c(storage, 32, 5000);
    uint32_t t = 0;
    for (int i = 0; i < 12; i++) {
        const uint8_t d[] = {0x55, 0xAA, 0x08, 0x21, static_cast<uint8_t>(i % 3), 0x00, 0x3C, static_cast<uint8_t>(i)};
        addFrame(c, t, d, sizeof(d));
    }
    const uint8_t other[] = {0x77, 0x01};  // a rarer shape must not win
    addFrame(c, t, other, sizeof(other));
    c.closeIfIdle(t, true);

    GroupAnalysis g;
    analyze(&storage[0], c.count(), g);
    TEST_ASSERT_TRUE(g.valid);
    TEST_ASSERT_EQUAL(8, g.len);
    TEST_ASSERT_EQUAL_HEX8(0x55, g.first_byte);
    TEST_ASSERT_EQUAL(12, g.frames_in_group);
    TEST_ASSERT_EQUAL(13, g.total_frames);
    TEST_ASSERT_EQUAL(2, g.varying_count);
    TEST_ASSERT_TRUE(g.pos[4].varies);
    TEST_ASSERT_EQUAL(3, g.pos[4].distinct);
    TEST_ASSERT_EQUAL_HEX8(0, g.pos[4].min);
    TEST_ASSERT_EQUAL_HEX8(2, g.pos[4].max);
    TEST_ASSERT_FALSE(g.pos[2].varies);
    TEST_ASSERT_EQUAL_UINT32(20, g.avg_period_ms);  // ~20.8 ms between frames, reported in ms
    char tpl[64];
    formatTemplate(g, tpl, sizeof(tpl));
    TEST_ASSERT_EQUAL_STRING("55 AA 08 21 ?? 00 3C ??", tpl);
}

void test_analyze_empty_input() {
    GroupAnalysis g;
    analyze(nullptr, 0, g);
    TEST_ASSERT_FALSE(g.valid);
    Frame empty{};
    analyze(&empty, 1, g);  // zero-length frame ignored
    TEST_ASSERT_FALSE(g.valid);
}

// ---- checksum --------------------------------------------------------------

static void buildChecksumFrames(FrameCollector& c, int mode) {
    uint32_t t = 0;
    for (int i = 0; i < 10; i++) {
        uint8_t d[10] = {0x55, 0xAA, 0x06, 0x20, 0x61, static_cast<uint8_t>(i * 5), static_cast<uint8_t>(i), 0x00, 0, 0};
        if (mode == 1) {  // Xiaomi-like: 16-bit LE of ~(sum from byte 2)
            uint32_t sum = 0;
            for (int k = 2; k < 8; k++) sum += d[k];
            const uint16_t ck = static_cast<uint16_t>(~sum);
            d[8] = static_cast<uint8_t>(ck & 0xFF);
            d[9] = static_cast<uint8_t>(ck >> 8);
        } else if (mode == 2) {  // plain xor8 over everything before the last byte
            uint8_t x = 0;
            for (int k = 0; k < 9; k++) x ^= d[k];
            d[9] = x;
        } else {  // wrong checksum
            d[8] = 0x12;
            d[9] = 0x34;
        }
        addFrame(c, t, d, 10);
    }
    c.closeIfIdle(t, true);
}

void test_checksum_detects_inverted_sum16_le() {
    Frame storage[16];
    FrameCollector c(storage, 16, 5000);
    buildChecksumFrames(c, 1);
    GroupAnalysis g;
    analyze(storage, c.count(), g);
    ChecksumHit hit;
    TEST_ASSERT_TRUE(detectChecksum(storage, c.count(), g, hit));
    TEST_ASSERT_EQUAL(static_cast<int>(ChecksumScheme::NotSum16LE), static_cast<int>(hit.scheme));
    TEST_ASSERT_EQUAL(2, hit.start);
}

void test_checksum_detects_xor8() {
    Frame storage[16];
    FrameCollector c(storage, 16, 5000);
    buildChecksumFrames(c, 2);
    GroupAnalysis g;
    analyze(storage, c.count(), g);
    ChecksumHit hit;
    TEST_ASSERT_TRUE(detectChecksum(storage, c.count(), g, hit));
    TEST_ASSERT_EQUAL(static_cast<int>(ChecksumScheme::Xor8), static_cast<int>(hit.scheme));
    TEST_ASSERT_EQUAL(0, hit.start);
}

void test_checksum_not_found_when_wrong() {
    Frame storage[16];
    FrameCollector c(storage, 16, 5000);
    buildChecksumFrames(c, 3);
    GroupAnalysis g;
    analyze(storage, c.count(), g);
    ChecksumHit hit;
    TEST_ASSERT_FALSE(detectChecksum(storage, c.count(), g, hit));
    TEST_ASSERT_EQUAL(static_cast<int>(ChecksumScheme::None), static_cast<int>(hit.scheme));
}

void test_checksum_needs_varying_distinct_frames() {
    Frame storage[8];
    FrameCollector c(storage, 8, 5000);
    uint32_t t = 0;
    for (int i = 0; i < 5; i++) {
        const uint8_t d[] = {0x55, 0xAA, 0x01, 0x02, 0x03};  // identical frames
        addFrame(c, t, d, sizeof(d));
    }
    c.closeIfIdle(t, true);
    GroupAnalysis g;
    analyze(storage, c.count(), g);
    ChecksumHit hit;
    TEST_ASSERT_FALSE(detectChecksum(storage, c.count(), g, hit));
}

void test_checksum_rejects_constant_trailing_byte() {
    // Frames whose last byte is always 0xFF while the data varies: must NOT be reported as a
    // checksum (regression: a 1-byte scheme matched by coincidence).
    Frame storage[16];
    FrameCollector c(storage, 16, 5000);
    uint32_t t = 0;
    for (int i = 0; i < 10; i++) {
        const uint8_t d[] = {0x55, 0xAA, static_cast<uint8_t>(i * 3), static_cast<uint8_t>(i), 0xFF};
        addFrame(c, t, d, sizeof(d));
    }
    c.closeIfIdle(t, true);
    GroupAnalysis g;
    analyze(storage, c.count(), g);
    ChecksumHit hit;
    TEST_ASSERT_FALSE(detectChecksum(storage, c.count(), g, hit));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_baud_detected_for_common_rates);
    RUN_TEST(test_baud_detected_for_repetitive_protocol_frames);
    RUN_TEST(test_confident_flag_on_clean_trace);
    RUN_TEST(test_too_few_pulses_gives_no_candidate);
    RUN_TEST(test_random_noise_is_not_confident);
    RUN_TEST(test_glitches_do_not_break_detection);
    RUN_TEST(test_collector_splits_frames_by_gap);
    RUN_TEST(test_collector_does_not_close_before_gap);
    RUN_TEST(test_collector_capacity_and_truncation);
    RUN_TEST(test_collector_survives_timer_wraparound);
    RUN_TEST(test_analyze_finds_varying_positions_and_template);
    RUN_TEST(test_analyze_empty_input);
    RUN_TEST(test_checksum_detects_inverted_sum16_le);
    RUN_TEST(test_checksum_detects_xor8);
    RUN_TEST(test_checksum_not_found_when_wrong);
    RUN_TEST(test_checksum_needs_varying_distinct_frames);
    RUN_TEST(test_checksum_rejects_constant_trailing_byte);
    return UNITY_END();
}
