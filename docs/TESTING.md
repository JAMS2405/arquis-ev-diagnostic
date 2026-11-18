# Pruebas

Este documento separa estrictamente tres categorías. **No se registra como ejecutada una prueba que no se haya realizado, ni como validado hardware que no se haya probado físicamente.**

- **Ejecutadas:** se realizaron realmente; se indica dónde y con qué resultado.
- **Simuladas:** se realizaron sin hardware real (compilación, tests nativos, simulación). No validan el hardware.
- **Pendientes:** aún no realizadas.

---

## 1. Pruebas ejecutadas

| ID | Prueba | Entorno | Resultado | Fecha |
|---|---|---|---|---|
| — | Ninguna prueba sobre hardware físico ha sido ejecutada | — | — | — |

_Esta tabla se actualiza solo con resultados reales._

## 2. Pruebas simuladas / de compilación (sin hardware)

| ID | Prueba | Entorno | Resultado | Fecha |
|---|---|---|---|---|
| S-001 | Compilación del esqueleto de la Etapa 0 (`pio run`, env `esp32s3_n16r8`) | Contenedor Linux, PlatformIO Core 6.2.0 | Ver nota abajo | 2026-11-18 |

> Una compilación exitosa **solo demuestra que el código compila**. No valida arranque, pines, pantalla ni ningún hardware.

## 3. Pruebas pendientes

### Hardware base
- [ ] P-001 Arranque de la placa ESP32-S3-N16R8 y banner por serie.
- [ ] P-002 Verificación de 16 MB de flash y 8 MB de PSRAM reportados por el firmware.
- [ ] P-003 Identificación del modelo exacto de la placa y verificación del pinout.
- [ ] P-004 Pantalla ILI9341: inicialización, orientación, colores y resolución 240×320.

### UART Analyzer
- [ ] P-010 Captura de tráfico de una fuente de prueba conocida a 3,3 V.
- [ ] P-011 Estimación de velocidad en baudios.
- [ ] P-012 Medición con multímetro de los niveles lógicos de TX/RX de la Evobike S13-2.
- [ ] P-013 Captura pasiva real en la Evobike S13-2.
- [ ] P-014 Confirmación de que no existe transmisión hacia el vehículo.

### BLE Analyzer
- [ ] P-020 Descubrimiento de un dispositivo BLE de prueba.
- [ ] P-021 Lectura de servicios y características GATT.
- [ ] P-022 Descubrimiento/exploración de BLE en la Evobike S13-2 (solo lectura).

### Electrical Monitor
- [ ] P-030 Calibración del ADC con fuente de referencia y multímetro.
- [ ] P-031 Validación de la protección de entrada (límites máximos).
- [ ] P-032 Medición de una señal real de baja tensión del vehículo.

### Integración
- [ ] P-040 Operación concurrente de UART, BLE y ADC.
- [ ] P-041 Persistencia de capturas tras reinicio.

---

## Convención de registro

Cada prueba ejecutada debe registrar: ID, fecha, versión del firmware (commit), hardware/entorno, procedimiento, resultado esperado, resultado observado y conclusión (pasa/falla).
