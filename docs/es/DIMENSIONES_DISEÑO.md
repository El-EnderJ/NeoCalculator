# 📐 NumOS — Especificaciones de Diseño 3D (Chasis V1.0)

&gt; Dimensiones críticas para el diseño del chasis de la calculadora. Todos los valores son los **mínimos recomendados** basados en las piezas de hardware reales. Siempre mide tus componentes específicos antes de finalizar el diseño.

---

## 1. Componentes Principales — Dimensiones

### ESP32-S3 N16R8 CAM

| Dimensión | Valor |
|:----------|:-----:|
| Largo | ~40 mm |
| Ancho | ~27 mm |
| Altura (con conector USB-C) | ~8 mm |
| Puerto USB-C | Centrado en lado corto — hueco necesario: **12 mm × 6 mm** |
| Montaje | Sin orificios estándar — usar snap-fit lateral o postes con clip |

&gt; El módulo CAM tiene antena WiFi/BT integrada + conector de cámara. Si no se usa la cámara, tapar o cubrir el conector para evitar acumulación de polvo.

### Pantalla TFT IPS 3.2" (ILI9341)

| Dimensión | Valor |
|:----------|:-----:|
| **Área activa (panel LCD)** | 64.8 mm × 48.6 mm |
| **Ventana recomendada en carcasa** | ~66 mm × 50 mm |
| **PCB total** | ~89 mm × 55 mm *(varía por fabricante — medir pieza real)* |
| **Profundidad desde cara frontal** | mínimo **7 mm** (panel + PCB + conector FPC) |
| **Ángulo de visión** | IPS: 170° H/V — sin restricción ergonómica por ángulo |

&gt; Recomendado inclinar la pantalla ~12° respecto a la horizontal del chasis para ergonomía en sobremesa.

### Teclado — Pulsadores 6×6 mm

| Dimensión | Valor |
|:----------|:-----:|
| Cuerpo del pulsador | 6.0 mm × 6.0 mm |
| Altura total (cuerpo + vástago) | ~7 mm (5 mm cuerpo + 2 mm recorrido) |
| **Capuchón impreso** | 7.5 mm × 7.5 mm × 3 mm altura |
| **Hueco en carcasa para capuchón** | 7.9 mm × 7.9 mm (tolerancia 0.4 mm) |

---

## 2. Geometría del Teclado 6×8

### Dimensionado Base

| Parámetro | Valor |
|:----------|:-----:|
| Separación entre centros (X horizontal) | **11 mm** |
| Separación entre centros (Y vertical) | **11 mm** |
| Tamaño de tecla impresa | 7.5 mm × 7.5 mm |
| Espacio libre entre teclas | ~3.5 mm (cómodo para dedos) |
| **Área total del teclado** | ~88 mm (ancho) × ~66 mm (alto) |

### Zonas funcionales del teclado

```
 ┌──────────────────────────────────────────┐
 │   R0: SHIFT │ ALPHA │ MODE │ SETUP │ F1-F4  │  ← Zona de modos (fila top)
 ├──────────────────────────────────────────┤
 │   R1: ON │ AC │ DEL │ FREE_EQ │ ← ↑ ↓ →   │  ← Control + Navegación
 ├──────────────────────────────────────────┤
 │   R2: X │ Y= │ TABLE │ GRAPH │ ZOOM…      │  ← Apps científicas
 ├──────────────────────────────────────────┤
 │   R3: 7 │ 8 │ 9 │ ( │ ) │ ÷ │ ^ │ √      │  ← Operadores superiores
 ├──────────────────────────────────────────┤
 │   R4: 4 │ 5 │ 6 │ × │ - │ sin │ cos │ tan │  ← Trig
 ├──────────────────────────────────────────┤
 │   R5: 1 │ 2 │ 3 │ + │ (-) │ 0 │ . │ = │  ← Numérico base
 └──────────────────────────────────────────┘
```

### Distribución recomendada en chasis

- Pantalla en la mitad superior del frente.
- 8-10 mm de separación entre el borde inferior de la pantalla y la fila superior del teclado.
- Las teclas de función (F1-F4) alineadas debajo del borde inferior de la pantalla, como en calculadoras Casio físicas.

---

## 3. Dimensiones Generales del Chasis

### Versión V1.0 (Apaisada y Compacta)

| Dimensión | Mínimo | Recomendado | Máximo |
|:----------|:------:|:-----------:|:------:|
| **Ancho** | 92 mm | **98 mm** | 105 mm |
| **Alto** | 175 mm | **185 mm** | 200 mm |
| **Grosor (zona de pantalla)** | 12 mm | **15 mm** | 20 mm |
| **Grosor (zona de teclado)** | 10 mm | **13 mm** | 18 mm |

&gt; El rango de 185 mm × 98 mm es equivalente al formato de la Casio fx-570/991, que resulta cómodo de sostener y de tamaño familiar para el usuario.

### Despiece de Grosor

```
Frente:
  ├── 1.5 mm  Panel frontal impreso 3D
  ├── 7.0 mm  Pantalla ILI9341 (panel + PCB)
  ├── 2.0 mm  Espacio para cables FPC / flex
  ├── 2.5 mm  ESP32-S3 CAM
  └── 2.0 mm  Carcasa trasera
  ─────────────────────────
  Total: ~15 mm (sin batería)

Con batería 18650:
  ├── 15 mm   PCB + componentes
  ├── 19 mm   Diámetro batería 18650
  └── 2.0 mm  Tapa trasera
  ─────────────────────────
  Total: ~36 mm
```

---

## 4. Ergonomía y Detalles de Diseño

### Inclinación de Pantalla

- Recomendar **12°** de inclinación en la zona de pantalla respecto a la base para uso en escritorio.
- Opcional: base trasera abatible (como en las TI-84) para ajustar el ángulo entre 5° y 20°.

### Etiquetas de Teclas

- Las etiquetas en teclas de colores secundarios (SHIFT en naranja, ALPHA en verde como en Casio) se pueden lograr con **cambio de filamento** en la impresora 3D a la capa de la etiqueta.
- Alternativa: usar **vinil adhesivo** impreso para las etiquetas de función secundaria.
- Las teclas F1-F4 deben tener el nombre de la función grabado en relieve en la carcasa **por encima** de cada tecla.

### Tapa de Batería (si aplica)

| Parámetro | Valor |
|:----------|:-----:|
| Orificio para 18650 | 18.5 mm diámetro × 70 mm largo |
| Tipo de cierre | Deslizante o tornillo M3 |
| Ubicación | Parte inferior trasera del chasis |

### Tornillería

- **Insertos roscados M3 × 4 mm** en postes de las 4 esquinas internas.
- **Tornillos M3 × 8 mm** cabeza Phillips niquelados.
- 4 tornillos mínimo (uno por esquina); 6 si el chasis supera 190 mm de alto.

---

## 5. Checklist de Diseño e Impresión

### Antes de Imprimir

- [ ] ¿El hueco de la pantalla mide exactamente las dimensiones del **área activa** de tu panel real?
- [ ] ¿El hueco USB-C tiene la orientación correcta (horizontal, en el lado corto inferior)?
- [ ] ¿Los postes de montaje del ESP32-S3 tienen la altura correcta (altura del módulo + 1 mm clearance)?
- [ ] ¿Las columnas de pulsadores están alineadas en rejilla de 11 mm × 11 mm?
- [ ] ¿Los huecos de teclas miden 7.9 × 7.9 mm (0.4 mm tolerancia)?

### Prueba Parcial (recomendada antes de imprimir la pieza completa)

- [ ] Imprimir una sección de 2×3 teclas para verificar tolerancias.
- [ ] Imprimir el marco de la pantalla para verificar el ajuste del panel.
- [ ] Verificar que el conector USB-C del ESP32-S3 es accesible.

### Tras el Ensamblado

- [ ] ¿Las teclas se mueven libremente sin atascarse?
- [ ] ¿El panel TFT está centrado y sin burbujas de luz por los laterales?
- [ ] ¿Los cables no obstruyen el cierre de las dos mitades?
- [ ] ¿Los tornillos cierran sin forzar el plástico?

---

## 6. Referencias de Tamaño — Calculadoras Comerciales

| Modelo | Ancho | Alto | Grosor |
|:-------|:-----:|:----:|:------:|
| Casio fx-991EX | 77 mm | 165 mm | 11 mm |
| Casio fx-CG50 | 89 mm | 188 mm | 18 mm |
| TI-84 Plus CE | 82 mm | 189 mm | 19 mm |
| NumWorks N0110 | 68 mm | 166 mm | 12 mm |
| **NumOS V1.0 (objetivo)** | **98 mm** | **185 mm** | **15 mm** |

---

*Última actualización: Febrero 2026*


Este documento contiene las dimensiones críticas para el diseño del chasis.

## 1. PCB y Componentes Principales

### ESP32 DevKit V1
*   **Dimensiones**: ~52mm x 28mm.
*   **Orificios de montaje**: No tiene orificios estándar, se recomienda sujetar por los laterales (snap-fit) o usar pines de soporte.
*   **Puerto USB**: Centrado en el lado corto inferior. Dejar hueco de **12mm x 8mm**.

### Pantalla TFT 3.2" (ILI9341 con PCB rojo/negro típico)
*   **Área Activa (LCD)**: 48.6mm x 64.8mm.
*   **Marco visible**: Recomiendo ventana de **50mm x 66mm**.
*   **PCB Total**: ~55mm x 89mm (varía según fabricante, medir pieza real).
*   **Profundidad**: Dejar al menos **6mm** desde la cara frontal.

### Matriz de Teclas (6x8 = 48 teclas)
*   **Layout**: Rejilla de 6 filas x 8 columnas.
*   **Tamaño de tecla**: 8mm x 8mm (recomendado) con espaciado de 3mm.
*   **Separación entre centros**:
    *   Horizontal (X): 11mm
    *   Vertical (Y): 11mm
*   **Área total del teclado**: ~88mm (ancho) x ~66mm (alto).
*   **Botones**:
    *   Imprimir botones de 7.5mm x 7.5mm para huecos de 8mm.
    *   altura base: 2mm (faldón) + altura vástago.

## 2. Dimensiones Generales del Chasis

*   **Ancho**: 95mm - 100mm (Cómodo para sostener a una mano).
*   **Alto**: 180mm - 190mm (Estilo calculadora científica estándar).
*   **Grosor**:
    *   Base: 15mm - 20mm (depende de batería).
    *   Incluir inclinación opcional de 10-15 grados en la zona de pantalla.

## 3. Ergonomía y Detalles

*   **Etiquetas**:
    *   Las teclas **F1-F4** y **Mode/Setup** deben tener etiquetas impresas o grabadas en el chasis sobre la tecla.
    *   Usa el cambio de filamento (capas superiores) para texto en otro color si es posible.
*   **Tapa de Batería**:
    *   Si usas 18650: Diámetro 18.5mm, Largo 65mm.
    *   Diseñar tapa deslizante o con tornillo M3 en la parte trasera.
*   **Tornillería**:
    *   Usar insertos roscados M3 x 4mm para cerrar las dos mitades de la carcasa.
    *   4 tornillos en las esquinas traseras.

## 4. Checklist de Impresión

- [ ] Verificar tolerancias de teclas (0.4mm gap).
- [ ] Puenteado correcto para puerto USB.
- [ ] Refuerzos en los postes de tornillos.
