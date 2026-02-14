# Especificaciones de Diseño 3D (Calculadora V1.0)

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
