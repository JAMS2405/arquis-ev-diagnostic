# Notas BLE

Observaciones de pruebas reales con el BLE Analyzer. Lo que viene de la salida del dispositivo es dato; lo que viene de documentación pública conocida se marca como tal y **no ha sido verificado aquí**.

## H-004 — BMS JBD cercano (no es la scooter)

Dispositivo anunciado como un modelo `SP14S…` de la familia JBD. Dirección MAC omitida a propósito.

| Servicio | Característica | Propiedades | Valor leído |
|---|---|---|---|
| `0x1800` Generic Access | `0x2A00` Device Name | READ NOTIFY | texto: nombre del modelo |
| | `0x2A01` Appearance | READ | `00 00` |
| | `0x2A04` Conn. Parameters | READ | `14 00 28 00 00 00 E8 03` |
| `0x1801` Generic Attribute | `0x2A05` Service Changed | INDICATE | — |
| `0x180A` Device Information | `0x2A50` PnP ID | READ | `02 8A 24 66 82 01 00` |
| `0xFF00` (propietario) | `0xFF01` | READ NOTIFY | 20 bytes a cero |
| | `0xFF02` | READ **WRITE_NR** | `DD A5 04 00 FF FC 77 00…` |
| `00010203-0405-0607-0809-0a0b0c0d1912` | `…2b12` | READ **WRITE_NR** | `00` |

### Qué dice cada dato
- **PnP ID** `02 8A 24 66 82 01 00`: origen USB-IF, VID `0x248A`, PID `0x8266`, versión 1. Según listados públicos, `0x248A` corresponde a Telink Semiconductor, es decir, el módulo BLE sería un chip Telink. Sin verificar aquí.
- **`0xFF00` / `0xFF01` / `0xFF02`**: es la disposición habitual de los BMS JBD (documentación pública): `FF01` recibe las respuestas por notificación y `FF02` recibe los comandos.
- **Valor de `0xFF02`** `DD A5 04 00 FF FC 77`: tiene la forma de una **petición** del protocolo JBD: inicio `DD`, `A5` = lectura, comando `04` (tensiones de celdas), longitud `00`, suma de verificación `FF FC` (= 0x10000 − 0x04) y fin `77`. Es lo último que escribió un cliente en esa característica; no es una respuesta del BMS. `FF01` está a cero porque nadie estaba suscrito.
- **Servicio `00010203-0405-0607-0809-0a0b0c0d1912`**: según documentación pública es el servicio de **actualización de firmware (OTA) de Telink**. **Escribir ahí puede dejar el módulo inservible.**

## H-005 — dispositivo anunciado como `lsrt`

Resultado descargado desde la web. Dirección MAC omitida a propósito. **El usuario confirmó que es su Evobike S13-2** y que el resultado se obtuvo desde la web en el teléfono. Hipótesis (sin verificar): corresponde al módulo BLE de la pantalla (`Orient Sunteh FLT_T3`, ver `HARDWARE.md`).

| Servicio | Característica | Propiedades | Valor leído |
|---|---|---|---|
| `0x1800` Generic Access | `0x2A00` Device Name | READ | texto `LS Dis Server` |
| | `0x2A01` Appearance | READ | `00 00` |
| `0x1801` Generic Attribute | `0x2A05` Service Changed | INDICATE | — |
| | `0x2B29` | READ WRITE | `00` |
| | `0x2B2A` | READ | 16 bytes (hash) |
| `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | `6e400002-…` | WRITE, WRITE_NR | — |
| | `6e400003-…` | NOTIFY | — |

### Qué dice
- **Servicio `6e400001-…`**: es el UUID del **Nordic UART Service (NUS)** (documentación pública), un puerto serie transparente sobre BLE. `…0002` recibe datos del teléfono y `…0003` los envía por notificaciones. **Los datos del aparato viajan por ese canal**, y la herramienta no puede leerlos: ninguna de las dos tiene la propiedad READ.
- **`0x2B29` y `0x2B2A`**: *Client Supported Features* y *Database Hash* (caché GATT del estándar Bluetooth 5.1 en adelante). Indican una pila BLE moderna. El hash identifica la estructura del perfil.
- **Nombre anunciado `lsrt` frente a nombre GAP `LS Dis Server`**: parece un nombre de ejemplo del SDK del fabricante sin personalizar. No permite identificar el fabricante.
- La dirección MAC tenía un patrón secuencial; en algunos módulos es un valor por defecto de fábrica, así que **no sirve como identificador fiable**.
- El NUS también lo usan otros fabricantes, incluidas algunas scooters. Por sí solo **no identifica la marca**.

### Consecuencia
Para ver qué envía este dispositivo hay que **suscribirse** a `…0003` (escribe el CCCD); para pedirle datos habría que escribir en `…0002`. Lo primero se resolvió con el **modo de escucha** (`LISTEN_MODE.md`), aprobado por el usuario para su Evobike. Lo segundo no se hace. Enviar bytes inventados a un controlador de vehículo puede cambiar su configuración, así que **no se envían tramas adivinadas**: el protocolo se aprende de capturas reales (registro HCI del teléfono con la app oficial, o línea UART) y solo se enviarán peticiones de lectura ya conocidas.

## Reglas derivadas
1. El **analizador BLE** (`scan`, `gatt`) **nunca escribe** en ninguna característica.
2. Para obtener datos en vivo de un BMS JBD hay que enviar una petición a `FF02` y suscribirse a `FF01`; ambas son escrituras GATT. Por decisión explícita del usuario (batería propia) se añadió el módulo `src/bms` (Etapa 3c) como **única excepción**, limitada por construcción a 3 peticiones de lectura. Ver `BMS_JBD.md`.
3. Conectarse (aunque sea solo lectura) únicamente a dispositivos propios o con permiso.
4. Nunca se escribe en el servicio OTA de Telink, ni siquiera desde el módulo BMS.
   El modo de escucha (`ble_listen`) solo puede suscribirse; no puede llamar a `writeValue`.
5. La salida de `gatt` etiqueta los UUID conocidos y marca el servicio OTA de Telink con `NEVER WRITE`.
