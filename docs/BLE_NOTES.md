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

## Reglas derivadas
1. El **analizador BLE** (`scan`, `gatt`) **nunca escribe** en ninguna característica.
2. Para obtener datos en vivo de un BMS JBD hay que enviar una petición a `FF02` y suscribirse a `FF01`; ambas son escrituras GATT. Por decisión explícita del usuario (batería propia) se añadió el módulo `src/bms` (Etapa 3c) como **única excepción**, limitada por construcción a 3 peticiones de lectura. Ver `BMS_JBD.md`.
3. Conectarse (aunque sea solo lectura) únicamente a dispositivos propios o con permiso.
4. Nunca se escribe en el servicio OTA de Telink, ni siquiera desde el módulo BMS.
5. La salida de `gatt` etiqueta los UUID conocidos y marca el servicio OTA de Telink con `NEVER WRITE`.
