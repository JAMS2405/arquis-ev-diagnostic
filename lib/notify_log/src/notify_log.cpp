#include "notify_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace notify_log {

void reset(Stats& s) { memset(&s, 0, sizeof(s)); }

void add(Stats& s, const uint8_t* data, size_t len, uint32_t t_ms) {
    if (s.count == 0) {
        s.first_ms = t_ms;
        s.min_len = s.max_len = static_cast<uint16_t>(len);
    } else {
        if (len < s.min_len) s.min_len = static_cast<uint16_t>(len);
        if (len > s.max_len) s.max_len = static_cast<uint16_t>(len);
    }
    s.last_ms = t_ms;
    s.count++;
    s.bytes += static_cast<uint32_t>(len);
    if (len > 0 && data != nullptr && s.first_byte_count[data[0]] < 0xFFFF) {
        s.first_byte_count[data[0]]++;
    }
}

size_t topFirstBytes(const Stats& s, TopByte* out, size_t n) {
    size_t found = 0;
    for (size_t k = 0; k < n; k++) {
        int best = -1;
        uint16_t best_count = 0;
        for (int v = 0; v < 256; v++) {
            const uint16_t c = s.first_byte_count[v];
            if (c <= best_count) continue;
            bool taken = false;
            for (size_t j = 0; j < found; j++) {
                if (out[j].value == v) { taken = true; break; }
            }
            if (!taken) { best = v; best_count = c; }
        }
        if (best < 0) break;
        out[found].value = static_cast<uint8_t>(best);
        out[found].count = best_count;
        found++;
    }
    return found;
}

uint32_t averageIntervalMs(const Stats& s) {
    if (s.count < 2) return 0;
    return (s.last_ms - s.first_ms) / (s.count - 1);
}

namespace {

// printf-style append that never writes past `out_len` and keeps `out` NUL-terminated.
void appendf(char* out, size_t out_len, size_t& pos, const char* fmt, ...) {
    if (pos + 1 >= out_len) return;
    va_list args;
    va_start(args, fmt);
    const int w = vsnprintf(out + pos, out_len - pos, fmt, args);
    va_end(args);
    if (w <= 0) return;
    const size_t room = out_len - pos - 1;
    pos += (static_cast<size_t>(w) < room) ? static_cast<size_t>(w) : room;
}

}  // namespace

size_t formatLine(char* out, size_t out_len, uint32_t t_ms, const char* tag, const uint8_t* data,
                  size_t len, size_t max_bytes) {
    if (out == nullptr || out_len == 0) return 0;
    out[0] = '\0';
    size_t pos = 0;

    appendf(out, out_len, pos, "[%7lu ms] %s %3u B  ", static_cast<unsigned long>(t_ms),
            tag ? tag : "?", static_cast<unsigned>(len));

    const size_t shown = len < max_bytes ? len : max_bytes;
    for (size_t i = 0; i < shown; i++) appendf(out, out_len, pos, "%02X ", data[i]);
    if (len > shown) appendf(out, out_len, pos, "+%u more ", static_cast<unsigned>(len - shown));

    appendf(out, out_len, pos, "| ");
    for (size_t i = 0; i < shown; i++) {
        const char c = (data[i] >= 32 && data[i] < 127) ? static_cast<char>(data[i]) : '.';
        appendf(out, out_len, pos, "%c", c);
    }
    appendf(out, out_len, pos, "\n");
    return pos;
}

}  // namespace notify_log
