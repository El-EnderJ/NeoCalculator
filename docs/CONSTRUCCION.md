# Guía de Construcción y Test de Hardware

Este documento detalla el proceso de construcción de la Calculadora Graficadora ESP32.

## 1. Diagrama de Conexionado (Wiring)

### MCU: ESP32 DevKit V1 (38 pines)
La alimentación principal entra por el puerto USB (5V) o por el pin VIN (5V-9V) si usas batería externa + booster.

### Pantalla TFT 3.2" SPI (ILI9341)

| Pin TFT | Pin ESP32 | Función | Notas |
| :--- | :--- | :--- | :--- |
| VCC | 3V3 | Alimentación | Usar la salida de 3.3V del ESP32 |
| GND | GND | Tierra | Común con ESP32 |
| CS | GPIO 5 | Chip Select | |
| RESET | GPIO 17 | Reset | |
| DC/RS | GPIO 16 | Data/Command | |
| SDI/MOSI | GPIO 23 | Data In | SPI MOSI |
| SCK | GPIO 18 | Clock | SPI SCK |
| LED | GPIO 4 | Backlight | Se recomienda usar transistor (NPN o MOSFET) |
| SDO/MISO | GPIO 19 | Data Out | Opcional (para lectura/touch) |

### Matriz de Teclado 6x8

La matriz consta de **6 filas** (entradas con pull-up interno) y **8 columnas** (salidas).
**NO SE REQUIEREN RESISTENCIAS EXTERNAS** para las filas, ya que se activan las pull-ups internas del ESP32 (`INPUT_PULLUP`).

#### Mapeo de Pines

| Fila (Input) | GPIO ESP32 | | Columna (Output) | GPIO ESP32 |
| :--- | :--- | :--- | :--- | :--- |
| **R0** | GPIO 32 | | **C0** | GPIO 13 |
| **R1** | GPIO 33 | | **C1** | GPIO 19 |
| **R2** | GPIO 25 | | **C2** | GPIO 21 |
| **R3** | GPIO 26 | | **C3** | GPIO 22 |
| **R4** | GPIO 27 | | **C4** | GPIO 0 (*) |
| **R5** | GPIO 14 | | **C5** | GPIO 2 (*) |
| | | | **C6** | GPIO 12 (*) |
| | | | **C7** | GPIO 15 (*) |

*> **ATENCIÓN**: Las columnas C4, C5, C6 y C7 (GPIO 0, 2, 12, 15) son pines de "strapping" que determinan el modo de arranque del ESP32. **NO MANTENER PULSADAS** teclas de estas columnas durante el encendido o reset, o el ESP32 podría entrar en modo bootloader/fallo.*

## 2. Instrucciones de Impresión 3D (Ender CR6 SE)

Para una carcasa robusta y teclas funcionales:

### Parámetros Generales
*   **Material**: PLA o PETG.
*   **Altura de capa**: 0.2mm (0.12mm para las teclas para mejor detalle).
*   **Relleno**: 20% para carcasa, 100% para teclas.
*   **Soportes**: Solo necesarios en huecos de puertos si no se diseñan con puentes.

### Tolerancias para Botones
*   Diseña los botones con una tolerancia de **0.4mm** respecto al hueco de la carcasa.
    *   *Ejemplo*: Si el botón mide 6.0mm x 6.0mm, el hueco en la carcasa debe ser de 6.4mm x 6.4mm.
    *   Esto asegura que las teclas no se atasquen y tengan un recorrido suave.
*   Usa **pulsadores táctiles de 6x6mm** de altura adecuada (ej. 5mm + altura de tecla impresa).
*   Imprime una **prueba de ajuste** con 4-5 teclas antes de imprimir la carcasa completa.

## 3. Test de Hardware

Para verificar soldaduras y conexiones, compila y sube el código de prueba `src/HardwareTest.cpp` (renómbralo temporalmente a `main.cpp` o ajusta `platformio.ini` para usarlo).

Este test mostrará en la pantalla TFT cada tecla que presiones (Fila, Columna y Código).
Si una tecla no responde:
1.  Revisa el cableado de esa fila/columna específica.
2.  Verifica con multímetro la continuidad.
3.  Asegúrate de que el diodo (si usaste) esté en la dirección correcta (cátodo a columna).

## 4. Batería (Opcional)

Si añades batería Li-Ion (3.7V):
*   Usa un módulo **TP4056** para carga.
*   Usa un módulo **Boost (MT3608)** para elevar a 5V y alimentar por el pin 5V (VIN), o alimenta directo a 3.3V (menos recomendado por estabilidad).
*   **Monitor de batería**: Conecta un divisor de tensión (ej. 100k + 100k) desde V_BAT a un pin analógico (ej. GPIO 35) para leer el nivel. (Actualmente no implementado en software).
