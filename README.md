# ARQUIS EV DIAGNOSTIC

Herramienta portátil de diagnóstico electrónico para scooters, bicicletas y motocicletas eléctricas, inicialmente desarrollada para investigar una **Evobike S13-2** mediante ESP32-S3.

> **Estado del proyecto: Etapa 0 — estructura y documentación.**
> No hay ninguna funcionalidad implementada todavía y ningún hardware ha sido validado con pruebas físicas. Ver [`docs/TESTING.md`](docs/TESTING.md).

---

## Tabla de contenido

1. [Objetivo](#objetivo)
2. [Alcance de la versión 1](#alcance-de-la-versión-1)
3. [Hardware](#hardware)
4. [Arquitectura](#arquitectura)
5. [Requisitos](#requisitos)
6. [Instalación](#instalación)
7. [Compilación](#compilación)
8. [Etapas de desarrollo](#etapas-de-desarrollo)
9. [Seguridad eléctrica](#seguridad-eléctrica)
10. [Documentación](#documentación)
11. [Licencia](#licencia)

---

## Objetivo

Construir un instrumento portátil, basado en ESP32-S3 con pantalla TFT, que permita **investigar de forma no intrusiva** la electrónica de un vehículo eléctrico ligero. La primera versión está dedicada exclusivamente a una Evobike S13-2, de la que no se dispone de documentación de protocolo; por ello la herramienta se centra en **observar**, no en actuar.

Principios de diseño:

- **Solo observación.** La v1 no transmite comandos al vehículo, ni por UART ni por BLE.
- **Seguridad primero.** Ninguna señal de batería se conecta directamente al ESP32. Ver [`docs/ELECTRICAL_SAFETY.md`](docs/ELECTRICAL_SAFETY.md).
- **Desarrollo controlado.** Cada etapa tiene criterios de aceptación explícitos y no se declara validado nada que no se haya probado físicamente.
- **Sin código ajeno.** No se reutiliza firmware de otros proyectos sin autorización.

## Alcance de la versión 1

Tres módulos, en este orden de prioridad:

| Prioridad | Módulo | Descripción | Restricción clave |
|:-:|---|---|---|
| 1 | **UART Analyzer** | Captura y análisis de las líneas TX/RX entre controlador, display y demás componentes de la scooter. | Modo pasivo: **sin transmitir**. |
| 2 | **BLE Analyzer** | Descubrimiento de dispositivos BLE y exploración de servicios/características GATT. | **Sin escrituras** ni modificación de parámetros del vehículo. |
| 3 | **Electrical Monitor** | Medición de señales de baja tensión mediante entradas protegidas. | Solo señales de baja tensión y con protección; **nunca batería directa**. |

**Fuera de alcance en v1:** CAN, RS485, BMS avanzado (configuración, modo de fábrica, control de MOSFET), diagnóstico multimarca.

**Ampliación decidida por el usuario:** reporte de **solo lectura** de un BMS JBD propio por BLE (Etapa 3c, [`docs/BMS_JBD.md`](docs/BMS_JBD.md)). Es una excepción a «el BLE no escribe», acotada por construcción y vigilada por `tools/check_ble_write_policy.py`. La otra es el **modo de escucha** (solo suscripción a notificaciones, sin enviar datos; [`docs/LISTEN_MODE.md`](docs/LISTEN_MODE.md)).

## Hardware

Hardware confirmado por el propietario del proyecto (pendiente de validación física en cada caso):

| Elemento | Detalle |
|---|---|
| MCU | ESP32-S3-N16R8 (16 MB Flash, 8 MB PSRAM) |
| Placa | ESP32-S3 DevKit con USB |
| Pantalla | TFT 2.4" ILI9341, 240×320, SPI, **sin táctil** |
| Alimentación | USB 5 V |
| Herramientas | Multímetro, protoboard, soldador |

Los pines se centralizan en [`src/config/pins_config.h`](src/config/pins_config.h) y son **provisionales** hasta verificar el modelo físico de la placa. Detalle en [`docs/HARDWARE.md`](docs/HARDWARE.md) y [`docs/PINOUT.md`](docs/PINOUT.md).

## Arquitectura

Firmware modular sobre PlatformIO + framework Arduino (ESP32). Cada módulo vive en su propio directorio bajo `src/`:

```
src/
├── main.cpp         Punto de entrada (esqueleto de arranque)
├── config/          Configuración central: pines, constantes, versión
├── display/         Interfaz TFT (pendiente)
├── uart/            UART Analyzer — captura pasiva (pendiente)
├── ble/             BLE Analyzer — escaneo y GATT de solo lectura (pendiente)
├── electrical/      Electrical Monitor — ADC con entradas protegidas (pendiente)
├── bms/             Reporte de BMS JBD por BLE (única excepción de escritura, 3 lecturas fijas)
├── web/             Interfaz web para teléfono: Wi-Fi AP + HTTP, solo lectura
├── storage/         Almacenamiento de capturas (pendiente)
└── diagnostics/     Análisis e interpretación de datos (pendiente)
```

Cada módulo expondrá una interfaz mínima y no dependerá de los detalles internos de los demás. La capa `diagnostics` consumirá datos de `uart`, `ble` y `electrical`; `display` y `storage` serán consumidores finales.

## Requisitos

- [PlatformIO Core](https://platformio.org/install/cli) o la extensión PlatformIO IDE para Visual Studio Code.
- Python 3.8 o superior (requerido por PlatformIO).
- Cable USB de datos para la placa ESP32-S3.
- Git.

## Instalación

```bash
git clone https://github.com/JAMS2405/ARQUIS-EV-Diagnostic.git
cd ARQUIS-EV-Diagnostic
pip install -U platformio
```

PlatformIO descargará automáticamente la plataforma `espressif32` y el framework Arduino en la primera compilación.

## Compilación

```bash
pio run                # compilar
pio run -t upload      # flashear (requiere la placa conectada)
pio device monitor     # monitor serie (115200 baudios)
```

El estado real de compilación y pruebas se registra en [`docs/TESTING.md`](docs/TESTING.md).

## Etapas de desarrollo

| Etapa | Contenido | Estado |
|:-:|---|---|
| 0 | Estructura del repositorio y documentación | En curso |
| 1 | Base del firmware: arranque, configuración, pantalla TFT | Implementada (rama `feat/stage-1-display`), compila; **pendiente de validación en hardware** |
| 2 | UART Analyzer (pasivo, solo recibe) | Versión inicial (rama `feat/stage-2-uart`), compila y pasa 36 pruebas en PC; **pendiente de validación en hardware** |
| 3 | BLE Analyzer | Versión inicial (rama `feat/stage-3-ble`), compila; **pendiente de validación en hardware** |
| 3b | Interfaz web para teléfono (Wi-Fi AP, solo lectura) | Versión inicial (rama `feat/web-ui`), compila; **pendiente de validación en hardware** |
| 3c | Reporte de BMS JBD (lectura, batería propia) | Versión inicial (rama `feat/bms-jbd-report`), compila y pasa 12 pruebas en PC; **pendiente de validación con un BMS real** |
| 3e | Escucha de notificaciones BLE, solo recibe (Evobike) | Versión inicial (rama `feat/ble-listen`), compila; **pendiente de validación en hardware** |
| 4 | Electrical Monitor | Pendiente |
| 5 | Almacenamiento e integración | Pendiente |

Criterios de aceptación de cada etapa en [`docs/DEVELOPMENT_PLAN.md`](docs/DEVELOPMENT_PLAN.md). Las etapas no se inician sin autorización explícita.

## Seguridad eléctrica

Resumen (ver documento completo en [`docs/ELECTRICAL_SAFETY.md`](docs/ELECTRICAL_SAFETY.md)):

- **Nunca** conectar al ESP32 la tensión de batería, del bus de potencia o de las fases del motor.
- Todas las GPIO del ESP32-S3 toleran **3,3 V máximo**. Medir con multímetro el nivel lógico de cada señal antes de conectarla.
- Señales de 5 V o de nivel desconocido requieren adaptación de nivel y protección.
- Evitar bucles de masa: preferir un aislador USB o un portátil alimentado solo por batería al conectar a un vehículo.
- Este proyecto se entrega **sin garantía** y es de uso bajo responsabilidad del usuario.

## Documentación

| Documento | Contenido |
|---|---|
| [`docs/HARDWARE.md`](docs/HARDWARE.md) | Hardware confirmado y pendiente de verificar |
| [`docs/PINOUT.md`](docs/PINOUT.md) | Asignación provisional de pines |
| [`docs/ELECTRICAL_SAFETY.md`](docs/ELECTRICAL_SAFETY.md) | Qué se puede conectar y qué requiere protección |
| [`docs/DEVELOPMENT_PLAN.md`](docs/DEVELOPMENT_PLAN.md) | Etapas y criterios de aceptación |
| [`docs/TESTING.md`](docs/TESTING.md) | Pruebas ejecutadas, pendientes y simuladas |
| [`docs/UART_TAP.md`](docs/UART_TAP.md) | Captura UART pasiva: circuito protegido, montaje y lectura de resultados |
| [`docs/LISTEN_MODE.md`](docs/LISTEN_MODE.md) | Modo de escucha de notificaciones BLE: uso y límites |
| [`docs/BMS_ONBOARDING.md`](docs/BMS_ONBOARDING.md) | Cómo incorporar una marca nueva de BMS: ficha de datos y criterios |
| [`docs/BMS_JBD.md`](docs/BMS_JBD.md) | Reporte de BMS JBD: alcance, seguridad y límites |
| [`docs/WEB_UI.md`](docs/WEB_UI.md) | Uso, clave y API de la interfaz web |

## Licencia

[MIT](LICENSE) © 2026 JAMS2405
