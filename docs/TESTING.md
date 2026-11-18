# Pruebas

Este documento separa estrictamente tres categorías. **No se registra como ejecutada una prueba que no se haya realizado, ni como validado hardware que no se haya probado físicamente.**

- **Ejecutadas:** se realizaron realmente; se indica dónde y con qué resultado.
- **Simuladas:** se realizaron sin hardware real (compilación, tests nativos, simulación). No validan el hardware.
- **Pendientes:** aún no realizadas.

---

## 1. Pruebas ejecutadas

| ID | Prueba | Entorno | Resultado | Fecha |
|---|---|---|---|---|
| H-001 | Primer flasheo de la Etapa 1 (commit `28be70d`) y monitor serie en COM5 a 115200 | ESP32-S3 real, conector USB y cableado de la TFT sin confirmar | **Fallo / no concluyente**: sin salida en el monitor tras RESET y la pantalla TFT no encendió. Causa sin determinar. Hipótesis: `Serial` iba solo al USB nativo (`CDC_ON_BOOT=1`) y el conector usado pudo ser el del puente UART; cableado de la TFT a los pines provisionales sin confirmar. | 2026-11-18 |
| H-002 | Segundo flasheo de la Etapa 1 (commit `60d2feb`, consola dual) y monitor serie en COM4 a 115200 | ESP32-S3 real, informado por el usuario | **Pasa (arranque y memoria)**. Salida literal: `ARQUIS EV DIAGNOSTIC v0.1.0-stage1`; `Chip: ESP32-S3 rev 0, 2 core(s)`; `Flash: 16777216 bytes` (16 MB); `PSRAM: 8386279 bytes` (≈8 MB); `Heap: 366848 bytes free`. Cierra P-001 y P-002. | 2026-11-18 |
| H-003 | Pantalla TFT con el firmware de H-002 | ESP32-S3 real, informado por el usuario | **Parcial, con defectos de software**: la foto del usuario muestra la pantalla ILI9341 funcionando en vertical, con el uptime avanzando (`00:02:21`), Heap 358 KB, PSRAM 7.99 MB y Flash 16.00 MB. Defectos observados: (1) el título se cortaba («ARQUIS EV DIAGNOSTI», la «C» pasaba a la línea siguiente); (2) el valor de FW se cortaba («0.1.0-stag») y la «e» invadía la fila de Uptime. Causa: texto más ancho que el área disponible. Corregido en el commit siguiente; pendiente de reflashear y confirmar (P-004). Por mostrar imagen, quedan funcionales SCLK, MOSI, CS, DC y RST con los pines de `pins_config.h`; MISO y BL (14) siguen sin confirmar. | 2026-11-18 |
| — | Resto de pruebas sobre hardware: ninguna ejecutada | — | — | — |

_Esta tabla se actualiza solo con resultados reales._

## 2. Pruebas simuladas / de compilación (sin hardware)

| ID | Prueba | Entorno | Resultado | Fecha |
|---|---|---|---|---|
| S-001 | Compilación del esqueleto de la Etapa 0 (`pio run`, env `esp32s3_n16r8`) | Contenedor Linux, PlatformIO Core 6.2.0, plataforma espressif32 + framework Arduino | **Pasa**: `[SUCCESS]`, 0 advertencias. RAM 7,0 % (22 808 B), Flash 4,5 % (298 188 B de 6 553 600 B de partición de app). Se generó `firmware.bin`. | 2026-11-18 |
| S-002 | Compilación del firmware de la Etapa 1 (display ILI9341 + informe de sistema), rama `feat/stage-1-display` | Contenedor Linux, PlatformIO Core 6.2.0; Adafruit GFX 1.13.0, Adafruit ILI9341 1.7.0 | **Pasa**: `[SUCCESS]`, 0 advertencias en el código del proyecto. RAM 7,0 % (23 048 B), Flash 4,7 % (311 268 B de 6 553 600 B). **No ejecutado en hardware.** | 2026-11-18 |

| S-004 | Compilación de la Etapa 3 inicial (BLE Analyzer + comandos serie), rama `feat/stage-3-ble` | Contenedor Linux, PlatformIO Core 6.2.0; NimBLE-Arduino 2.5.2 | **Pasa**: `[SUCCESS]`. RAM 13,3 % (43 616 B), Flash 8,9 % (581 515 B). 0 advertencias en `src/`; 1 advertencia dentro de la librería NimBLE (`availableForWrite` oculto). Comprobación por `grep` en `src/ble/`: sin `writeValue`, `subscribe`, `pair`, `setSecurity`, ni servidor GATT/advertising. **No ejecutado en hardware.** | 2026-11-18 |

| S-005 | Compilación de `feat/stage-3-ble` con la plataforma fijada `espressif32@7.0.1` (Arduino core 2.0.17), la misma que usa el PC del usuario | Contenedor Linux, PlatformIO Core 6.2.0 | **Pasa**: `[SUCCESS]`, RAM 12,3 % (40 420 B, idéntico al log del usuario), Flash 8,4 % (547 233 B). El usuario compiló la misma rama en su PC con el mismo resultado de memoria (el upload falló por COM4 ocupado, no por el firmware). **No ejecutado en hardware.** | 2026-11-18 |

| S-006 | Compilación con la interfaz web (`feat/web-ui`): AP Wi-Fi + servidor HTTP + escaneo BLE asíncrono | Contenedor Linux, PlatformIO Core 6.2.0, `espressif32@7.0.1` | **Pasa**: `[SUCCESS]`, 0 errores. RAM 19,0 % (62 248 B), Flash 14,8 % (972 049 B). También compila con `secrets.h` presente; `git` lo ignora. **No ejecutado en hardware.** | 2026-11-18 |
| S-007 | Página web en Chromium headless (Playwright) contra un servidor **simulado** (no el ESP32), viewport 390×800 | Contenedor Linux | **Pasa**: escaneo, lista de 3 dispositivos, botón GATT solo en los conectables, resultado, sin errores JS, sin scroll horizontal. Con nombres/UUID maliciosos (`<img onerror>`, `<script>`) se muestran como texto y no se ejecuta nada. **Valida la lógica de la página, no el firmware ni el Wi-Fi.** | 2026-11-18 |

> Una compilación exitosa **solo demuestra que el código compila**. No valida arranque, pines, pantalla ni ningún hardware.

## 3. Pruebas pendientes

### Hardware base
- [x] P-001 Arranque de la placa ESP32-S3-N16R8 y banner por serie (H-002).
- [x] P-002 Verificación de 16 MB de flash y 8 MB de PSRAM reportados por el firmware (H-002).
- [ ] P-003 Identificación del modelo exacto de la placa y verificación del pinout.
- [ ] P-004 Pantalla ILI9341: inicialización, orientación, colores y resolución 240×320 (parcial en H-003: hay imagen; falta confirmar el resto).

### UART Analyzer
- [ ] P-010 Captura de tráfico de una fuente de prueba conocida a 3,3 V.
- [ ] P-011 Estimación de velocidad en baudios.
- [ ] P-012 Medición con multímetro de los niveles lógicos de TX/RX de la Evobike S13-2.
- [ ] P-013 Captura pasiva real en la Evobike S13-2.
- [ ] P-014 Confirmación de que no existe transmisión hacia el vehículo.

### BLE Analyzer
- [ ] P-020 Descubrimiento de un dispositivo BLE de prueba (comando `scan`).
- [ ] P-021 Lectura de servicios y características GATT.
- [ ] P-022 Descubrimiento/exploración de BLE en la Evobike S13-2 (solo lectura).

### Electrical Monitor
- [ ] P-030 Calibración del ADC con fuente de referencia y multímetro.
- [ ] P-031 Validación de la protección de entrada (límites máximos).
- [ ] P-032 Medición de una señal real de baja tensión del vehículo.

### Interfaz web
- [ ] P-050 El punto de acceso aparece en el teléfono y acepta la clave mostrada en la TFT.
- [ ] P-051 La página carga en `http://192.168.4.1` en un teléfono real (Android/iOS).
- [ ] P-052 Escaneo BLE con Wi-Fi activo: comparar número de dispositivos con y sin cliente conectado.
- [ ] P-053 `Explorar GATT` desde el teléfono sobre un dispositivo de prueba.
- [ ] P-054 Estabilidad: 10 minutos con la web abierta sin reinicios ni cuelgues.

### Integración
- [ ] P-040 Operación concurrente de UART, BLE y ADC.
- [ ] P-041 Persistencia de capturas tras reinicio.

---

## Convención de registro

Cada prueba ejecutada debe registrar: ID, fecha, versión del firmware (commit), hardware/entorno, procedimiento, resultado esperado, resultado observado y conclusión (pasa/falla).
