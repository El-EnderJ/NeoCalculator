# Documentación Maestra: Calculadora Científica ESP32 Neo

[TOC]

## 1. Visión General del Proyecto
Este proyecto es una **Calculadora Científica Gráfica** basada en el microcontrolador **ESP32**, diseñada para competir con modelos comerciales (Casio fx-991EX, TI-84) mediante una pantalla TFT a color, display natural (fórmulas matemáticas renderizadas correctamente) y una arquitectura modular de aplicaciones.

**Objetivo Principal**: Crear un sistema operativo de calculadora robusto, eficiente y visualmente atractivo que sea fácil de extender con nuevas funciones.

---

## 2. Arquitectura del Software
El sistema sigue un patrón de diseño orientado a **Aplicaciones (Apps)** gestionadas por un **SystemApp** central.

### 2.1 Componentes Clave
1.  **SystemApp (Dispatcher)**:
    *   Gestiona el bucle principal (`update`, `render`).
    *   Recibe eventos de teclado (`KeyMatrix`) y los despacha a la App activa.
    *   Gestiona la barra de estado global y menús de superposición (opciones, settings).
    *   Contiene punteros a las Apps instanciadas (`_calcApp`, `_grapherApp`).

2.  **Apps Individuales**:
    *   **CalculationApp**: Calculadora principal con historial, display natural y cursor 2D.
    *   **GrapherApp**: Graficadora de funciones, con pestañas (Expresiones, Gráfico, Tabla).
    *   *Futuro*: EquationSolver, MatrixApp, etc.

3.  **Motor Matemático (Math Engine)**:
    *   **ExprNode**: Estructura de árbol para representar expresiones matemáticas (Texto, Fracciones, Raíces, Potencias).
    *   **Tokenizer**: Convierte strings planos en tokens.
    *   **Parser (Shunting-Yard)**: Convierte tokens a Notación Polaca Inversa (RPN) para evaluación, o construye árboles `ExprNode` para visualización.
    *   **Evaluator**: Ejecuta el cálculo numérico sobre la RPN.
    *   **VariableContext**: Almacena variables persistentes (`A-Z`, `Ans`).

4.  **Drivers y HAL**:
    *   **DisplayDriver**: Wrapper sobre `TFT_eSPI` para abstracción de dibujo.
    *   **KeyMatrix**: Escaneo de matriz de teclado (hardware específico).
    *   **Theme**: Definiciones dcolor y constantes visuales.

---

## 3. Estructura de Directorios (`src/`)

```
src/
├── apps/               # Lógica de las aplicaciones
│   ├── CalculationApp  # App Principal (Cálculo Natural)
│   ├── GrapherApp      # App Graficadora
│   └── (Futuras apps)
├── math/               # Núcleo matemático
│   ├── Evaluator       # Ejecutor de operaciones
│   ├── ExprNode        # Nodos del árbol de expresión (AST visual)
│   ├── Parser          # Analizador sintáctico
│   ├── Tokenizer       # Analizador léxico
│   └── VariableContext # Gestión de memoria de variables
├── display/            # Controladores de pantalla
│   └── DisplayDriver   # API gráfica de bajo nivel
├── input/              # Controladores de entrada
│   └── KeyMatrix       # Driver de teclado matricial
├── ui/                 # Elementos de interfaz gráfica
│   ├── Icons           # Bitmaps y activos gráficos
│   └── Theme           # Paleta de colores y estilos
├── SystemApp.cpp/.h    # Gestor de aplicaciones y ciclo de vida
├── main.cpp            # Punto de entrada (setup/loop)
└── Config.h            # Configuración global de hardware (pines)
```

---

## 4. Estado Actual e Implementación
A fecha de **Febrero 2026**, el sistema cuenta con:

### 4.1 CalculationApp (Estable)
*   [x] **Natural Display**: Renderizado recursivo de fracciones complejas, raíces anidadas y potencias (superscript).
*   [x] **Cursor Inteligente**: Navegación 2D real. Permite entrar y salir de estructuras (numerador -> denominador, radicando -> índice).
*   [x] **Historial**: Scroll vertical de cálculos anteriores con selección y reutilización.
*   [x] **Edición**: Inserción, borrado (backspace inteligente que desmonta estructuras) y modificación en sitio.

### 4.2 GrapherApp (Funcional)
*   [x] **Pestañas**: Navegación cíclica entre "Expresiones", "Gráfico" y "Tabla" (Shift + Flechas).
*   [x] **Editor de Funciones**: Input de hasta 3 funciones (Y1, Y2, Y3).
*   [x] **Plotter Optimizado**: Renderizado de gráficos con escalado automático y clipping. Parseo de expresión optimizado (1 vez por frame).
*   [x] **View Control**: Zoom y Panning básico.

### 4.3 Sistema
*   [x] **Gestión de Teclado**: Soporte para pulsaciones simples, repetidas y modificadores (SHIFT/ALPHA).
*   [x] **Barra de Estado**: Indicador de modo (DEG/RAD), batería (simulado) y App activa.

---

## 5. Guía de Desarrollo

### Cómo añadir una nueva función matemática:
1.  **Tokenizer**: Añadir el token en `TokenType` y la regla de parseo.
2.  **ExprNode**: Si requiere dibujo especial (como integral o sumatorio), crear un `NodeType` y su lógica de `measure`/`draw` en `CalculationApp`.
3.  **Evaluator**: Implementar la operación matemática real.

### Cómo mejorar el rendimiento gráfico:
*   El renderizado actual redibuja toda la pantalla en cada cambio (`tft.fillScreen`).
*   **Mejora futura**: Implementar "dirty rectangles" o renderizado parcial para solo actualizar la zona del cursor o la expresión modificada.

### Estilo de Código:
*   Nombres de clases: `PascalCase`.
*   Métodos y variables: `camelCase`.
*   Variables privadas: `_variable`.
*   Bloques: `Bracket` en la misma línea o siguiente (consistente por archivo).

---

## 6. Roadmap / Siguientes Pasos

### Prioridad Alta (Corto Plazo)
1.  **Settings App**: Menú para configurar brillo, timeout, modo angular global (DEG/RAD/GRA).
2.  **Persistencia**: Guardar variables y configuraciones en la memoria Flash/EEPROM del ESP32 (`Preferences.h`).
3.  **Tabla de Valores (GrapherApp)**: Implementar la vista de tabla que actualmente está vacía.

### Prioridad Media
1.  **Equation Solver**: App para resolver sistemas lineales y cuadráticas/cúbicas.
2.  **Estadística**: App para listas de datos y regresiones.
3.  **Teclado Completo**: Implementar overlay de teclas ALPHA y funciones secundarias (amarillas/rojas) en pantalla cuando se pulsa SHIFT.

### Prioridad Baja / Largo Plazo
1.  **Evaluación Simbólica (CAS)**: Capacidad de simplificar expresiones algebraicas (`2x + x -> 3x`). *Complejidad Alta*.
2.  **Conectividad**: Uso de WiFi/BLE para transferir programas o actualizar firmware.
3.  **Scripting**: Intérprete ligero de Lua o MicroPython embebido.


