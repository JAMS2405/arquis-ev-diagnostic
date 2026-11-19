# Caso: Evobike S13-2 que no anda

Notas del caso real que motiva el proyecto. Todo lo de «Informado» lo dijo el usuario y **no está verificado por la herramienta**. Las hipótesis son causas habituales en scooters eléctricas, **sin evidencia propia en este caso**.

## Síntomas (informados)
- La scooter **no avanza**: al acelerar, no acelera.
- La **pantalla muestra un error** (icono de advertencia), **sin ningún código** visible.
- Con la palanca de cambios mantenida, el **motor gira pero «no hace nada»** (detalle pendiente de aclarar).
- No existe **aplicación oficial** para la scooter.

## Observaciones de la herramienta
| Qué | Resultado |
|---|---|
| BLE de la pantalla (`lsrt`, Nordic UART) | Se puede explorar; **no transmite nada por sí solo** en 30 s (H-006), incluso con la palanca pulsada durante la escucha (informado) |
| TX / RX de la pantalla (medidas con multímetro) | ≈ 4,26 V y ≈ 2,27 V; **no cambian** al acelerar, frenar o cambiar luces, salvo ≈ 0,01 V (informado) |

## Qué se concluye (y qué no)
- Con el BLE no hay vía práctica para el diagnóstico: sin app oficial no hay protocolo que observar, y el módulo no emite telemetría solo. **No es un camino prometedor para encontrar la falla.**
- Que el multímetro (DC) no vea cambios **no prueba que las líneas estén inactivas**: promedia pulsos rápidos. Un cambio de ≈ 0,01 V es compatible con tráfico UART cuyo contenido varía, pero **está sin demostrar**. Hace falta una captura con el analizador (Etapa 2) para saberlo.
- Si la pantalla y el controlador conversan por TX/RX, el **código de falla** probablemente viaja en esas tramas aunque la pantalla no lo muestre. Es la vía de la herramienta con más posibilidades de dar el código.

## Hipótesis habituales (sin evidencia aquí)
Se comprueban **con el multímetro**, sin la herramienta:
1. **Acelerador** (sensor Hall): señal que no varía al girarlo.
2. **Sensor de freno** (corte de motor) atascado o con la señal activa.
3. **Sensores Hall del motor** (5 hilos): un Hall que no conmuta al girar la rueda a mano. Es un cuadro típico de «el motor gira pero no tiene fuerza».
4. **Controlador** averiado.
5. **Protección del BMS** (por tensión, temperatura o corriente).

## Actualización 2026-11-19: mediciones del usuario

Todo «informado» por el usuario, no verificado por la herramienta.

| Elemento | Resultado | Lectura |
|---|---|---|
| Acelerador | 5 V de alimentación, GND, señal ≈ 0,8 V en reposo que sube a ≈ 4 V; además cambió el sensor Hall del acelerador | En su conector la señal es correcta |
| Hall del motor | Todos conmutan al girar la rueda a mano | Correctos |
| Frenos | Ambos bien; ninguno activo; la luz de freno se enciende al accionarlos | Correctos |
| Batería (B+ contra B−) | Tensión normal | Sin indicio de falla de batería |
| Botón de marcha | Cambia 1, 2, 3 con cada pulsación | Funciona |
| **Botón de marcha mantenido** | **El motor anda; con una persona encima empuja a ≈ 5 km/h** | Ver hipótesis |
| Icono de error | Parpadea ≈ 1 vez por segundo; sin código, sin pitidos | — |
| Antecedentes | Falló sin causa conocida | — |
| TX / RX | Sin cambios al acelerar, frenar o cambiar luces (salvo ≈ 0,01 V) | Sin concluir |

### Hipótesis (sin verificar)
- Que el motor se mueva con el botón mantenido sugiere que el **motor, sus Hall y la etapa de potencia del controlador funcionan**, y que el controlador **sí recibe órdenes** de la pantalla. Es una interpretación mía: «modo de empuje» es un nombre habitual de esa función, no confirmado en esta scooter.
- Entonces el controlador **no acepta la orden del acelerador** y la pantalla muestra un error. Qué falla exactamente no se sabe.
- Con las medidas del usuario quedan **descartadas** (salvo error de medición): sensor del acelerador, Hall del motor, frenos y batería.
- Posibles causas que **no** se han comprobado: el acelerador en el **lado del controlador** (cable o conector entre el acelerador y el controlador), un estado de protección del controlador (p. ej. limitación de velocidad por una falla), configuración del controlador, o el controlador mismo.
- El **código de falla** probablemente viaja por la línea UART controlador→pantalla. Es el siguiente paso: `UART_TAP.md`.

## Pendiente
- [x] Qué hace la palanca/botón de marcha y el motor (ver tabla).
- [x] Patrón del icono: ≈ 1 parpadeo por segundo, sin pitidos.
- [x] B+ contra B−: normal.
- [x] Acelerador, frenos y Hall del motor: correctos.
- [ ] ¿El controlador tiene LED de estado? ¿Se puede medir la señal del acelerador **en el conector del controlador**?
- [ ] Etapa 2: captura pasiva de la línea UART con entrada protegida (`UART_TAP.md`): autoprueba sin scooter, luego estado de error, acelerador, modo de empuje y freno.
