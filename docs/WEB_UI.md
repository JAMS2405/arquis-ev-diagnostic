# Interfaz web (teléfono)

> **Estado: implementada y compilando; NO validada en hardware.** Ver `TESTING.md` (S-006, S-007 y pruebas P-050 a P-054 pendientes).

## Uso

1. Enciende el ESP32. En la pantalla TFT aparece el bloque **PHONE WEB** con el nombre de la red, la clave y la dirección. Lo mismo sale por el monitor serie (comando `wifi`).
2. En el teléfono, conéctate a la red **`ARQUIS-EV-DIAG`** con esa clave. Android puede avisar de que «no hay internet»: es normal; elige mantener la conexión.
3. Abre **`http://192.168.4.1`** en el navegador.
4. Pulsa **Escanear**, elige un dispositivo conectable y **Explorar GATT (solo lectura)**.
5. Puedes **copiar** o **descargar** el resultado como `.txt`.

## Clave de la red

- **Por defecto:** se genera una clave aleatoria de 10 caracteres **en cada arranque** y se muestra en la TFT y por serie. No hay ninguna clave fija en el repositorio.
- **Clave fija (opcional):** copia `src/config/secrets.example.h` a `src/config/secrets.h` y edítala (8 a 63 caracteres). `secrets.h` está en `.gitignore`: **no lo subas**. La clave queda dentro del binario compilado.
- La red es WPA2, con máximo 2 clientes.

## API (todo local, sin escritura al vehículo)

| Método y ruta | Función |
|---|---|
| `GET /` | La página |
| `GET /api/status` | Versión, uptime, memoria, si hay un escaneo en curso |
| `POST /api/scan?secs=N` | Inicia un escaneo BLE no bloqueante (1 a 30 s) |
| `GET /api/devices` | Dispositivos del último escaneo |
| `POST /api/gatt?index=N` | Exploración GATT de solo lectura del dispositivo N. **Bloquea** hasta terminar (hasta ~30 s) |

Garantías: sin endpoints de escritura al vehículo, sin OTA, sin subida de archivos, sin suscripciones a notificaciones. Los datos BLE (nombres, UUID) son **no confiables**: la página los muestra solo con `textContent` y el servidor los sanea al generar JSON.

## Limitaciones conocidas

- Wi-Fi y BLE comparten la antena del ESP32-S3: la fiabilidad del escaneo con Wi-Fi activo **no está medida** (P-052).
- Durante `Explorar GATT` el ESP32 atiende solo esa petición (síncrona); la página puede tardar en responder y el uptime de la TFT se detiene.
- La página no funciona con `https` (el AP sirve `http`); por eso «Copiar» usa un método compatible con `http`.
- Cualquiera con la clave y a alcance de la red puede usar la web. Es solo de lectura, pero evita compartir la clave.
