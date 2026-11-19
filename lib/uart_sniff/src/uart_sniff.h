#pragma once
/**
 * Helpers for the PASSIVE UART analyzer (receive-only). Pure C++ (C++11), so
 * they can be unit-tested on the host with synthetic traces.
 *
 *  1. estimateBaud():   baud rate from the widths of consecutive line pulses.
 *  2. FrameCollector:   splits a timestamped byte stream into frames by idle gap.
 *  3. analyze():        which byte positions vary across the most common frame
 *                       shape (to locate status / fault bytes).
 *  4. detectChecksum(): tests common checksum schemes over a group of frames.
 */

#include <stddef.h>
#include <stdint.h>

namespace uart_sniff {

// ------------------------------------------------------------------ baud ----

constexpr size_t kMaxCandidates = 3;

struct BaudCandidate {
    uint32_t baud;
    float score;  // 0..1, fraction of pulses aligned to whole bit times
};

struct BaudEstimate {
    size_t pulses_considered;
    uint32_t min_width_us;  // robust minimum (ignores the smallest ~2 % as glitches)
    BaudCandidate top[kMaxCandidates];
    size_t top_count;
    bool confident;
};

/// `widths_us` are the durations of consecutive pulses (high or low) on the line.
/// Needs at least 20 pulses, otherwise top_count = 0.
void estimateBaud(const uint32_t* widths_us, size_t n, BaudEstimate& out);

// ---------------------------------------------------------------- frames ----

constexpr size_t kMaxFrameBytes = 64;

struct Frame {
    uint32_t t_ms;  // time of the first byte, relative to capture start
    uint8_t len;
    bool truncated;  // more than kMaxFrameBytes bytes arrived
    uint8_t data[kMaxFrameBytes];
};

class FrameCollector {
public:
    /// `storage` is owned by the caller. A new frame starts when the idle time
    /// between two bytes exceeds `gap_us`.
    FrameCollector(Frame* storage, size_t capacity, uint32_t gap_us);

    void addByte(uint8_t b, uint32_t t_us);

    /// Closes the open frame if the line has been idle for longer than the gap.
    /// Call periodically with the current time. Pass force=true at the end.
    void closeIfIdle(uint32_t now_us, bool force = false);

    size_t count() const { return count_; }
    const Frame& at(size_t i) const { return frames_[i]; }
    size_t dropped() const { return dropped_; }
    uint32_t totalBytes() const { return total_bytes_; }

private:
    void closeOpen();

    Frame* frames_;
    size_t capacity_;
    uint32_t gap_us_;
    size_t count_;
    size_t dropped_;
    uint32_t total_bytes_;
    bool open_;
    Frame cur_;
    uint32_t last_byte_us_;
    uint32_t first_us_;
    bool have_first_;
};

// -------------------------------------------------------------- analysis ----

struct PositionStat {
    uint8_t min;
    uint8_t max;
    uint8_t distinct;  // saturates at 255
    bool varies;
};

struct GroupAnalysis {
    bool valid;
    uint8_t len;
    uint8_t first_byte;
    size_t frames_in_group;
    size_t total_frames;
    uint8_t varying_count;
    uint32_t avg_period_ms;  // between frames of the group (0 if < 2)
    PositionStat pos[kMaxFrameBytes];
};

/// Finds the most frequent frame shape (same length and first byte) and computes
/// per-position variability over its frames.
void analyze(const Frame* frames, size_t n, GroupAnalysis& out);

/// Writes e.g. "55 AA ?? 21 ?? 00" (?? = varying position) into `out`.
void formatTemplate(const GroupAnalysis& g, char* out, size_t out_len);

// -------------------------------------------------------------- checksum ----

enum class ChecksumScheme : uint8_t {
    None,
    Sum8,
    NegSum8,
    NotSum8,
    Xor8,
    Sum16LE,
    Sum16BE,
    NegSum16LE,
    NegSum16BE,
    NotSum16LE,
    NotSum16BE,
};

struct ChecksumHit {
    ChecksumScheme scheme;
    uint8_t start;  // first byte included in the sum
};

const char* schemeName(ChecksumScheme s);

/// Tests whether the last 1-2 bytes of EVERY frame in the group are a checksum of
/// the bytes before them (from a start offset of 0..3). Requires >= 3 frames in the
/// group and at least 2 different frame contents. Returns true with the first scheme
/// that matches all of them.
bool detectChecksum(const Frame* frames, size_t n, const GroupAnalysis& g, ChecksumHit& out);

}  // namespace uart_sniff
