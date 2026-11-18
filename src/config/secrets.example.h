#pragma once
/**
 * Local secrets template.
 *
 * To set a FIXED Wi-Fi password for the web UI, copy this file to
 * `src/config/secrets.h` (it is git-ignored) and edit it. NEVER commit
 * secrets.h.
 *
 * If `secrets.h` does not exist, the firmware generates a random password at
 * every boot and shows it on the TFT and the serial console.
 */

namespace secrets {
// WPA2 password, 8..63 characters.
constexpr const char* WIFI_PASSWORD = "change-me-please";
}  // namespace secrets
