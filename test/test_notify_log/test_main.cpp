#include <unity.h>

#include <string.h>

#include "notify_log.h"

using namespace notify_log;

void setUp() {}
void tearDown() {}

void test_format_line_exact() {
    const uint8_t d[] = {0x55, 0xAA, 0x41, 0x42, 0x00};
    char out[128];
    const size_t n = formatLine(out, sizeof(out), 1234, "6e400003", d, sizeof(d), 64);
    TEST_ASSERT_EQUAL_STRING("[   1234 ms] 6e400003   5 B  55 AA 41 42 00 | U.AB.\n", out);
    TEST_ASSERT_EQUAL(strlen(out), n);
}

void test_format_line_truncates_payload() {
    uint8_t d[20];
    for (int i = 0; i < 20; i++) d[i] = static_cast<uint8_t>(0x30 + i);
    char out[256];
    formatLine(out, sizeof(out), 5, "ffe1", d, sizeof(d), 4);
    TEST_ASSERT_NOT_NULL(strstr(out, "30 31 32 33 +16 more"));
    TEST_ASSERT_NOT_NULL(strstr(out, "| 0123\n"));
}

void test_format_line_never_overflows_small_buffer() {
    const uint8_t d[] = {1, 2, 3, 4, 5, 6, 7, 8};
    char out[24];
    memset(out, 'X', sizeof(out));
    formatLine(out, sizeof(out), 99999, "tag", d, sizeof(d), 64);
    TEST_ASSERT_TRUE(strlen(out) < sizeof(out));  // NUL-terminated inside the buffer
}

void test_format_line_empty_and_null_args() {
    char out[64];
    formatLine(out, sizeof(out), 1, nullptr, nullptr, 0, 64);
    TEST_ASSERT_NOT_NULL(strstr(out, "? "));
    TEST_ASSERT_EQUAL(0, formatLine(nullptr, 10, 1, "x", nullptr, 0, 8));
    TEST_ASSERT_EQUAL(0, formatLine(out, 0, 1, "x", nullptr, 0, 8));
}

void test_stats_accumulate() {
    Stats s;
    reset(s);
    const uint8_t a[] = {0x55, 1, 2};
    const uint8_t b[] = {0x55, 1, 2, 3, 4};
    const uint8_t c[] = {0xAA};
    add(s, a, sizeof(a), 100);
    add(s, b, sizeof(b), 300);
    add(s, c, sizeof(c), 500);
    TEST_ASSERT_EQUAL_UINT32(3, s.count);
    TEST_ASSERT_EQUAL_UINT32(9, s.bytes);
    TEST_ASSERT_EQUAL_UINT16(1, s.min_len);
    TEST_ASSERT_EQUAL_UINT16(5, s.max_len);
    TEST_ASSERT_EQUAL_UINT32(200, averageIntervalMs(s));
    TopByte top[3];
    const size_t n = topFirstBytes(s, top, 3);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_HEX8(0x55, top[0].value);
    TEST_ASSERT_EQUAL_UINT16(2, top[0].count);
    TEST_ASSERT_EQUAL_HEX8(0xAA, top[1].value);
}

void test_stats_empty_and_single() {
    Stats s;
    reset(s);
    TopByte top[2];
    TEST_ASSERT_EQUAL(0, topFirstBytes(s, top, 2));
    TEST_ASSERT_EQUAL_UINT32(0, averageIntervalMs(s));
    const uint8_t a[] = {7};
    add(s, a, 1, 50);
    TEST_ASSERT_EQUAL_UINT32(0, averageIntervalMs(s));
    TEST_ASSERT_EQUAL_UINT16(1, s.min_len);
}

void test_zero_length_notification_does_not_crash() {
    Stats s;
    reset(s);
    add(s, nullptr, 0, 10);
    TEST_ASSERT_EQUAL_UINT32(1, s.count);
    TEST_ASSERT_EQUAL_UINT32(0, s.bytes);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_format_line_exact);
    RUN_TEST(test_format_line_truncates_payload);
    RUN_TEST(test_format_line_never_overflows_small_buffer);
    RUN_TEST(test_format_line_empty_and_null_args);
    RUN_TEST(test_stats_accumulate);
    RUN_TEST(test_stats_empty_and_single);
    RUN_TEST(test_zero_length_notification_does_not_crash);
    return UNITY_END();
}
