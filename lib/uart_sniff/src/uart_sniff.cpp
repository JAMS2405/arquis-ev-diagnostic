#include "uart_sniff.h"

#include <stdio.h>
#include <string.h>

namespace uart_sniff {

// ------------------------------------------------------------------ baud ----

namespace {

const uint32_t kBauds[] = {1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 76800, 115200, 230400};
const size_t kBaudCount = sizeof(kBauds) / sizeof(kBauds[0]);

// Smallest values with the ~2 % smallest ones dropped (glitch rejection).
uint32_t robustMin(const uint32_t* w, size_t n) {
    const size_t k = n / 50 + 1;  // number of smallest samples to skip
    uint32_t small[64];
    size_t have = 0;
    const size_t keep = k < 64 ? k + 1 : 64;
    for (size_t i = 0; i < n; i++) {
        const uint32_t v = w[i];
        if (have < keep) {
            size_t j = have++;
            while (j > 0 && small[j - 1] > v) { small[j] = small[j - 1]; j--; }
            small[j] = v;
        } else if (v < small[keep - 1]) {
            size_t j = keep - 1;
            while (j > 0 && small[j - 1] > v) { small[j] = small[j - 1]; j--; }
            small[j] = v;
        }
    }
    return small[have > k ? k : have - 1];
}

float scoreBaud(const uint32_t* w, size_t n, uint32_t baud) {
    const float T = 1.0e6f / static_cast<float>(baud);
    float tol = 0.2f * T;
    if (tol < 2.5f) tol = 2.5f;      // timestamp jitter floor (interrupt latency)
    if (tol > 0.35f * T) tol = 0.35f * T;

    size_t considered = 0, aligned = 0, singles = 0;
    for (size_t i = 0; i < n; i++) {
        const float width = static_cast<float>(w[i]);
        if (width > 10.0f * T) continue;  // idle gap, not part of a character
        considered++;
        const float k = static_cast<float>(static_cast<int>(width / T + 0.5f));
        const float err = width - k * T;
        if (k >= 1.0f && (err < 0 ? -err : err) <= tol) aligned++;
        if (width >= 0.75f * T && width <= 1.25f * T) singles++;
    }
    if (considered < 10) return 0.0f;
    // A candidate faster than the real baud aligns everything; real data must show
    // single-bit pulses, which a too-fast candidate never sees.
    if (static_cast<float>(singles) / static_cast<float>(considered) < 0.02f) return 0.0f;
    return static_cast<float>(aligned) / static_cast<float>(considered);
}

}  // namespace

void estimateBaud(const uint32_t* widths_us, size_t n, BaudEstimate& out) {
    memset(&out, 0, sizeof(out));
    if (widths_us == nullptr || n < 20) return;

    out.min_width_us = robustMin(widths_us, n);

    BaudCandidate all[12];
    for (size_t i = 0; i < kBaudCount; i++) {
        all[i].baud = kBauds[i];
        all[i].score = scoreBaud(widths_us, n, kBauds[i]);
    }
    // Select the best kMaxCandidates by score (ties: lower baud first).
    bool used[12] = {false};
    for (size_t t = 0; t < kMaxCandidates; t++) {
        int best = -1;
        for (size_t i = 0; i < kBaudCount; i++) {
            if (used[i] || all[i].score <= 0.0f) continue;
            if (best < 0 || all[i].score > all[best].score) best = static_cast<int>(i);
        }
        if (best < 0) break;
        used[best] = true;
        out.top[out.top_count++] = all[best];
    }
    out.pulses_considered = n;
    out.confident = out.top_count > 0 && out.top[0].score >= 0.9f &&
                    (out.top_count == 1 || out.top[1].score <= out.top[0].score - 0.15f);
}

// ---------------------------------------------------------------- frames ----

FrameCollector::FrameCollector(Frame* storage, size_t capacity, uint32_t gap_us)
    : frames_(storage), capacity_(capacity), gap_us_(gap_us), count_(0), dropped_(0),
      total_bytes_(0), open_(false), last_byte_us_(0), first_us_(0), have_first_(false) {
    memset(&cur_, 0, sizeof(cur_));
}

void FrameCollector::closeOpen() {
    if (!open_) return;
    if (count_ < capacity_) {
        frames_[count_++] = cur_;
    } else {
        dropped_++;
    }
    open_ = false;
}

void FrameCollector::addByte(uint8_t b, uint32_t t_us) {
    if (!have_first_) {
        have_first_ = true;
        first_us_ = t_us;
    }
    if (open_ && static_cast<uint32_t>(t_us - last_byte_us_) > gap_us_) closeOpen();
    if (!open_) {
        memset(&cur_, 0, sizeof(cur_));
        cur_.t_ms = (t_us - first_us_) / 1000u;
        open_ = true;
    }
    if (cur_.len < kMaxFrameBytes) {
        cur_.data[cur_.len++] = b;
    } else {
        cur_.truncated = true;
    }
    last_byte_us_ = t_us;
    total_bytes_++;
}

void FrameCollector::closeIfIdle(uint32_t now_us, bool force) {
    if (!open_) return;
    if (force || static_cast<uint32_t>(now_us - last_byte_us_) > gap_us_) closeOpen();
}

// -------------------------------------------------------------- analysis ----

void analyze(const Frame* frames, size_t n, GroupAnalysis& out) {
    memset(&out, 0, sizeof(out));
    out.total_frames = n;
    if (frames == nullptr || n == 0) return;

    // Most frequent (len, first byte) among non-empty frames. O(n * shapes) with a
    // small candidate table.
    struct Shape { uint8_t len, first; size_t count; };
    Shape shapes[16];
    size_t shape_n = 0;
    for (size_t i = 0; i < n; i++) {
        if (frames[i].len == 0) continue;
        size_t s = 0;
        for (; s < shape_n; s++) {
            if (shapes[s].len == frames[i].len && shapes[s].first == frames[i].data[0]) break;
        }
        if (s == shape_n) {
            if (shape_n == 16) continue;  // ignore rare shapes beyond the table
            shapes[shape_n++] = Shape{frames[i].len, frames[i].data[0], 0};
        }
        shapes[s].count++;
    }
    if (shape_n == 0) return;
    size_t best = 0;
    for (size_t s = 1; s < shape_n; s++) {
        if (shapes[s].count > shapes[best].count) best = s;
    }
    out.len = shapes[best].len;
    out.first_byte = shapes[best].first;

    uint8_t seen[kMaxFrameBytes][32];
    memset(seen, 0, sizeof(seen));
    bool init[kMaxFrameBytes] = {false};
    uint32_t first_t = 0, last_t = 0;
    size_t in_group = 0;
    for (size_t i = 0; i < n; i++) {
        const Frame& f = frames[i];
        if (f.len != out.len || f.data[0] != out.first_byte) continue;
        if (in_group == 0) first_t = f.t_ms;
        last_t = f.t_ms;
        in_group++;
        for (uint8_t p = 0; p < out.len; p++) {
            const uint8_t v = f.data[p];
            if (!init[p]) {
                init[p] = true;
                out.pos[p].min = out.pos[p].max = v;
            } else {
                if (v < out.pos[p].min) out.pos[p].min = v;
                if (v > out.pos[p].max) out.pos[p].max = v;
            }
            uint8_t& cell = seen[p][v >> 3];
            const uint8_t mask = static_cast<uint8_t>(1u << (v & 7));
            if (!(cell & mask)) {
                cell |= mask;
                if (out.pos[p].distinct < 255) out.pos[p].distinct++;
            }
        }
    }
    out.frames_in_group = in_group;
    for (uint8_t p = 0; p < out.len; p++) {
        out.pos[p].varies = out.pos[p].distinct > 1;
        if (out.pos[p].varies) out.varying_count++;
    }
    out.avg_period_ms = in_group > 1 ? (last_t - first_t) / static_cast<uint32_t>(in_group - 1) : 0;
    out.valid = true;
}

void formatTemplate(const GroupAnalysis& g, char* out, size_t out_len) {
    if (out == nullptr || out_len == 0) return;
    out[0] = '\0';
    if (!g.valid) return;
    size_t pos = 0;
    for (uint8_t p = 0; p < g.len && pos + 4 < out_len; p++) {
        int w;
        if (g.pos[p].varies) {
            w = snprintf(out + pos, out_len - pos, "?? ");
        } else {
            w = snprintf(out + pos, out_len - pos, "%02X ", g.pos[p].min);
        }
        if (w <= 0) break;
        pos += static_cast<size_t>(w);
    }
    if (pos > 0) out[pos - 1] = '\0';  // drop trailing space
}

// -------------------------------------------------------------- checksum ----

const char* schemeName(ChecksumScheme s) {
    switch (s) {
        case ChecksumScheme::Sum8:       return "sum8";
        case ChecksumScheme::NegSum8:    return "two's-complement sum8";
        case ChecksumScheme::NotSum8:    return "inverted sum8 (0xFF - sum)";
        case ChecksumScheme::Xor8:       return "xor8";
        case ChecksumScheme::Sum16LE:    return "sum16 little-endian";
        case ChecksumScheme::Sum16BE:    return "sum16 big-endian";
        case ChecksumScheme::NegSum16LE: return "two's-complement sum16 little-endian";
        case ChecksumScheme::NegSum16BE: return "two's-complement sum16 big-endian";
        case ChecksumScheme::NotSum16LE: return "inverted sum16 little-endian (0xFFFF - sum)";
        case ChecksumScheme::NotSum16BE: return "inverted sum16 big-endian (0xFFFF - sum)";
        default:                         return "none";
    }
}

namespace {

bool frameMatches(const Frame& f, ChecksumScheme s, uint8_t start) {
    const bool two = s >= ChecksumScheme::Sum16LE;
    const uint8_t cks = two ? 2 : 1;
    if (f.len < static_cast<uint8_t>(start + cks + 1)) return false;
    const uint8_t end = static_cast<uint8_t>(f.len - cks);  // exclusive end of summed region

    uint32_t sum = 0;
    uint8_t x = 0;
    for (uint8_t i = start; i < end; i++) {
        sum += f.data[i];
        x ^= f.data[i];
    }
    const uint8_t lo = f.data[end];
    const uint8_t hi = two ? f.data[end + 1] : 0;
    const uint16_t le = static_cast<uint16_t>(lo | (hi << 8));
    const uint16_t be = static_cast<uint16_t>((lo << 8) | hi);

    switch (s) {
        case ChecksumScheme::Sum8:       return lo == static_cast<uint8_t>(sum);
        case ChecksumScheme::NegSum8:    return lo == static_cast<uint8_t>(0x100 - (sum & 0xFF));
        case ChecksumScheme::NotSum8:    return lo == static_cast<uint8_t>(~sum);
        case ChecksumScheme::Xor8:       return lo == x;
        case ChecksumScheme::Sum16LE:    return le == static_cast<uint16_t>(sum);
        case ChecksumScheme::Sum16BE:    return be == static_cast<uint16_t>(sum);
        case ChecksumScheme::NegSum16LE: return le == static_cast<uint16_t>(0x10000 - (sum & 0xFFFF));
        case ChecksumScheme::NegSum16BE: return be == static_cast<uint16_t>(0x10000 - (sum & 0xFFFF));
        case ChecksumScheme::NotSum16LE: return le == static_cast<uint16_t>(~sum);
        case ChecksumScheme::NotSum16BE: return be == static_cast<uint16_t>(~sum);
        default:                         return false;
    }
}

}  // namespace

bool detectChecksum(const Frame* frames, size_t n, const GroupAnalysis& g, ChecksumHit& out) {
    out.scheme = ChecksumScheme::None;
    out.start = 0;
    if (!g.valid || g.frames_in_group < 3 || g.varying_count == 0) return false;

    // Frames of the group, and proof that they are not all identical.
    const Frame* first = nullptr;
    bool distinct = false;
    size_t in_group = 0;
    for (size_t i = 0; i < n; i++) {
        const Frame& f = frames[i];
        if (f.len != g.len || f.data[0] != g.first_byte) continue;
        in_group++;
        if (!first) first = &f;
        else if (memcmp(first->data, f.data, f.len) != 0) distinct = true;
    }
    if (in_group < 3 || !distinct) return false;

    for (uint8_t s = static_cast<uint8_t>(ChecksumScheme::Sum8);
         s <= static_cast<uint8_t>(ChecksumScheme::NotSum16BE); s++) {
        for (uint8_t start = 0; start <= 3; start++) {
            bool all = true;
            for (size_t i = 0; i < n && all; i++) {
                const Frame& f = frames[i];
                if (f.len != g.len || f.data[0] != g.first_byte) continue;
                all = frameMatches(f, static_cast<ChecksumScheme>(s), start);
            }
            // A checksum field that never changes proves nothing (e.g. a constant
            // last byte can equal a sum by coincidence): it must vary across frames.
            if (all) {
                const uint8_t cks = (static_cast<ChecksumScheme>(s) >= ChecksumScheme::Sum16LE) ? 2 : 1;
                bool field_varies = false;
                for (size_t i = 0; i < n && !field_varies; i++) {
                    const Frame& f = frames[i];
                    if (f.len != g.len || f.data[0] != g.first_byte) continue;
                    field_varies = memcmp(&f.data[g.len - cks], &first->data[g.len - cks], cks) != 0;
                }
                all = field_varies;
            }
            if (all) {
                out.scheme = static_cast<ChecksumScheme>(s);
                out.start = start;
                return true;
            }
        }
    }
    return false;
}

}  // namespace uart_sniff
