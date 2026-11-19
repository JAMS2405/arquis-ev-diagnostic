# uart

**Estado: implementado (Etapa 2), compila, NO validado en hardware.**

UART Analyzer **pasivo (solo recepción)**. Detecta la velocidad por temporización de flancos y captura tramas, con análisis de bytes variables y detección de checksum (`lib/uart_sniff`). Guía de montaje y uso: `docs/UART_TAP.md`.

Garantías (verificadas por `tools/check_ble_write_policy.py`):
- No hay pin TX asignado: `pins_config.h` solo define pines RX.
- Los UART se abren con `txPin = -1`: el pad de TX nunca se conecta (comprobado en el código del core 2.0.17).
- Los pines se configuran solo como `INPUT`, sin pull-up ni pull-down.
- Ninguna llamada `write`/`print` sobre un puerto, ni `digitalWrite`, ni llamadas IDF que manejen pines.

**Eléctrico:** la entrada debe pasar por el divisor protegido de `docs/UART_TAP.md`. Nunca conectar una señal del vehículo directamente a un GPIO.
