# Reporte de BMS JBD

> **Estado: implementado y compilando; NO validado en un BMS real.** Ver `TESTING.md` (S-008 a S-010 y pruebas P-060 a P-064 pendientes).
> **Alcance ampliado por decisión del usuario** (el README dejaba «BMS avanzado» fuera de la v1). Solo lectura de datos; úsalo **únicamente en baterías propias**.

## Qué hace

Se conecta por BLE a un BMS JBD (Xiaoxiang) y obtiene un reporte con lo que el BMS informa:

| Dato | Origen |
|---|---|
| Nombre BLE, dirección, nombre de hardware, versión de software | anuncio BLE + comando `0x05` |
| Fecha de fabricación | `0x03` |
| **Ciclos de carga** | `0x03` |
| Tensión del pack, corriente (signo: carga/descarga), potencia derivable | `0x03` |
| Capacidad restante y nominal, RSOC | `0x03` |
| Estado de los MOSFET de carga y descarga | `0x03` |
| Protecciones activas (13 posibles) | `0x03` |
| Temperaturas de los sensores NTC | `0x03` |
| Número de celdas, **tensión de cada celda**, mínima/máxima/promedio/diferencia, celdas en balanceo | `0x03` + `0x04` |

## Qué NO puede dar (y por qué)

- **Parámetros configurados** (umbrales de protección, capacidad de diseño, etc.): requieren entrar en el «modo de fábrica» del BMS, que es una **escritura**. No se implementa.
- **Estado de salud (SOH) real o capacidad medida**: el BMS no lo calcula. Solo una descarga completa controlada lo mide.
- Historial de eventos o contadores de protección: no forman parte de los tres registros de lectura usados.

## Uso

**Desde el teléfono (web):** escanea, pulsa **Reporte BMS JBD** en el dispositivo, confirma el aviso, y descarga el reporte como `.md` o `.json`. La fecha se añade en el teléfono al descargar.

**Por serie:** `scan`, luego `jbd <n>`.

Cierra antes la app oficial del BMS: un BMS atiende a un solo cliente BLE.

## Seguridad: la excepción a «BLE solo lectura»

El resto del proyecto no escribe en ningún dispositivo BLE. Este módulo es la **única excepción**, y está acotada **por construcción**:

1. `lib/jbd_protocol` solo puede construir 3 peticiones (`0x03`, `0x04`, `0x05`), todas con opcode de lectura `0xA5` y longitud 0. La API recibe un `enum` cerrado; no admite bytes arbitrarios.
2. Esas tramas se escriben únicamente en `0xFF02` del servicio `0xFF00`. La otra escritura es la suscripción a `0xFF01`.
3. No se toca el servicio OTA de Telink (`00010203-…-1912`) ni ninguna otra característica.
4. Solo se ejecuta tras una acción explícita (botón con aviso, o `jbd <n>`).
5. `python3 tools/check_ble_write_policy.py` falla si otro archivo llama a `writeValue`, `subscribe` u operaciones de seguridad.
6. Pruebas en el PC (`pio test -e native`) comprueban que todas las peticiones son de lectura.

## Límites del reporte (importante si la batería es para venta)

- Todo lo que se muestra es **lo que el BMS dice**. **No es una prueba de capacidad.** El contador de ciclos y otros datos pueden haberse reprogramado.
- Se recomienda presentarlo como «lectura del BMS», no como certificación, y complementarlo con una prueba de descarga si el comprador pide capacidad real.
- El signo de la corriente (positivo = cargando) viene de la documentación pública y **está pendiente de verificar** con carga real (P-064).
- Los valores de celdas y totales se leen en momentos ligeramente distintos; pueden diferir unos mV.

## Protocolo (documentación pública, validado parcialmente)

- Petición: `DD A5 <cmd> 00 <chk> 77`, con `chk = 0x10000 − cmd`. La petición `0x04` coincide byte a byte con la que apareció en el buffer de un BMS JBD real (H-004).
- Respuesta: `DD <cmd> <estado> <len> <datos> <chk> 77`, con `chk = 0x10000 − (estado + len + Σdatos)`. La fórmula se verificó con una trama de ejemplo de la documentación pública (checksum `FBFF`).
- La disposición de campos de `0x03` está en `lib/jbd_protocol/src/jbd_protocol.h`.
