# Seguridad eléctrica

> **Regla absoluta:** ninguna señal de batería, bus de potencia ni fase de motor se conecta directamente al ESP32.

Este proyecto toca la electrónica de un vehículo con batería de potencia. Una conexión incorrecta puede destruir el ESP32, el controlador del vehículo, la batería/BMS, o causar lesiones e incendio. La tensión nominal de la batería de la Evobike S13-2 **no está confirmada**: debe medirse con multímetro antes de cualquier trabajo y se asume peligrosa.

## Límites del ESP32-S3

- Todas las GPIO: **3,3 V lógicos**. Tensión máxima admisible en pin: ≈3,6 V. **No son tolerantes a 5 V.**
- ADC: rango útil limitado (≈0–3,1 V según atenuación); el ADC del ESP32 es no lineal y requiere calibración.
- Alimentación del proyecto: **solo por USB 5 V**. No alimentar desde el vehículo en v1.

## Clasificación de señales

### Verde — pueden conectarse tras medir y verificar nivel

| Señal | Condición |
|---|---|
| UART TX/RX entre componentes, **si el nivel medido es ≤ 3,3 V** | Conectar solo la línea de lectura (RX del analizador) + masa común. Verificar con multímetro con el vehículo en reposo y en funcionamiento. |
| Salidas lógicas de 3,3 V del propio vehículo | Medir primero; nivel estable y confirmado. |

### Amarillo — requieren protección o adaptación antes de conectar

| Señal | Protección mínima |
|---|---|
| UART con nivel de 5 V | Divisor resistivo o adaptador de nivel; resistencia serie; opcional diodo Zener/TVS de 3,3 V. |
| Nivel lógico desconocido o variable | No conectar hasta medirlo. Tratar como 5 V o más. |
| Señales analógicas de baja tensión (acelerador Hall, freno, sensores; típ. 0–5 V) | Divisor resistivo a rango ≤ 3,3 V, filtro RC, resistencia serie y limitación con diodo/TVS. Verificar tensión máxima real, incluyendo picos. |
| Líneas de alimentación auxiliar (5 V, 12 V de accesorios) | Solo mediante divisor dimensionado y protegido, para **medición**; nunca como alimentación del ESP32. |

### Rojo — NO conectar al ESP32 (requieren aislamiento galvánico y diseño específico, fuera del alcance de v1)

| Señal | Motivo |
|---|---|
| Positivo de batería / bus de potencia | Decenas de voltios y alta corriente; destrucción y riesgo de incendio. |
| Fases del motor (U/V/W) | Conmutación PWM con picos de tensión elevados. |
| Líneas de carga / cargador | Tensión elevada y posible conexión a red. |
| Cualquier señal con tensión no medida | Desconocida = peligrosa. |
| Líneas de comunicación del BMS (hasta caracterizarlas) | Pueden estar referidas a un potencial distinto de la masa del controlador. |

Para medir tensión de batería en v1 se usa **solo el multímetro**. Un futuro monitor de alta tensión requeriría divisor de alta impedancia calificado, fusible, aislamiento y revisión previa de diseño documentada.

## Conector de la pantalla de la Evobike S13-2

Pines: B+, DS, B-, TX, RX, ZB (ver `HARDWARE.md`). Clasificación:

| Pin | Zona | Regla |
|---|---|---|
| B+ | Roja | Tensión de batería. **Nunca** al ESP32. Cuidado al medir: una punta que toque pines vecinos puede provocar un cortocircuito. |
| B- | Referencia | Es la masa para medir y para el GND común del analizador. |
| TX, RX | Amarilla | Solo a través de divisor/adaptador de nivel validado (ver más abajo), de una línea a la vez. |
| DS, ZB | Roja hasta medir | Función desconocida. No conectar. |

Con la pantalla conectada a la batería, **no conectar a la vez un portátil enchufado a red** al ESP32 mientras este comparte GND (B-) con la scooter. Usar un portátil funcionando a batería o un aislador USB.

## Prácticas obligatorias

1. **Vehículo apagado y batería desconectada** al cablear. Rueda motriz levantada del suelo en cualquier prueba con el vehículo encendido.
2. **Medir antes de conectar**: nivel de cada señal respecto a su masa, con el multímetro.
3. **Masa común**: el GND del analizador debe unirse al GND de la señal observada. Verificar que ese GND es realmente la referencia de la línea (no un punto flotante).
4. **Bucles de masa**: al conectar a un vehículo, alimentar el ESP32 desde un portátil funcionando **a batería** o mediante un **aislador USB**. Evitar conectar un portátil enchufado a red al mismo tiempo que se está conectado a un vehículo en carga.
5. **Nunca** realizar pruebas con el cargador conectado.
6. **Conexión solo de lectura** en UART: no cablear el TX del ESP32 a ninguna línea del vehículo en v1.
7. Mantener cables cortos, sin conductores expuestos, y aislar con termorretráctil o cinta las uniones soldadas.
8. Usar un fusible/limitador en cualquier derivación de prueba hacia el vehículo cuando se pueda.
9. No trabajar solo con baterías de litio de potencia; tener un extintor apropiado cerca.
10. Si algo se calienta, huele o chispea: desconectar la batería de inmediato.

## Estado de validación

Ninguna de las protecciones descritas ha sido diseñada, construida ni validada aún. Cuando se construya cada circuito de entrada, se documentará su esquema, valores calculados y la medición que lo valida en [`TESTING.md`](TESTING.md).

## Descargo de responsabilidad

Esta documentación es una guía de buenas prácticas, no una certificación. El uso del proyecto es bajo responsabilidad del usuario.
