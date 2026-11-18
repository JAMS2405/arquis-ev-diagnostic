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
- [ ] Verificación de PSRAM y flash reportada por serie. *(hardware)* — el código la imprime; falta ejecutarlo en la placa.
- [ ] Pantalla ILI9341 muestra texto y colores correctamente, orientación y resolución 240×320 confirmadas. *(hardware)*
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

**Alcance:** escaneo de dispositivos BLE (nombre, dirección, RSSI, datos de anuncio); conexión opcional y **solo lectura** de servicios/características GATT; sin escrituras, sin suscripciones que alteren el estado del vehículo, sin modificar parámetros.

**Criterios de aceptación**
- Ninguna escritura GATT en el código (revisión de código).
- Descubre un dispositivo BLE de prueba conocido y lista sus servicios/características. *(hardware)*
- Lista de dispositivos y detalle de GATT visibles en TFT.
- Si la Evobike S13-2 expone BLE, se documentan UUID y propiedades observadas. *(hardware)*

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
