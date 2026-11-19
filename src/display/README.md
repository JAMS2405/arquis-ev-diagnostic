# display

**Estado:** inicialización, texto y orientación **validados en hardware** (foto del usuario, H-003). El **panel** nuevo está implementado y compila, pero **su diseño no está validado en hardware**.

TFT ILI9341 (240×320, SPI, sin táctil) con Adafruit GFX / ILI9341. Pines en `../config/pins_config.h`.

## Panel (dashboard)

```
ARQUIS EV DIAG
WIFI                    clientes: 1
ARQUIS-EV-DIAG                       (tamaño 2)
Clave xxxxxxxx                       (tamaño 2; muestra la clave real)
http://192.168.4.1
---------------------------------
BLE: Libre
Ultimo escaneo: 7 dispositivos
UART: Captura A
9600 baud 34 tramas 270 B 12s
BMS: 46.20 V
87 ciclos  76%  dif 20 mV
Abre la direccion en el navegador
---------------------------------
SISTEMA
Activo 00:12:31  Heap 285 KB
PSRAM 7.99 MB  Flash 16.00 MB
No envia ordenes a la scooter
FW 0.1.0-stage1
```

- Los textos los construye `lib/panel_text`, que **recorta por sí misma** a lo que cabe en pantalla (19 caracteres a tamaño 2, 39 a tamaño 1 desde x = 4) y tiene pruebas con valores extremos. Así un texto largo no puede salirse como pasó con el título de la primera versión.
- `updateDashboard()` repinta **solo los campos que cambian**: sin parpadeo y con poco tráfico SPI.
- Los módulos no se acoplan a la pantalla: `main.cpp` lee su estado (`ble_*`, `uart_analyzer`, `jbd_bms::last()`, `web::clients()`) y arma el panel.
- Los resultados de un `scan` por serie se muestran como lista durante 15 s y el panel vuelve solo.
- Colores: verde = actividad en curso o clientes conectados, rojo = protección del BMS activa, amarillo = indicación.

Limitaciones: fuente de 6×8 (solo ASCII, sin tildes ni ñ); la clave se muestra en pantalla.
