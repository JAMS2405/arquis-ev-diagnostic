#include <unity.h>

#include <string.h>

#include "panel_text.h"

using namespace panel_text;

void setUp() {}
void tearDown() {}

static const uint32_t kMax32 = 0xFFFFFFFFu;

static void assertFitsTitle(const char* s) {
    TEST_ASSERT_TRUE_MESSAGE(strlen(s) <= kTitleMax, s);
}
static void assertFitsDetail(const char* s) {
    TEST_ASSERT_TRUE_MESSAGE(strlen(s) <= kDetailMax, s);
}
static void assertAscii(const char* s) {
    for (const char* p = s; *p; p++) {
        TEST_ASSERT_TRUE_MESSAGE(*p >= 32 && *p < 127, s);  // the TFT font is ASCII only
    }
}

void test_ble_texts_fit_for_all_states_and_extremes() {
    BleState cases[] = {
        {false, false, 0, 0, 0, 0},
        {true, false, 0, 0, 0, 40},
        {false, true, kMax32, kMax32, kMax32, 40},
        {false, true, 120, 12345, 2048, 0},
        {false, false, 0, 0, 0, 2147483647},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char t[64], d[64];
        bleTitle(t, sizeof(t), cases[i]);
        bleDetail(d, sizeof(d), cases[i]);
        assertFitsTitle(t);
        assertFitsDetail(d);
        assertAscii(t);
        assertAscii(d);
    }
}

void test_ble_content() {
    char t[64], d[64];
    BleState idle = {false, false, 0, 0, 0, 7};
    bleTitle(t, sizeof(t), idle);
    bleDetail(d, sizeof(d), idle);
    TEST_ASSERT_EQUAL_STRING("BLE: Libre", t);
    TEST_ASSERT_EQUAL_STRING("Ultimo escaneo: 7 dispositivos", d);

    BleState listening = {false, true, 12, 34, 280, 7};
    bleTitle(t, sizeof(t), listening);
    bleDetail(d, sizeof(d), listening);
    TEST_ASSERT_EQUAL_STRING("BLE: Escuchando", t);
    TEST_ASSERT_EQUAL_STRING("quedan 12s  34 notif  280 B", d);

    BleState scanning = {true, false, 0, 0, 0, 0};
    bleTitle(t, sizeof(t), scanning);
    TEST_ASSERT_EQUAL_STRING("BLE: Escaneando", t);
}

void test_uart_texts_fit_for_all_states_and_extremes() {
    UartState cases[] = {
        {0, 0, 0, 0, 0, 0, 0},
        {0, 1, 0, 0, 1500, 99999, 230400},
        {1, 1, 0, kMax32, 0, 0, 0},
        {2, 0, 230400, 600, 1500, kMax32, 0},
        {2, 1, kMax32, kMax32, kMax32, kMax32, kMax32},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char t[64], d[64];
        uartTitle(t, sizeof(t), cases[i]);
        uartDetail(d, sizeof(d), cases[i]);
        assertFitsTitle(t);
        assertAscii(t);
        assertAscii(d);
        assertFitsDetail(d);  // even the absurd all-max case must fit
    }
}

void test_uart_content() {
    char t[64], d[64];
    UartState cap = {2, 0, 9600, 12, 34, 270, 0};
    uartTitle(t, sizeof(t), cap);
    uartDetail(d, sizeof(d), cap);
    TEST_ASSERT_EQUAL_STRING("UART: Captura A", t);
    TEST_ASSERT_EQUAL_STRING("9600 baud 34 tramas 270 B 12s", d);

    UartState det = {1, 1, 0, 2, 0, 0, 0};
    uartTitle(t, sizeof(t), det);
    TEST_ASSERT_EQUAL_STRING("UART: Detecta B", t);

    UartState done = {0, 0, 0, 0, 0, 0, 9600};
    uartDetail(d, sizeof(d), done);
    TEST_ASSERT_EQUAL_STRING("Ultima velocidad: 9600 baud", d);

    UartState fresh = {0, 0, 0, 0, 0, 0, 0};
    uartDetail(d, sizeof(d), fresh);
    TEST_ASSERT_EQUAL_STRING("Solo recibe. Sin captura todavia", d);
}

void test_bms_texts_fit_and_content() {
    char t[64], d[64];
    BmsState none = {false, 0, 0, 0, 0, 0};
    bmsTitle(t, sizeof(t), none);
    bmsDetail(d, sizeof(d), none);
    TEST_ASSERT_EQUAL_STRING("BMS: sin lectura", t);
    assertFitsDetail(d);

    BmsState ok = {true, 4620, 87, 76, 20, 0};
    bmsTitle(t, sizeof(t), ok);
    bmsDetail(d, sizeof(d), ok);
    TEST_ASSERT_EQUAL_STRING("BMS: 46.20 V", t);
    TEST_ASSERT_EQUAL_STRING("87 ciclos  76%  dif 20 mV", d);

    BmsState prot = {true, 4620, 87, 76, 20, 2};
    bmsDetail(d, sizeof(d), prot);
    TEST_ASSERT_EQUAL_STRING("PROTECCION ACTIVA  87 ciclos", d);

    BmsState extreme = {true, 65535u * 100u, 65535, 255, 65535, 0xFFFF};
    bmsTitle(t, sizeof(t), extreme);
    bmsDetail(d, sizeof(d), extreme);
    assertFitsTitle(t);
    assertFitsDetail(d);
    BmsState extreme2 = {true, 65535u * 100u, 65535, 255, 65535, 0};
    bmsDetail(d, sizeof(d), extreme2);
    assertFitsDetail(d);
}

void test_hint_priorities_and_fit() {
    char h[64];
    hint(h, sizeof(h), HintState{0, false, false});
    TEST_ASSERT_EQUAL_STRING("Conecta el telefono al WiFi", h);
    hint(h, sizeof(h), HintState{1, false, false});
    TEST_ASSERT_EQUAL_STRING("Abre la direccion en el navegador", h);
    hint(h, sizeof(h), HintState{1, true, false});
    TEST_ASSERT_EQUAL_STRING("BLE en curso: espera a que termine", h);
    hint(h, sizeof(h), HintState{0, true, true});  // UART wins: it carries the safety message
    TEST_ASSERT_EQUAL_STRING("UART: solo recibe, no envia nada", h);
    assertFitsDetail(h);
}

void test_uptime_and_bytes() {
    char b[32];
    uptime(b, sizeof(b), 0);
    TEST_ASSERT_EQUAL_STRING("00:00:00", b);
    uptime(b, sizeof(b), 3661);
    TEST_ASSERT_EQUAL_STRING("01:01:01", b);
    uptime(b, sizeof(b), 360000);
    TEST_ASSERT_EQUAL_STRING("100:00:00", b);
    uptime(b, sizeof(b), 4294967u);  // largest value millis()/1000 can reach
    TEST_ASSERT_EQUAL_STRING("1193:02:47", b);

    bytesText(b, sizeof(b), 512);
    TEST_ASSERT_EQUAL_STRING("512 B", b);
    bytesText(b, sizeof(b), 285u * 1024u);
    TEST_ASSERT_EQUAL_STRING("285 KB", b);
    bytesText(b, sizeof(b), 8386279u);
    TEST_ASSERT_EQUAL_STRING("7.99 MB", b);
}

void test_small_or_null_buffers_are_safe() {
    char tiny[4];
    BleState s = {false, true, 12, 34, 280, 7};
    bleDetail(tiny, sizeof(tiny), s);
    TEST_ASSERT_TRUE(strlen(tiny) < sizeof(tiny));
    hint(tiny, sizeof(tiny), HintState{1, false, false});
    TEST_ASSERT_TRUE(strlen(tiny) < sizeof(tiny));
    bleTitle(nullptr, 10, s);  // must not crash
    bleTitle(tiny, 0, s);
    uptime(nullptr, 0, 5);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ble_texts_fit_for_all_states_and_extremes);
    RUN_TEST(test_ble_content);
    RUN_TEST(test_uart_texts_fit_for_all_states_and_extremes);
    RUN_TEST(test_uart_content);
    RUN_TEST(test_bms_texts_fit_and_content);
    RUN_TEST(test_hint_priorities_and_fit);
    RUN_TEST(test_uptime_and_bytes);
    RUN_TEST(test_small_or_null_buffers_are_safe);
    return UNITY_END();
}
