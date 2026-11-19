#include "panel_text.h"

#include <stdio.h>
#include <string.h>

namespace panel_text {

namespace {

char channelChar(int ch) { return ch == 1 ? 'B' : 'A'; }

// Buffer size to use so that at most `max_chars` characters are ever written,
// whatever the caller's buffer size or the input values.
size_t lim(size_t n, size_t max_chars) { return n > max_chars + 1 ? max_chars + 1 : n; }

void put(char* out, size_t n, const char* text) {
    if (out == nullptr || n == 0) return;
    strncpy(out, text, n - 1);
    out[n - 1] = '\0';
}

}  // namespace

void bytesText(char* out, size_t n, uint32_t bytes) {
    if (out == nullptr || n == 0) return;
    if (bytes < 1024u) {
        snprintf(out, n, "%lu B", static_cast<unsigned long>(bytes));
    } else if (bytes < 1024u * 1024u) {
        snprintf(out, n, "%lu KB", static_cast<unsigned long>(bytes / 1024u));
    } else {
        const unsigned long mb = bytes / (1024u * 1024u);
        const unsigned long frac = (bytes % (1024u * 1024u)) * 100u / (1024u * 1024u);
        snprintf(out, n, "%lu.%02lu MB", mb, frac);
    }
}

void uptime(char* out, size_t n, uint32_t secs) {
    if (out == nullptr || n == 0) return;
    snprintf(out, n, "%02lu:%02lu:%02lu", static_cast<unsigned long>(secs / 3600u),
             static_cast<unsigned long>((secs / 60u) % 60u), static_cast<unsigned long>(secs % 60u));
}

void bleTitle(char* out, size_t n, const BleState& s) {
    n = lim(n, kTitleMax);
    if (s.listening) put(out, n, "BLE: Escuchando");
    else if (s.scanning) put(out, n, "BLE: Escaneando");
    else put(out, n, "BLE: Libre");
}

void bleDetail(char* out, size_t n, const BleState& s) {
    n = lim(n, kDetailMax);
    if (out == nullptr || n == 0) return;
    if (s.listening) {
        char b[16];
        bytesText(b, sizeof(b), s.bytes);
        snprintf(out, n, "quedan %lus  %lu notif  %s", static_cast<unsigned long>(s.listen_secs_left),
                 static_cast<unsigned long>(s.notifications), b);
    } else if (s.scanning) {
        put(out, n, "Buscando dispositivos...");
    } else if (s.devices > 0) {
        snprintf(out, n, "Ultimo escaneo: %d dispositivos", s.devices);
    } else {
        put(out, n, "Sin escaneo todavia");
    }
}

void uartTitle(char* out, size_t n, const UartState& s) {
    n = lim(n, kTitleMax);
    if (out == nullptr || n == 0) return;
    if (s.mode == 1) snprintf(out, n, "UART: Detecta %c", channelChar(s.channel));
    else if (s.mode == 2) snprintf(out, n, "UART: Captura %c", channelChar(s.channel));
    else put(out, n, "UART: Libre");
}

void uartDetail(char* out, size_t n, const UartState& s) {
    n = lim(n, kDetailMax);
    if (out == nullptr || n == 0) return;
    if (s.mode == 1) {
        snprintf(out, n, "midiendo flancos  quedan %lus", static_cast<unsigned long>(s.secs_left));
    } else if (s.mode == 2) {
        char b[16];
        bytesText(b, sizeof(b), s.bytes);
        snprintf(out, n, "%lu baud %lu tramas %s %lus", static_cast<unsigned long>(s.baud),
                 static_cast<unsigned long>(s.frames), b, static_cast<unsigned long>(s.secs_left));
    } else if (s.last_detect_baud > 0) {
        snprintf(out, n, "Ultima velocidad: %lu baud", static_cast<unsigned long>(s.last_detect_baud));
    } else if (s.frames > 0) {
        snprintf(out, n, "Ultima captura: %lu tramas", static_cast<unsigned long>(s.frames));
    } else {
        put(out, n, "Solo recibe. Sin captura todavia");
    }
}

void bmsTitle(char* out, size_t n, const BmsState& s) {
    n = lim(n, kTitleMax);
    if (out == nullptr || n == 0) return;
    if (s.have) {
        snprintf(out, n, "BMS: %lu.%02lu V", static_cast<unsigned long>(s.voltage_cv / 100u),
                 static_cast<unsigned long>(s.voltage_cv % 100u));
    } else {
        put(out, n, "BMS: sin lectura");
    }
}

void bmsDetail(char* out, size_t n, const BmsState& s) {
    n = lim(n, kDetailMax);
    if (out == nullptr || n == 0) return;
    if (!s.have) {
        put(out, n, "Usa Reporte BMS JBD en la web");
    } else if (s.protection_flags != 0) {
        snprintf(out, n, "PROTECCION ACTIVA  %u ciclos", static_cast<unsigned>(s.cycles));
    } else {
        snprintf(out, n, "%u ciclos  %u%%  dif %u mV", static_cast<unsigned>(s.cycles),
                 static_cast<unsigned>(s.rsoc), static_cast<unsigned>(s.delta_mv));
    }
}

void hint(char* out, size_t n, const HintState& s) {
    n = lim(n, kDetailMax);
    if (s.uart_busy) put(out, n, "UART: solo recibe, no envia nada");
    else if (s.ble_busy) put(out, n, "BLE en curso: espera a que termine");
    else if (s.clients <= 0) put(out, n, "Conecta el telefono al WiFi");
    else put(out, n, "Abre la direccion en el navegador");
}

}  // namespace panel_text
