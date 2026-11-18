#pragma once
/**
 * Formatting and statistics for captured BLE notifications (listen-only mode).
 * Pure C++ so it can be unit-tested on the host.
 */

#include <stddef.h>
#include <stdint.h>

namespace notify_log {

struct Stats {
    uint32_t count;
    uint32_t bytes;
    uint16_t min_len;
    uint16_t max_len;
    uint32_t first_ms;
    uint32_t last_ms;
    uint16_t first_byte_count[256];  // saturating counters
};

void reset(Stats& s);

/// Accounts for one notification of `len` bytes received at `t_ms`.
void add(Stats& s, const uint8_t* data, size_t len, uint32_t t_ms);

struct TopByte {
    uint8_t value;
    uint16_t count;
};

/// Fills `out` with up to `n` most frequent first bytes (descending). Returns how many.
size_t topFirstBytes(const Stats& s, TopByte* out, size_t n);

/// Average interval between notifications in ms (0 if fewer than 2).
uint32_t averageIntervalMs(const Stats& s);

/// Writes one log line: "[ t ms] tag  N B  HH HH ... | ascii\n".
/// Shows at most `max_bytes` of payload (adds "+K more" when cut).
/// Always NUL-terminates; returns the number of characters written (excl. NUL).
size_t formatLine(char* out, size_t out_len, uint32_t t_ms, const char* tag, const uint8_t* data,
                  size_t len, size_t max_bytes);

}  // namespace notify_log
