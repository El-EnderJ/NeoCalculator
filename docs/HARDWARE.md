# HARDWARE – Calculadora Graficadora/Científica ESP32

Este documento describe el wiring recomendado y la matriz de teclas para la calculadora basada en ESP32 con pantalla TFT de 3.2" (ILI9341).

## MCU

- **Placa**: ESP32 DevKit V1 (38 pines).
- **Alimentación**:
  - Entrada típica por USB (5V).
  - El regulador del DevKit genera 3.3V para la lógica.
  - Opcional: batería Li‑Ion + módulo de carga (p.ej. TP4056) y booster a 5V si se desea portabilidad.

## Pantalla TFT 3.2" (ILI9341, SPI)

Conexión recomendada (coincide con `src/Config.h` y debe alinearse con la configuración de `TFT_eSPI`):

| ESP32        | TFT ILI9341 | Descripción                          |
|--------------|-------------|--------------------------------------|
| 3V3          | VCC         | Alimentación lógica 3.3V            |
| GND          | GND         | Tierra común                         |
| GPIO23       | SDI/MOSI    | Datos SPI hacia el TFT               |
| GPIO18       | SCK         | Reloj SPI                            |
| GPIO5        | CS          | Chip Select de la pantalla           |
| GPIO16       | DC/RS       | Data/Command                         |
| GPIO17       | RST         | Reset de la pantalla                 |
| GPIO4 (PWM)  | LED/BL      | Backlight (vía transistor + R serie) |
| (opcional)19 | SDO/MISO    | Lectura SPI desde el TFT             |

> Nota: El backlight se recomienda controlar mediante un transistor NPN o MOSFET canal N, con resistencia en la base/puerta y, si es necesario, resistencia limitadora de corriente en serie con el LED.

## Matriz de teclas 6x8

La calculadora utiliza una matriz de **6 filas x 8 columnas** (48 teclas).  
Las columnas se configuran como **salidas**, y las filas como **entradas con `INPUT_PULLUP` interno**. Se han evitado los GPIO 34, 35, 36 y 39 (solo entrada y sin pull-up) para no requerir resistencias externas.

### Pines ESP32 ↔ Matriz

| Fila/Columna | GPIO ESP32 | Dirección | Comentario                                                                 |
|--------------|-----------:|----------|---------------------------------------------------------------------------|
| R0           | GPIO32     | Entrada  | Fila 0 (superior), con `INPUT_PULLUP` interno                             |
| R1           | GPIO33     | Entrada  | Fila 1, con `INPUT_PULLUP` interno                                        |
| R2           | GPIO25     | Entrada  | Fila 2, con `INPUT_PULLUP` interno                                        |
| R3           | GPIO26     | Entrada  | Fila 3, con `INPUT_PULLUP` interno                                        |
| R4           | GPIO27     | Entrada  | Fila 4, con `INPUT_PULLUP` interno                                        |
| R5           | GPIO14     | Entrada  | Fila 5 (inferior), con `INPUT_PULLUP` interno                             |
| C0           | GPIO13     | Salida   | Columna 0                                                                 |
| C1           | GPIO19     | Salida   | Columna 1                                                                 |
| C2           | GPIO21     | Salida   | Columna 2                                                                 |
| C3           | GPIO22     | Salida   | Columna 3                                                                 |
| C4           | GPIO0      | Salida   | Columna 4 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |
| C5           | GPIO2      | Salida   | Columna 5 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |
| C6           | GPIO12     | Salida   | Columna 6 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |
| C7           | GPIO15     | Salida   | Columna 7 (**pin de arranque**, evitar mantener pulsada esta tecla al boot) |

### Layout lógico de teclas

La disposición se inspira en calculadoras TI‑84/Casio FX, aprovechando teclas de `SHIFT` y `ALPHA` para segundas funciones.

#### Fila R0 (superior) – Modos y softkeys

| Columna | Tecla   |
|---------|---------|
| C0      | SHIFT   |
| C1      | ALPHA   |
| C2      | MODE    |
| C3      | SETUP   |
| C4      | F1      |
| C5      | F2      |
| C6      | F3      |
| C7      | F4      |

#### Fila R1 – Control global y navegación

| Columna | Tecla                 |
|---------|-----------------------|
| C0      | ON                    |
| C1      | AC (Clear All)       |
| C2      | DEL                   |
| C3      | Free Eq (toggle)     |
| C4      | LEFT                  |
| C5      | UP                    |
| C6      | DOWN                  |
| C7      | RIGHT / F5           |

#### Fila R2 – Graficadora y solver

| Columna | Tecla          |
|---------|----------------|
| C0      | X (variable)   |
| C1      | Y=/f(x)        |
| C2      | TABLE          |
| C3      | GRAPH          |
| C4      | ZOOM           |
| C5      | TRACE          |
| C6      | SHOW STEPS     |
| C7      | SOLVE          |

#### Fila R3 – Paréntesis y operaciones altas

| Columna | Tecla                 |
|---------|-----------------------|
| C0      | 7                     |
| C1      | 8                     |
| C2      | 9                     |
| C3      | (                     |
| C4      | )                     |
| C5      | ÷                     |
| C6      | ^ (potencia)         |
| C7      | √ (SHIFT: x²)        |

#### Fila R4 – Operaciones y trigonometría

| Columna | Tecla   |
|---------|---------|
| C0      | 4       |
| C1      | 5       |
| C2      | 6       |
| C3      | ×       |
| C4      | −       |
| C5      | sin     |
| C6      | cos     |
| C7      | tan     |

#### Fila R5 (inferior) – Numérico base

| Columna | Tecla        |
|---------|--------------|
| C0      | 1            |
| C1      | 2            |
| C2      | 3            |
| C3      | +            |
| C4      | (−) cambio signo |
| C5      | 0            |
| C6      | .            |
| C7      | ENTER (=)    |

### Escaneo de la matriz

Estrategia de escaneo recomendada:

1. Configurar todas las columnas a estado HIGH (o HIGH‑Z cuando no estén activas).
2. Para cada columna C_i:
   - Poner C_i en LOW.
   - Leer el estado de todas las filas R_j (con `INPUT_PULLUP` interno).
   - Si una fila lee LOW, la tecla (R_j, C_i) está pulsada.
   - Restaurar C_i a HIGH antes de pasar a la siguiente.

Con esto se evitan conflictos y se puede implementar debounce y autorepetición en firmware.  
Si en el futuro se requiere soporte de múltiples teclas simultáneas complejas, se pueden añadir diodos por tecla para evitar “ghosting”.

## Consideraciones de carcasa 3D

- Pantalla inclinada aproximadamente **10–15°** respecto a la base para mejor ergonomía visual.
- Uso de pulsadores táctiles de **6x6mm** con capuchones impresos para un tacto similar al de calculadoras comerciales.
- Dejar espacio suficiente entre filas/columnas de teclas para una escritura cómoda (similar a TI‑84/FX‑570).
- Prever volumen interno para la PCB de la matriz de teclas, la placa ESP32 y, si se usa, una batería Li‑Ion.

