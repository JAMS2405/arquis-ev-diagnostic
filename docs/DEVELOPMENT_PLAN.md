# Plan de desarrollo

Desarrollo por etapas con **autorización explícita** antes de iniciar cada una. Una etapa se considera cerrada solo cuando todos sus criterios de aceptación están cumplidos y registrados en [`TESTING.md`](TESTING.md). Los criterios marcados *(hardware)* requieren prueba física.

Reglas transversales:

- Commits pequeños con mensajes convencionales (`feat:`, `fix:`, `docs:`, `chore:`, `refactor:`, `test:`).
- Sin código ajeno reutilizado sin autorización.
- La v1 **no transmite** al vehículo (ni UART ni BLE).
- Sin CAN, RS485, BMS avanzado ni diagnóstico multimarca.

---

## Etapa 0 — Estructura y documentación

**Alcance:** estructura del repositorio, documentación base, configuración de PlatformIO, esqueleto de arranque.

**Criterios de aceptación**
- [x] Estructura de directorios creada.
- [x] README, HARDWARE, ELECTRICAL_SAFETY, PINOUT, DEVELOPMENT_PLAN y TESTING redactados.
- [x] Pines centralizados en `src/config/pins_config.h`, marcados como provisionales.
- [x] `.gitignore` excluye `.pio`, secretos y temporales.
- [x] Compilación del esqueleto verificada en contenedor Linux (S-001 en `TESTING.md`). No valida hardware.
- [ ] Repositorio publicado en GitHub (pendiente de acceso).

## Etapa 1 — Base del firmware y pantalla

**Estado:** implementada y compilando en la rama `feat/stage-1-display`. **No cerrada**: faltan las verificaciones de hardware. No fusionar a `main` hasta cumplirlas.

**Alcance:** arranque estable, configuración central, controlador de pantalla TFT, pantalla de estado y navegación básica (sin táctil; entrada por definir, por ejemplo botón(es) físicos).

**Criterios de aceptación**
- [x] Compila sin errores ni advertencias relevantes (S-002).
- [x] Verificación de PSRAM y flash reportada por serie. *(hardware)* — 16 777 216 B de flash y 8 386 279 B de PSRAM en la placa real (H-002).
- [ ] Pantalla ILI9341 muestra texto y colores correctamente, orientación y resolución 240×320 confirmadas. *(hardware)* — parcial: el usuario informa imagen (H-003); falta confirmar detalles.
- [ ] Pines finales verificados contra la placa física y `PINOUT.md` actualizado. *(hardware)*
- [x] Estructura de módulos con interfaces definidas, sin lógica de otros módulos (`display::` expone `begin`, `showBoot`, `showStatusLayout`, `updateStatus`).
- [ ] **Decisión pendiente:** entrada para navegación (botón/es físicos, encoder). No hay táctil y el hardware de entrada no está definido, por lo que **no se implementó navegación**; la pantalla de estado es de solo lectura.

## Etapa 2 — UART Analyzer (prioridad máxima)

**Alcance:** captura **pasiva** (solo RX) de las líneas TX/RX de la scooter; registro de bytes con marca de tiempo; estimación de velocidad en baudios; visualización hexadecimal/ASCII; detección de tramas por separación temporal; sin transmisión.

**Criterios de aceptación**
- Ninguna ruta de código transmite por los puertos del analizador (revisión de código + ausencia de pin TX asignado).
- Se captura tráfico generado por una fuente conocida de prueba (p. ej. otro microcontrolador a 3,3 V) sin pérdida de bytes a las velocidades objetivo. *(hardware)*
- Estimación de baudios correcta frente a la fuente de prueba. *(hardware)*
- Los niveles lógicos de la Evobike S13-2 se miden con multímetro y se documentan **antes** de conectar. *(hardware)*
- Captura real de la Evobike S13-2 almacenada y descrita (sin publicar datos privados). *(hardware)*
- Visualización en TFT legible y estable durante captura continua.

## Etapa 3 — BLE Analyzer

**Estado:** versión inicial implementada y compilando en la rama `feat/stage-3-ble`. Se adelantó a la Etapa 2 por decisión del usuario, porque no requiere conexión eléctrica a la scooter mientras se miden los niveles del UART. Pendiente de validación en hardware; no fusionada a `main`.

**Alcance:** escaneo de dispositivos BLE (nombre, dirección, RSSI, datos de anuncio); conexión opcional y **solo lectura** de servicios/características GATT; sin escrituras, sin suscripciones que alteren el estado del vehículo, sin modificar parámetros.

**Criterios de aceptación**
- [x] Ninguna escritura GATT en el código (revisión de código y `grep` sin coincidencias de `writeValue`/`subscribe`; ver S-004).
- Descubre un dispositivo BLE de prueba conocido y lista sus servicios/características. *(hardware)*
- Lista de dispositivos y detalle de GATT visibles en TFT.
- Si la Evobike S13-2 expone BLE, se documentan UUID y propiedades observadas. *(hardware)*

## Etapa 3b — Interfaz web para teléfono

**Estado:** versión inicial implementada y compilando en la rama `feat/web-ui` (sobre `feat/stage-3-ble`). Pendiente de validación en hardware; no fusionada a `main`. Detalle en `WEB_UI.md`.

**Alcance:** punto de acceso Wi-Fi propio y página única, solo lectura, para controlar el escaneo BLE y la exploración GATT desde el teléfono. Reutilizará el mismo patrón para mostrar capturas UART (Etapa 2) y mediciones (Etapa 4).

**Criterios de aceptación**
- [x] Compila (S-006) y la página se comporta correctamente contra un servidor simulado (S-007).
- [x] Sin endpoints de escritura al vehículo, sin OTA, sin clave fija en el repositorio.
- [ ] El teléfono se conecta al AP y carga la página. *(hardware, P-050/P-051)*
- [ ] Escaneo y GATT funcionan desde el teléfono. *(hardware, P-053)*
- [ ] Efecto del Wi-Fi sobre la fiabilidad del escaneo medido y documentado. *(hardware, P-052)*

## Etapa 3c — Reporte de BMS JBD (alcance ampliado por el usuario)

**Estado:** implementado y compilando en la rama `feat/bms-jbd-report`. Pendiente de validación con un BMS real; no fusionada a `main`. Detalle en `BMS_JBD.md`.

**Alcance:** lectura de un BMS JBD propio por BLE mediante 3 peticiones fijas de lectura; reporte con ciclos, modelo, celdas, temperaturas, protecciones y capacidad; descarga `.md`/`.json`. **Fuera de alcance:** parámetros de configuración, modo de fábrica, control de MOSFET, escritura de cualquier tipo, SOH calculado.

**Criterios de aceptación**
- [x] Biblioteca de protocolo pura, que solo puede construir peticiones de lectura, con pruebas en el PC (S-009).
- [x] Única escritura BLE del proyecto aislada en `src/bms` y vigilada por `tools/check_ble_write_policy.py` (S-008).
- [x] Interfaz web y reporte verificados contra un servidor simulado (S-010).
- [ ] Reporte de un BMS JBD real sin errores. *(hardware, P-060)*
- [ ] Valores coinciden con la app oficial del BMS. *(hardware, P-061)*
- [ ] El BMS conserva su estado (MOSFET, protecciones) y la app oficial sigue conectando. *(hardware, P-062)*
- [ ] Signo de la corriente verificado con carga real. *(hardware, P-064)*

## Etapa 3d — Soporte incremental de otras marcas de BMS

**Estado:** planificada; sin código. Procedimiento y criterios en `BMS_ONBOARDING.md`.

**Alcance:** una marca a la vez, a partir de datos reales de una unidad y con la app oficial y el multímetro como referencia. Misma regla que 3c: solo lecturas, baterías propias o con permiso. Las marcas sin protocolo viable por BLE (cifrado, autenticación) pueden quedar fuera o resolverse por cable en una etapa posterior.

**Criterios de aceptación (por marca)**
- Fichas de datos recolectadas (`BMS_ONBOARDING.md`).
- Biblioteca de protocolo pura con pruebas en el PC hechas con tramas reales.
- Lista cerrada de peticiones de lectura y política de escritura actualizada. *(hardware)*
- Valores comparados con la app oficial y el multímetro en una unidad real. *(hardware)*

## Etapa 4 — Electrical Monitor

**Alcance:** lectura de señales de baja tensión mediante entradas **protegidas** (ADC1), con calibración y filtrado.

**Criterios de aceptación**
- Diseño y esquema de acondicionamiento de entrada documentados y revisados contra `ELECTRICAL_SAFETY.md`.
- Calibración con fuente/multímetro: error medido y documentado. *(hardware)*
- Verificación de que las entradas toleran el máximo esperado de la señal con la protección implementada. *(hardware)*
- Ninguna entrada acepta tensión de batería directa.

## Etapa 5 — Almacenamiento e integración

**Alcance:** persistencia de capturas y registros, exportación, integración de los tres módulos en una interfaz unificada, módulo `diagnostics` básico (resumen de hallazgos).

**Criterios de aceptación**
- Capturas guardadas y recuperables sin corrupción tras reinicio. *(hardware)*
- Exportación de capturas a un formato abierto y documentado.
- Los tres módulos coexisten sin degradación perceptible (UART, BLE y ADC concurrentes). *(hardware)*
- Documentación y `TESTING.md` actualizados con resultados reales.

---

## Fuera de alcance (v1)

CAN, RS485, BMS avanzado, diagnóstico multimarca, transmisión de comandos al vehículo, escritura de parámetros BLE.
