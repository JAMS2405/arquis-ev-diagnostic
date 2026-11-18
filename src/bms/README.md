# bms

**Estado: implementado (Etapa 3c), compila, NO validado en un BMS real.**

Reporte de un BMS **JBD (Xiaoxiang)** por BLE. Alcance ampliado por decisión del usuario, solo para baterías propias. Ver `docs/BMS_JBD.md`.

Es el **único módulo del proyecto que escribe en un dispositivo BLE**, y de forma muy acotada:
- Escribe solo en la característica `0xFF02` del servicio `0xFF00`.
- Solo tramas de `jbd::buildReadRequest()`: 3 peticiones fijas de **lectura** (`0x03` info básica, `0x04` celdas, `0x05` nombre de hardware).
- La única otra escritura es la suscripción (CCCD) a `0xFF01` para recibir las respuestas.
- No existe forma de enviar bytes arbitrarios; no se pueden construir tramas de configuración ni de control de MOSFET.

`tools/check_ble_write_policy.py` comprueba que ningún otro archivo llame a `writeValue`/`subscribe`.
