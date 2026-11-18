#pragma once
/**
 * Dual console: mirrors output to UART0 (USB-UART bridge connector, "COM")
 * and to the native USB CDC (connector "USB").
 *
 * Many ESP32-S3 DevKit clones have two USB connectors and it is not known in
 * advance which one the user connects; writing to both avoids a silent console.
 */

#include <Arduino.h>
#include <HWCDC.h>

namespace console {

class Dual : public Print {
public:
    void begin(unsigned long baud) {
        Serial.begin(baud);        // UART0 -> USB-UART bridge connector
        cdc_.begin();             // native USB CDC connector
        cdc_.setTxTimeoutMs(0);   // never block when no host is attached
    }

    size_t write(uint8_t c) override {
        Serial.write(c);
        cdc_.write(c);
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        Serial.write(buffer, size);
        cdc_.write(buffer, size);
        return size;
    }

private:
    HWCDC cdc_;  // own instance: HWCDCSerial only exists when CDC_ON_BOOT=1
};

inline Dual& out() {
    static Dual instance;
    return instance;
}

}  // namespace console
