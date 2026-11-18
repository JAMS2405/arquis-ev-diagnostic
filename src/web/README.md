# web

**Estado: implementado (versión inicial), compila, NO validado en hardware.**

Interfaz web para el teléfono. El ESP32 crea su propia red Wi-Fi (punto de acceso) y sirve una página única; no necesita internet ni router. Detalle de uso y seguridad en `docs/WEB_UI.md`.

- `web_server.*`: AP + servidor HTTP (`WebServer` del core Arduino) y API JSON.
- `web_page.h`: HTML/CSS/JS embebido. Sin recursos externos.

**Solo lectura respecto al vehículo.** Los únicos endpoints: estado, iniciar escaneo BLE, listar dispositivos y exploración GATT de solo lectura. No hay escritura al vehículo, ni OTA, ni subida de archivos.
