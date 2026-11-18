# ble

**Estado: implementado (Etapa 3, versión inicial), compila, NO validado en hardware.**

BLE Analyzer de **solo lectura** con NimBLE-Arduino.

- `scan`: escaneo activo; muestra dirección, RSSI, conectable, nombre, UUID de servicios anunciados y datos de fabricante.
- `gatt <n>`: se conecta al dispositivo `n`, enumera servicios y características y lee **solo** las que declaran la propiedad READ. Se desconecta al terminar.

Garantías de diseño (verificables con `grep` en este directorio):
- **Sin escrituras** de características ni descriptores.
- **Sin suscripciones** a notificaciones/indicaciones (suscribirse escribe el CCCD).
- **Sin emparejamiento** ni seguridad.
- El ESP32 no anuncia ni crea servidor GATT: solo observa.

Control por el monitor serie (la TFT no es táctil): `help`, `info`, `scan [s]`, `list`, `gatt <n>`.

Limitaciones: un escaneo bloquea el bucle (el uptime de la pantalla se detiene unos segundos). Un periférico conectado a otro cliente (p. ej. la app del móvil) normalmente deja de anunciarse.
