# Modo de solo escucha (notificaciones BLE)

> **Estado: implementado y compilando; NO validado en hardware.** Ver `TESTING.md` (S-011, S-012 y pruebas P-070 a P-072 pendientes).
> **Excepción aprobada por el usuario** para su Evobike S13-2, que es suya. Úsalo solo con aparatos propios.

## Para qué sirve

Algunos dispositivos, como el módulo BLE de la Evobike (`lsrt`, con el servicio de puerto serie Nordic UART), transmiten su estado por **notificaciones** sin que nadie se lo pida. El modo de escucha se suscribe a esas notificaciones y **registra lo que llega**, con la hora, en hexadecimal y ASCII, más un resumen (cuántas llegaron, longitudes, intervalo medio, bytes iniciales más frecuentes). Eso permite ver el formato de los datos **sin enviar ni un solo byte** al aparato.

Si el aparato solo habla cuando se le pregunta, no llegará nada: el resumen lo indica. En ese caso el siguiente paso es aprender el protocolo con el registro HCI del teléfono mientras la app oficial funciona.

## Uso

**Web (teléfono):** escanea, pulsa **Escuchar (solo recibe)** en el dispositivo, confirma el aviso y mira el registro en vivo. Puedes **Detener** y **Descargar .txt**. Duración: 20, 30, 60 o 120 s.

**Serie:** `scan`, luego `listen <n> [segundos]`, y `stoplisten` para terminar. Mientras escucha, los demás comandos BLE se rechazan.

Cierra antes la app oficial de la scooter: un periférico atiende a un solo cliente BLE.

## Seguridad: la excepción y sus límites

1. La **única** escritura es la del CCCD que activa las notificaciones, lo que hace cualquier app para recibir datos.
2. `src/ble/ble_listen.cpp` **no puede llamar a `writeValue`**: el vigilante `tools/check_ble_write_policy.py` falla si lo hace. Se comprobó con una violación deliberada.
3. No se escribe en ninguna característica, incluida la de comandos (`6e400002-…`) y el servicio OTA.
4. Se suscribe solo a características con `NOTIFY` (no `INDICATE`) y salta el servicio `0x1801`.
5. Solo actúa tras una acción explícita (botón con aviso, o `listen`).
6. Con la sesión activa se bloquean el escaneo, el GATT y el reporte BMS.

Suscribirse puede hacer que algunos dispositivos empiecen a transmitir; no modifica parámetros.

## Para analizar lo capturado

Pega el `.txt` descargado. Qué buscar: bytes de inicio repetidos (cabecera), longitud fija o variable, campos que cambian con el estado de la scooter (velocidad, batería, marcha) y posibles sumas de verificación. **No se deben enviar tramas adivinadas** al controlador: solo peticiones de lectura ya conocidas.

## Límites
- Registro máximo de 32 KB por sesión (luego se trunca y se avisa).
- Se muestran como máximo 64 bytes por notificación (marca `+N more`).
- Una desconexión del aparato termina la sesión y queda anotada.
