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

## Pendiente
- [ ] Aclarar qué hace exactamente la palanca de cambios y qué hace el motor.
- [ ] ¿La pantalla muestra algún patrón (parpadeo, cuántas veces)? ¿El controlador tiene LED o pitidos?
- [ ] Medir B+ contra B-.
- [ ] Medir señal del acelerador, del freno y los Hall del motor (rueda en el aire).
- [ ] Etapa 2: captura pasiva de la línea UART con entrada protegida (`ELECTRICAL_SAFETY.md`).
