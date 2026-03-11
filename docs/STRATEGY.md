# NumOS Strategic Blueprint: Investor & Design Roadmap

This blueprint outlines the vision to transform **NumOS** from a high-performance OS into a market-disrupting product and investor-ready brand.

---

## 1. Product-Market Fit: The "Calculator Disruption" Moat

Investors look for defensible advantages. NumOS has three distinct technical barriers that translate into massive business value:

### Moat 1: Pro-CAS Symbolic Engine (Symbolic Supremacy)
*   **Technical Asset**: Immutable DAG with hash-consing, Slagle integration, and symbolic differentiation in PSRAM.
*   **Business Value**: NumOS provides $180 (TI-Nspire/HP Prime) functionality for $15 in hardware.
*   **Metric**: **12x ROI per unit** for users. Educational systems can deploy university-level tools at primary school prices.

### Moat 2: Hybrid PSRAM/DMA Architecture (Performance Efficiency)
*   **Technical Asset**: Zero-copy display pipeline and 8MB PSRAM management.
*   **Business Value**: High-end UX normally requiring $30 Linux-based ARM chips.
*   **Metric**: **70% BoM reduction** vs. competitors with similar UI fluidities.

### Moat 3: Multi-Physics Simulation Ecosystem (Vertical Integration)
*   **Technical Asset**: Native SPICE (Circuit), Navier-Stokes (Fluid), and Verlet (Optics) integration.
*   **Business Value**: Not just a calculator, but a handheld laboratory.
*   **Metric**: **High Community Retention**. Open-source developers can add specialized lab apps in hours, not months.

---

## 2. Visual Concept: "Cyberpunk Industrial & High-Level Calculus"

The brand must bridge the gap between "Open-Source Hacker" and "NASA-level precision."

*   **Narrative**: "The Raw Power of Physics, Handheld."
*   **Color Palette**:
    *   **Main**: `#0A0A0E` (Dark Obsidian) — Deep, focused background.
    *   **Accent**: `#AFFE00` (Cyber Lime) — Glow effects for correct results and "active" code.
    *   **Secondary**: `#22222A` (Circuit Grey) — Industrial UI containers.
    *   **Highlight**: `#00D1FF` (Electric Blue) — Ray-tracing and simulation highlights.
*   **Typography**:
    *   **Headlines**: `Inter` (Extra Bold) — Tight tracking, modern, authoritative.
    *   **Data/Code**: `JetBrains Mono` — The universal language of engineering.
    *   **Math**: `KaTeX` (TeX) — The gold standard for scientific documentation.

---

## 3. Landing Page Architecture (The Investor Journey)

1.  **The Hero (Impact)**: A 3D render of the NumOS device calculating a complex integral in Real-Time. Headline: *"Scientific Mastery, Decolonized."*
2.  **The Pain Point (Disruption)**: A brutalist comparison table. TI-84/Casio ($150+) vs. NumOS ($15 DIY).
3.  **The Tech Stack (The "How")**: Interactive "X-Ray" view of the PCB. Hovering over the ESP32-S3 reveals the LVGL dual-buffer logic.
4.  **The Engine (Interactive)**: A "CAS Playground" where investors can type `d/dx sin(x^2)` and see the symbolic step-logger in action.
5.  **Roadmap (Vision)**: SVG-connected nodes from "Phase 1: Boot" to "Phase 9: Global Edu Distribution."

---

## 4. Visual Effects Strategy (The "WOW" Factors)

1.  **Circuit Flow Scroll**: As the user scrolls, background "circuits" light up with Cyber Lime particles, representing data moving through the PSRAM.
2.  **Shader Integration**: A Hero background using GLSL to render a 2D fluid simulation (Fluid2DApp port) that reacts to the mouse cursor.
3.  **Code-to-Math Morphing**: When reaching the CAS section, raw C++ code transforms into a beautifully rendered KaTeX integral via a GSAP transition.

---

## 5. Technology Research (Static GitHub Pages)

*   **Framework**: `Vite` (for optimized static builds).
*   **3D/Graphics**: `Three.js` + `React Three Fiber` (Low-overhead 3D device model).
*   **Animations**: `GSAP` + `ScrollTrigger` (Industry standard for complex scroll-based storytelling).
*   **Math**: `KaTeX` (Lightest and fastest math renderer).
*   **Styling**: `Vanilla Extract` or `Tailwind` with custom design tokens for performance.

---

## 6. Phase-Based Implementation Roadmap

1.  **Fase Alpha (Planning)**: Finalize high-fidelity mockups in Figma using the Cyberpunk Design System.
2.  **Fase Beta (Assets)**: 3D modeling of the calculator and GLSL shader development.
3.  **Fase Delta (Interaction)**: Implementation of the "CAS Playground" using a JS-compiled version of the C++ logic (via Emscripten).
4.  **Fase Omega (Live)**: Performance optimization for mobile devices and investor notification.

> [!IMPORTANT]
> This plan ensures that investors perceive NumOS not as a weekend project, but as a sophisticated hardware/software platform ready for mass-market hardware production.
