# Captura UART pasiva de la Evobike S13-2

> **Estado: implementado y compilando; NO validado en hardware.** Ver `TESTING.md` (S-013 a S-015 y pruebas P-010 a P-014).
> El ESP32 **solo recibe**: no transmite nada por esta vía.

## Para qué

La pantalla y el controlador conversan por TX/RX. Si el controlador detecta una falla, **el código probablemente viaja en esas tramas** aunque la pantalla solo dibuje un icono (hipótesis, sin verificar). Capturarlas permite ver qué bytes cambian entre «funciona» y «con error».

Contexto del caso en `CASE_EVOBIKE_S13-2.md`.

## Lo que necesitas

- 1 o 2 resistencias de **10 kΩ**, de **20 kΩ** y de **1 kΩ** (una de cada por línea que midas).
- Cables dupont, protoboard, multímetro, soldador si quieres fijar el montaje.
- Una **batería externa USB (power bank)** para alimentar el ESP32 en lugar del PC, y tu teléfono para la web.

## Circuito de entrada (una vez por línea)

```
línea del vehículo ──[10 kΩ]──┬──[1 kΩ]── GPIO15 (canal A)  /  GPIO16 (canal B)
                              │
                           [20 kΩ]
                              │
B− del vehículo ──────────────┴────────── GND del ESP32
```

Cálculo: `V_nodo = V_línea × 20 / (10 + 20)`.

| Nivel alto de la línea | En el nodo | ¿El ESP32 lo lee? |
|---|---|---|
| 5,0 V | 3,33 V | Sí (límite: no superar ~3,6 V) |
| 4,3 V | 2,87 V | Sí (el umbral de «alto» es ≈ 2,5 V) |
| 3,3 V | 2,20 V | **No** de forma fiable |

Con tus mediciones (≈ 4,26 V y ≈ 2,27 V de promedio) lo razonable es que ambas sean líneas de 4–5 V con tráfico, cuyo promedio queda por debajo de su nivel alto. **Es una hipótesis**: la captura lo confirmará o no. Si una línea resultara ser de 3,3 V, este divisor no sirve y hay que cambiarlo.

## Conexiones completas del ESP32

Pines definidos en `src/config/pins_config.h` (los de UART son **provisionales**: confirma en la serigrafía de tu placa que existen los rótulos `15` y `16`).

| Pin del ESP32 | Se conecta a | Notas |
|---|---|---|
| **GND** | **B− (cable negro)** del conector de la pantalla **y** a la pata de abajo de cada 20 kΩ | Es la referencia común. Conéctalo **primero** |
| **GPIO15** (canal A) | Resistencia de **1 kΩ** → nodo del divisor A | Línea **amarilla (TX)** |
| **GPIO16** (canal B) | Resistencia de **1 kΩ** → nodo del divisor B | Línea **verde (RX)** (opcional al principio) |
| 5V | Solo en el Paso 0, para validar el divisor | Retirar después |
| 3V3 | Nada | |
| GPIO8–14 | Pantalla TFT (ya conectada) | No tocar |

**No se conecta nada** a B+ (rojo), DS (blanco) ni ZB (azul). El ESP32 **no tiene ningún pin de salida** hacia la scooter.

```
Conector de la pantalla                        Protoboard                         ESP32-S3
┌───────────┐
│ B−  negro ├────────────────────────────────────┬───────────┬─────────────────── GND
│           │                                    │           │
│ TX amarillo├──[10 kΩ]──┬──[1 kΩ]──────────────────────────────────────────────── GPIO15 (A)
│           │            │                       │           │
│           │         [20 kΩ]──────────────────── ┤           │
│           │                                    │           │
│ RX verde  ├──[10 kΩ]──┬──[1 kΩ]──────────────────────────────────────────────── GPIO16 (B)
│           │            │                                   │
│           │         [20 kΩ]─────────────────────────────────┘
│ B+ rojo   │  ✗ NO CONECTAR
│ DS blanco │  ✗ NO CONECTAR
│ ZB azul   │  ✗ NO CONECTAR
└───────────┘
```

Cada divisor es: línea → 10 kΩ → **nodo** → 20 kΩ → GND, y del nodo sale la 1 kΩ hacia el GPIO.

**Primera vez:** monta y valida **solo el canal A** (amarillo, GPIO15). El canal B puede quedar sin conectar hasta que el A funcione. El software captura **un canal a la vez**.

**Orden de conexión:** con la scooter apagada, 1) GND, 2) la línea al divisor, 3) enciende la scooter. Para desconectar, al revés: apaga, retira la línea, luego GND.

## Paso 0 — Probar el montaje SIN la scooter

1. **Valida el divisor:** conecta la entrada del divisor al pin **5V** del ESP32 y su GND al GND. Mide con el multímetro el punto medio (entre 10 kΩ y 20 kΩ, antes de la 1 kΩ): debe dar **≈ 3,3 V**. Si no, no continúes.
2. **Autoprueba de todo el sistema** con una fuente conocida: une el pin **TX** del ESP32 (GPIO43 en la DevKitC-1; **confírmalo en la serigrafía de tu placa**) directamente a **GPIO15**. Es una salida de 3,3 V hacia una entrada de 3,3 V, sin divisor. El TX es el de la consola, no del analizador.
3. En la web, `UART → Detectar velocidad` con la ventana abierta y mientras tanto escribe `help` varias veces en el monitor serie: debe estimar **115200**. Luego `Capturar` a 115200 y escribe `info`: deben aparecer tramas con texto ASCII.
4. Retira el puente.

Si la autoprueba falla, el problema está en el firmware o en el montaje, no en la scooter.

## Conexión a la scooter

Pines del conector de la pantalla: `B+ (rojo), DS (blanco), B− (negro), TX (amarillo), RX (verde), ZB (azul)`.

1. **Scooter apagada y sin cargador.**
2. **B− (negro) → GND del ESP32** (cable corto).
3. **Una sola línea** (TX amarillo o RX verde) a la entrada del divisor. **No toques B+ (rojo), DS ni ZB.** B+ está pegado a DS: usa puntas o pinzas que no puedan deslizarse, y aísla las uniones.
4. ESP32 alimentado desde el **power bank**, no desde el PC (evita un bucle de masa).
5. Enciende la scooter. Conéctate al Wi‑Fi del ESP32 y abre `http://192.168.4.1`.
6. `UART → Detectar velocidad` (2 s). Si sale una velocidad con confianza, usa `Capturar`.

## Cómo leer los resultados

**Detección.** Lista las velocidades candidatas con su puntuación. «Confident» indica una candidata clara. Si dice que no hay actividad, revisa GND, el divisor y el cable.

**Captura.** Cada línea es una trama: tiempo desde la primera, canal, longitud, bytes en hexadecimal y ASCII. Al final:
- la **trama dominante** (longitud y primer byte) y su período,
- la **plantilla** con `??` en las posiciones que cambian,
- las **posiciones variables** con su rango,
- el **checksum**, si coincide con un esquema común en todas las tramas.

## Cómo buscar el código de falla

1. Captura la línea **con el error parpadeando** y la scooter quieta (30 s).
2. Repite en otros estados: **acelerador pulsado**, **botón de marcha mantenido** (el modo de empuje que ya mueve el motor), freno accionado.
3. Compara las posiciones variables entre capturas. Un byte que **solo es distinto mientras dura el error**, o que vale 0 en el modo de empuje y no cero en el estado de error, es candidato a código de falla.
4. Haz lo mismo con la **otra línea**: una lleva pantalla→controlador y la otra controlador→pantalla; el estado de falla suele venir de esta última.

Pega aquí los `.txt` descargados y los analizo.

## Si algo no cuadra

| Síntoma | Causa probable |
|---|---|
| 0 flancos | GND no común, divisor mal, cable equivocado o scooter apagada |
| Hay flancos pero no velocidad | No es UART estándar, nivel incorrecto o velocidad no estándar |
| Velocidad bien pero las tramas no se repiten | Formato distinto (prueba 8E1 u 8O1) o datos muy irregulares |
| «Idle level: LOW» en la detección | Lógica invertida: **no está soportada**; avísame |

## Límites

- Un canal a la vez (A o B). No correlaciona ambas líneas.
- Las marcas de tiempo de los bytes tienen la resolución del sondeo (≈ 1 ms), suficiente para separar tramas, no para medir bits.
- La detección de velocidad usa interrupciones y es menos exacta a 115200 o más; si falla, prueba la captura con las velocidades candidatas.
- Hasta 600 s por captura; el registro guarda 96 KB de texto y 1500 tramas.

## Seguridad (resumen)

- Solo entrada: `tools/check_ble_write_policy.py` falla si `src/uart/` transmite, configura un pin como salida, activa un pull-up o abre un UART sin `txPin = -1`.
- **Nunca** conectes B+, DS ni ZB, ni una línea sin divisor. Un GPIO del ESP32 tolera ~3,3 V.
- **Nunca** con el cargador conectado.
- Revisa `ELECTRICAL_SAFETY.md`.
