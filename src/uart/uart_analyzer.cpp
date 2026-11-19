#include "uart/uart_analyzer.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <string.h>

#include "config/pins_config.h"
#include "notify_log.h"
#include "uart_sniff.h"

namespace uart_analyzer {

namespace {

constexpr size_t kEdgeCap          = 2048;
constexpr size_t kLogCapPsram      = 96 * 1024;
constexpr size_t kLogCapFallback   = 16 * 1024;
constexpr size_t kFramesPsram      = 1500;
constexpr size_t kFramesFallback   = 100;
constexpr uint32_t kMinGapMs       = 2;
constexpr uint32_t kMaxSecs        = 600;
constexpr uint32_t kMinBaud        = 300;
constexpr uint32_t kMaxBaud        = 230400;

// ---- state shared between the loop task and the capture task ---------------
volatile Mode g_mode = Mode::Idle;
SemaphoreHandle_t g_mutex = nullptr;

char* g_log = nullptr;
size_t g_logCap = 0;
size_t g_logLen = 0;
bool g_truncated = false;

uart_sniff::Frame* g_frames = nullptr;
size_t g_frameCap = 0;
uart_sniff::FrameCollector* g_collector = nullptr;
size_t g_logged = 0;  // frames already written to the log

HardwareSerial* g_port = nullptr;
int g_channel = 0;
uint32_t g_endMs = 0;
uint32_t g_startMs = 0;
uint32_t g_baud = 0;
uint32_t g_lastDetectBaud = 0;

// ---- edge capture (detect) -------------------------------------------------
volatile uint32_t g_edges[kEdgeCap];
volatile size_t g_edgeN = 0;
int g_detectPin = -1;
uint32_t g_detectStartMs = 0;
bool g_detectIdleLevel = false;

void IRAM_ATTR onEdge() {
    const size_t n = g_edgeN;
    if (n < kEdgeCap) {
        g_edges[n] = static_cast<uint32_t>(esp_timer_get_time());
        g_edgeN = n + 1;
    }
}

// ---- helpers -----------------------------------------------------------------
void setError(char* error, size_t len, const char* msg) {
    if (error && len) {
        strncpy(error, msg, len - 1);
        error[len - 1] = '\0';
    }
}

int pinFor(int channel) { return channel == 0 ? pins::UART_SNIFF_A_RX : pins::UART_SNIFF_B_RX; }
char tagFor(int channel) { return channel == 0 ? 'A' : 'B'; }

// Caller must hold g_mutex.
void appendLocked(const char* text, size_t n) {
    if (g_log == nullptr) return;
    if (g_logLen + n < g_logCap) {
        memcpy(g_log + g_logLen, text, n);
        g_logLen += n;
        g_log[g_logLen] = '\0';
    } else {
        g_truncated = true;
    }
}

void appendStr(const char* text) {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    appendLocked(text, strlen(text));
    xSemaphoreGive(g_mutex);
}

void appendf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void appendf(const char* fmt, ...) {
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    appendStr(buf);
}

void resetLog() {
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_logLen = 0;
    if (g_log) g_log[0] = '\0';
    g_truncated = false;
    xSemaphoreGive(g_mutex);
}

// Writes frames closed since the last call. Caller must hold g_mutex.
void logNewFramesLocked() {
    if (g_collector == nullptr) return;
    const char tag[2] = {tagFor(g_channel), '\0'};
    char line[uart_sniff::kMaxFrameBytes * 4 + 64];
    while (g_logged < g_collector->count()) {
        const uart_sniff::Frame& f = g_collector->at(g_logged++);
        const size_t n = notify_log::formatLine(line, sizeof(line), f.t_ms, tag, f.data, f.len,
                                                uart_sniff::kMaxFrameBytes);
        appendLocked(line, n);
    }
}

// ---- capture task ------------------------------------------------------------
void captureTask(void*) {
    for (;;) {
        if (g_mode != Mode::Capturing || g_port == nullptr || g_collector == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        if (g_mode == Mode::Capturing) {
            int n = g_port->available();
            if (n > 0) {
                const uint32_t now = static_cast<uint32_t>(micros());
                while (n-- > 0) {
                    const int b = g_port->read();
                    if (b >= 0) g_collector->addByte(static_cast<uint8_t>(b), now);
                }
            }
            g_collector->closeIfIdle(static_cast<uint32_t>(micros()));
            logNewFramesLocked();
        }
        xSemaphoreGive(g_mutex);
        vTaskDelay(1);
    }
}

// ---- finishing ---------------------------------------------------------------
void finishDetect(const char* reason) {
    detachInterrupt(digitalPinToInterrupt(g_detectPin));
    g_mode = Mode::Idle;

    const size_t edges = g_edgeN;
    const uint32_t elapsed = millis() - g_detectStartMs;
    appendf("--- detect end: %s ---\n", reason);
    appendf("edges: %u in %lu ms, idle level: %s\n", static_cast<unsigned>(edges),
            static_cast<unsigned long>(elapsed), g_detectIdleLevel ? "HIGH" : "LOW");

    if (edges < 21) {
        appendStr("No activity (fewer than 21 edges). Check: GND to the vehicle ground, the divider, "
                  "the right wire, and that the vehicle is powered.\n");
        return;
    }

    static uint32_t widths[kEdgeCap];
    size_t n = 0;
    for (size_t i = 1; i < edges; i++) {
        widths[n++] = g_edges[i] - g_edges[i - 1];
    }
    uart_sniff::BaudEstimate est;
    uart_sniff::estimateBaud(widths, n, est);

    appendf("pulses: %u, smallest typical pulse: %lu us\n", static_cast<unsigned>(n),
            static_cast<unsigned long>(est.min_width_us));
    if (est.top_count == 0) {
        appendStr("No standard baud rate fits these pulses. The line may not be a UART, the level may be "
                  "wrong, or the speed is non-standard.\n");
        return;
    }
    g_lastDetectBaud = est.top[0].baud;
    for (size_t i = 0; i < est.top_count; i++) {
        appendf(">> baud %lu score %.2f\n", static_cast<unsigned long>(est.top[i].baud),
                static_cast<double>(est.top[i].score));
    }
    appendStr(est.confident ? "Confident estimate. Capture at the first baud (8N1 is the common format).\n"
                            : "NOT confident: try the candidates in order and check that frames repeat.\n");
}

void finishCapture(const char* reason) {
    // Stop ingestion first, then close the open frame and summarize.
    g_mode = Mode::Idle;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    if (g_collector) {
        g_collector->closeIfIdle(static_cast<uint32_t>(micros()), true);
        logNewFramesLocked();
    }
    xSemaphoreGive(g_mutex);

    if (g_port) {
        g_port->end();
        g_port = nullptr;
    }

    appendf("--- capture end: %s ---\n", reason);
    if (g_collector == nullptr) return;

    const size_t frames = g_collector->count();
    appendf("channel %c, baud %lu, frames: %u, bytes: %lu, dropped frames: %u\n", tagFor(g_channel),
            static_cast<unsigned long>(g_baud), static_cast<unsigned>(frames),
            static_cast<unsigned long>(g_collector->totalBytes()),
            static_cast<unsigned>(g_collector->dropped()));

    if (g_collector->totalBytes() == 0) {
        appendStr("No bytes received. Run 'detect' first; check wiring, GND and the baud/format.\n");
        return;
    }

    uart_sniff::GroupAnalysis g;
    uart_sniff::analyze(g_frames, frames, g);
    if (!g.valid) {
        appendStr("Not enough data to analyze.\n");
        return;
    }
    const unsigned pct = frames ? static_cast<unsigned>((100u * g.frames_in_group) / frames) : 0;
    appendf("dominant frame: %u bytes, first byte %02X, %u of %u frames (%u%%), period ~%lu ms\n", g.len,
            g.first_byte, static_cast<unsigned>(g.frames_in_group), static_cast<unsigned>(frames), pct,
            static_cast<unsigned long>(g.avg_period_ms));

    char tpl[uart_sniff::kMaxFrameBytes * 3 + 4];
    uart_sniff::formatTemplate(g, tpl, sizeof(tpl));
    appendf("template (?? = varies): %s\n", tpl);

    if (g.varying_count > 0) {
        appendStr("varying positions (0-based: min..max, distinct values):\n");
        for (uint8_t p = 0; p < g.len; p++) {
            if (!g.pos[p].varies) continue;
            appendf("  [%2u] %02X..%02X  %u value(s)\n", p, g.pos[p].min, g.pos[p].max, g.pos[p].distinct);
        }
    }

    uart_sniff::ChecksumHit hit;
    if (uart_sniff::detectChecksum(g_frames, frames, g, hit)) {
        appendf("checksum: %s over bytes from offset %u (verified on every frame of the group)\n",
                uart_sniff::schemeName(hit.scheme), hit.start);
    } else {
        appendStr("checksum: not detected with the common schemes (or too few distinct frames)\n");
    }

    if (pct < 20) {
        appendStr("HINT: frames do not repeat. Wrong baud or format (try 8E1/8O1), or irregular data.\n");
    }
}

}  // namespace

bool begin() {
    if (g_mutex != nullptr) return true;
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == nullptr) return false;

    const bool psram = psramFound();
    g_logCap = psram ? kLogCapPsram : kLogCapFallback;
    g_frameCap = psram ? kFramesPsram : kFramesFallback;
    g_log = static_cast<char*>(psram ? ps_malloc(g_logCap) : malloc(g_logCap));
    g_frames = static_cast<uart_sniff::Frame*>(
        psram ? ps_malloc(g_frameCap * sizeof(uart_sniff::Frame)) : malloc(g_frameCap * sizeof(uart_sniff::Frame)));
    if (g_log == nullptr || g_frames == nullptr) return false;
    g_log[0] = '\0';

    return xTaskCreatePinnedToCore(captureTask, "uartcap", 6144, nullptr, 5, nullptr, 1) == pdPASS;
}

bool startDetect(int channel, uint32_t window_ms, char* error, size_t error_len) {
    if (g_mutex == nullptr || g_log == nullptr) {
        setError(error, error_len, "analyzer not initialized");
        return false;
    }
    if (g_mode != Mode::Idle) {
        setError(error, error_len, "a detection or capture is running");
        return false;
    }
    if (channel < 0 || channel > 1) {
        setError(error, error_len, "channel must be 0 (A) or 1 (B)");
        return false;
    }
    if (window_ms < 200) window_ms = 200;
    if (window_ms > 10000) window_ms = 10000;

    resetLog();
    g_channel = channel;
    g_detectPin = pinFor(channel);
    pinMode(g_detectPin, INPUT);  // input only, no pull: the divider defines the level
    g_detectIdleLevel = digitalRead(g_detectPin) != 0;
    g_edgeN = 0;
    g_detectStartMs = millis();
    g_endMs = g_detectStartMs + window_ms;
    appendf("DETECT: listening to channel %c (GPIO%d) for %lu ms. Receive-only.\n", tagFor(channel),
            g_detectPin, static_cast<unsigned long>(window_ms));
    g_mode = Mode::Detecting;
    attachInterrupt(digitalPinToInterrupt(g_detectPin), onEdge, CHANGE);
    return true;
}

bool startCapture(int channel, uint32_t baud, int format, uint32_t secs, uint32_t gap_ms, char* error,
                  size_t error_len) {
    if (g_mutex == nullptr || g_log == nullptr) {
        setError(error, error_len, "analyzer not initialized");
        return false;
    }
    if (g_mode != Mode::Idle) {
        setError(error, error_len, "a detection or capture is running");
        return false;
    }
    if (channel < 0 || channel > 1) {
        setError(error, error_len, "channel must be 0 (A) or 1 (B)");
        return false;
    }
    if (baud < kMinBaud || baud > kMaxBaud) {
        setError(error, error_len, "baud out of range (300..230400)");
        return false;
    }
    if (format < k8N1 || format > k8O1) {
        setError(error, error_len, "format must be 0 (8N1), 1 (8E1) or 2 (8O1)");
        return false;
    }
    if (secs < 1) secs = 1;
    if (secs > kMaxSecs) secs = kMaxSecs;
    if (gap_ms < kMinGapMs) gap_ms = kMinGapMs;
    if (gap_ms > 1000) gap_ms = 1000;

    resetLog();
    delete g_collector;
    g_collector = new uart_sniff::FrameCollector(g_frames, g_frameCap, gap_ms * 1000u);
    g_logged = 0;
    g_channel = channel;
    g_baud = baud;

    static const uint32_t kConfigs[] = {SERIAL_8N1, SERIAL_8E1, SERIAL_8O1};
    g_port = (channel == 0) ? &Serial1 : &Serial2;
    g_port->setRxBufferSize(2048);                        // must precede begin()
    g_port->begin(baud, kConfigs[format], pinFor(channel), -1);  // txPin = -1: TX is never attached
    while (g_port->available()) g_port->read();           // drop anything stale

    static const char* const kNames[] = {"8N1", "8E1", "8O1"};
    appendf("CAPTURE: channel %c (GPIO%d), %lu baud %s, %lu s, frame gap %lu ms. Receive-only.\n",
            tagFor(channel), pinFor(channel), static_cast<unsigned long>(baud), kNames[format],
            static_cast<unsigned long>(secs), static_cast<unsigned long>(gap_ms));
    g_startMs = millis();
    g_endMs = g_startMs + secs * 1000UL;
    g_mode = Mode::Capturing;
    return true;
}

void stop() {
    if (g_mode == Mode::Detecting) finishDetect("stopped by user");
    else if (g_mode == Mode::Capturing) finishCapture("stopped by user");
}

void poll() {
    if (g_mode == Mode::Detecting) {
        if (g_edgeN >= kEdgeCap) {
            finishDetect("edge buffer full");
        } else if (static_cast<int32_t>(millis() - g_endMs) >= 0) {
            finishDetect("time elapsed");
        }
    } else if (g_mode == Mode::Capturing) {
        if (static_cast<int32_t>(millis() - g_endMs) >= 0) finishCapture("time elapsed");
    }
}

Status status() {
    Status st{};
    st.mode = g_mode;
    st.channel = g_channel;
    st.baud = g_baud;
    st.last_detect_baud = g_lastDetectBaud;
    if (g_mode != Mode::Idle) {
        const int32_t left = static_cast<int32_t>(g_endMs - millis());
        st.secs_left = left > 0 ? static_cast<uint32_t>(left / 1000) : 0;
    }
    if (g_mutex) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        if (g_collector) {
            st.bytes = g_collector->totalBytes();
            st.frames = static_cast<uint32_t>(g_collector->count());
        }
        st.truncated = g_truncated;
        xSemaphoreGive(g_mutex);
    }
    return st;
}

bool isActive() { return g_mode != Mode::Idle; }

String textFrom(size_t offset, size_t max_chars) {
    String out;
    if (g_mutex == nullptr) return out;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    if (offset < g_logLen) {
        size_t n = g_logLen - offset;
        if (n > max_chars) n = max_chars;
        out.reserve(n);
        for (size_t i = 0; i < n; i++) out += g_log[offset + i];
    }
    xSemaphoreGive(g_mutex);
    return out;
}

size_t textSize() { return g_logLen; }

}  // namespace uart_analyzer
