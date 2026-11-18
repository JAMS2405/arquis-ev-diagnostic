# Incorporar una marca nueva de BMS

Cada marca usa su propio protocolo. Un módulo nuevo se construye **a partir de datos reales** de una unidad y se valida contra una referencia conocida (la app oficial y el multímetro). No se implementa de memoria.

> Regla de seguridad (igual que `BMS_JBD.md`): solo **lecturas** y solo con baterías propias o con permiso del cliente. Nunca configuración, modo de fábrica, control de MOSFET ni escritura de parámetros.

## 1. Ficha de datos por marca (qué guardar)

Copiar esta ficha y completarla **con la batería delante**. No subir fichas con datos de clientes al repositorio (`reports/` y `captures/` están en `.gitignore`).

```
Marca / modelo:
Tensión nominal y nº de celdas (si se sabe):
¿Tiene BLE? ¿Nombre de la app oficial?:
¿Cómo se conecta el BMS?: BLE / UART por cable / CAN / otro / no se sabe
Fecha y hora de la captura:

A) Referencia de la app oficial
   - Capturas de pantalla con TODOS los valores (tensión, celdas, ciclos,
     capacidad, temperaturas, protecciones), con la hora visible.

B) Datos del analizador (la batería cerca, app oficial CERRADA)
   - Línea del dispositivo en `scan`:
   - Salida completa de `gatt <n>`:

C) Medición independiente (mismo momento que A)
   - Tensión total con multímetro:
   - Tensión de cada celda con multímetro (si es accesible):

D) Opcional, muy útil: registro HCI de Bluetooth del teléfono Android
   mientras la app oficial conecta y muestra los datos (archivo btsnoop).
   Revela las peticiones y respuestas reales. Contiene el tráfico Bluetooth
   de TODO el teléfono: usarlo con el teléfono sin otros dispositivos
   emparejados en uso y no publicarlo.
```

## 2. Qué determino con eso

1. **Familia del protocolo**, por los servicios/UUID de `gatt` (o por el cableado si no es BLE).
2. **Si el camino es viable por BLE**: algunos fabricantes cifran o autentican la conexión; sin documentación pública o sin poder reproducir el intercambio, esa marca queda fuera.
3. **Lista exacta de peticiones de lectura** necesarias. Todo lo demás queda inalcanzable por diseño.

## 3. Criterios para aceptar un módulo de marca

- Biblioteca de protocolo **pura** en `lib/<marca>_protocol`, con pruebas en el PC (`pio test -e native`) construidas con **tramas reales** capturadas, no inventadas.
- Una enumeración **cerrada** de peticiones de lectura; sin forma de construir otras.
- La política `tools/check_ble_write_policy.py` actualizada para permitir solo ese archivo.
- Comparación con la app oficial y el multímetro en una unidad real, registrada en `TESTING.md`. Tolerancia **propuesta**: ≤ 10 mV por celda y ≤ 0,5 % en tensión total frente al multímetro (a confirmar según la precisión de tu instrumento).
- Documento por marca con qué datos entrega y cuáles **no**.

## 4. Orden recomendado

1. Recolectar la ficha de las marcas que ya tienes en el taller.
2. Resolver primero la marca con **BLE y servicios abiertos**, que es la más rápida de validar.
3. Cuando entre la **segunda** marca, generalizar el modelo común de reporte (el reporte actual es de JBD) con datos reales de dos protocolos, en lugar de diseñarlo a ciegas ahora.
