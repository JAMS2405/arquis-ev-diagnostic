# Pinout

> **Estado: PROVISIONAL — NO VERIFICADO.**
> La fuente única de verdad en código es [`src/config/pins_config.h`](../src/config/pins_config.h). Este documento debe mantenerse sincronizado con ese archivo.
> Los pines no son definitivos hasta verificar el modelo físico de la placa (serigrafía/esquema) y probar cada conexión.

## Criterios de selección

Para ESP32-S3-N16R8 (flash 16 MB + PSRAM octal 8 MB) se evitan:

| GPIO | Motivo |
|---|---|
| 26–32 | Flash SPI interna |
| 33–37 | PSRAM octal (variante N16R8) |
| 19, 20 | USB nativo (D−/D+) |
| 0, 3, 45, 46 | Pines de strapping (arranque) |
| 43, 44 | UART0 por defecto (consola) |

Las entradas analógicas usan solo **ADC1** (GPIO1–10) para no entrar en conflicto con la radio.

## Asignación provisional

### Pantalla TFT ILI9341 (SPI)

| Señal | GPIO | Notas |
|---|:-:|---|
| SCLK | 12 | |
| MOSI | 11 | |
| MISO | 13 | Opcional |
| CS | 10 | |
| DC | 9 | |
| RST | 8 | |
| BL (backlight) | 14 | Depende del módulo |

### UART Analyzer (solo RX, pasivo)

| Señal | GPIO | Notas |
|---|:-:|---|
| Sniff A RX | 15 | Nivel a medir; adaptar a 3,3 V si es necesario |
| Sniff B RX | 16 | Ídem |
| TX | — | **No asignado a propósito** en v1 |

### Electrical Monitor (ADC1)

| Señal | GPIO | Notas |
|---|:-:|---|
| ADC_IN_0 | 1 | Siempre tras acondicionamiento protegido |
| ADC_IN_1 | 2 | Ídem |
| ADC_IN_2 | 4 | Ídem |

## Verificación pendiente

- [ ] Identificar modelo y revisión exacta de la placa DevKit.
- [ ] Confirmar que los GPIO propuestos están expuestos en los headers.
- [ ] Confirmar orden de pines del módulo TFT.
- [ ] Confirmar si el módulo TFT incluye lector SD (CS adicional).
- [ ] Probar continuidad y funcionamiento de cada señal; registrar en [`TESTING.md`](TESTING.md).
