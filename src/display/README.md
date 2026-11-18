# display

**Estado: implementado en Etapa 1, compila, NO validado en hardware.**

Interfaz TFT ILI9341 (240×320, SPI, sin táctil) con Adafruit GFX / Adafruit ILI9341. Los pines se leen de `../config/pins_config.h` (provisionales).

API (`display.h`): `begin()`, `showBoot()`, `showStatusLayout()`, `updateStatus()`.

Limitaciones conocidas:
- El ILI9341 por SPI es de solo escritura aquí: `begin()` no puede confirmar que la pantalla respondió.
- El control del backlight (`TFT_BL`) depende del módulo.
- Sin navegación: el hardware de entrada no está definido.
