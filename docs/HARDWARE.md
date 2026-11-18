# Hardware

Este documento separa lo **declarado por el propietario del proyecto** de lo **verificado físicamente**. Ningún elemento figura como validado hasta que exista una prueba física registrada en [`TESTING.md`](TESTING.md).

## Hardware declarado

| Elemento | Especificación | Validado físicamente |
|---|---|:-:|
| Microcontrolador | ESP32-S3-N16R8 | No |
| Flash | 16 MB | No |
| PSRAM | 8 MB (en la variante N16R8 es PSRAM octal) | No |
| Placa | ESP32-S3 DevKit con USB | No |
| Pantalla | TFT 2.4", controlador ILI9341 | No |
| Resolución | 240×320 | No |
| Interfaz de pantalla | SPI | No |
| Táctil | No dispone | — |
| Alimentación | USB 5 V | No |

## Herramientas disponibles

- Multímetro
- Protoboard
- Soldador

No se dispone (según lo declarado) de osciloscopio ni analizador lógico; esto condiciona los métodos de verificación (ver [`TESTING.md`](TESTING.md)).

## Pendiente de verificar

- **Modelo exacto de la placa DevKit**: la serigrafía y el esquema determinan qué GPIO están realmente disponibles. Hay clones de N16R8 con diferencias (por ejemplo, LED RGB en un pin concreto, o conectores USB-UART y USB nativo).
- **Tipo de PSRAM**: N16R8 implica PSRAM octal, que ocupa GPIO33–37 y los hace inutilizables.
- **Módulo TFT**: orden de pines del conector, si tiene regulador de 3,3 V a bordo, nivel lógico aceptado, y si el pin de backlight se puede controlar o está cableado a alimentación.
- **Tarjeta SD en el módulo TFT**: muchos módulos de 2,4" incluyen ranura SD; si existe, comparte el bus SPI y requeriría su propio CS.

## Hardware del vehículo (objeto de estudio)

### Conector de la pantalla de la scooter (informado por el usuario)

Pines rotulados, en este orden: **B+, DS, B-, TX, RX, ZB**.

| Pin | Significado | Estado |
|---|---|---|
| B+ | Positivo de batería (potencia) | Inferido por la rotulación; tensión sin medir. **Zona roja.** |
| B- | Negativo de batería / referencia de masa | Inferido por la rotulación. Referencia para medir TX/RX. |
| TX | Línea de datos | Medición dudosa (ver abajo). Qué lado transmite: sin confirmar. |
| RX | Línea de datos | Medición dudosa (ver abajo). Qué lado transmite: sin confirmar. |
| DS | Desconocido | Función sin confirmar. Tratar como zona roja hasta medir. |
| ZB | Desconocido | Función sin confirmar. Tratar como zona roja hasta medir. |

Mediciones informadas por el usuario (dudosas: no se sabe qué valor es de TX y cuál de RX, ni el estado de la scooter ni la referencia de masa): una línea ≈ 4,26 V y la otra ≈ 2,27 V; al acelerar solo varían ≈ 0,01 V. **No concluyentes**: un multímetro en DC no distingue si hay datos. Pendiente repetir la medición con la tabla de `ELECTRICAL_SAFETY.md`.

---

**Evobike S13-2**: no hay documentación de protocolos, tensiones ni conectores confirmada. Todo dato del vehículo (tensión nominal de batería, niveles lógicos de UART, velocidad en baudios, servicios BLE) debe **medirse y registrarse** durante la investigación, no suponerse.
