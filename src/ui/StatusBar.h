/**
 * StatusBar.h — Barra de estado global LVGL (24 px)
 *
 * Widget reutilizable que muestra en la franja superior de pantalla:
 *   · Reloj 24 h   (hora:minuto, <time.h> + oscilador interno ESP32)
 *   · Título de app (centrado)
 *   · Modificadores (S / A / S-LOCK / A-LOCK / STO)  ← KeyboardManager
 *   · Modo angular (DEG / RAD)                        ← g_angleMode
 *   · Icono de batería (relleno dinámico 0-100 %)
 *
 * Estética: fondo #1A1A1A, texto gris claro / blanco, línea inferior.
 *
 * Uso:
 *   StatusBar bar;
 *   bar.create(parentScreen);      // lo posiciona en (0,0)
 *   bar.setTitle("Calculation");
 *   bar.update();                   // refresca indicadores
 *   bar.setBatteryLevel(85);
 */

#pragma once

#include <lvgl.h>
#include <cstdint>

namespace ui {

class StatusBar {
public:
    // ── Constantes de diseño ─────────────────────────────────────────────
    static constexpr int HEIGHT     = 24;
    static constexpr int SCREEN_W   = 320;
    static constexpr uint32_t COL_BG   = 0x1A1A1A;  ///< Fondo oscuro
    static constexpr uint32_t COL_TEXT = 0xCCCCCC;   ///< Texto gris claro
    static constexpr uint32_t COL_SEP  = 0x333333;   ///< Línea inferior

    // ── Ciclo de vida ────────────────────────────────────────────────────
    StatusBar() = default;

    /**
     * Crea el widget dentro de @p parent (normalmente una pantalla LVGL).
     * Ocupa toda la anchura y 24 px de alto, posición (0,0).
     */
    void create(lv_obj_t* parent);

    /** Destruye los widgets LVGL (no llama delete en la pantalla padre). */
    void destroy();

    // ── Contenido dinámico ───────────────────────────────────────────────

    /** Establece el título centrado (nombre de la app activa). */
    void setTitle(const char* title);

    /** Nivel de batería visual (0-100). */
    void setBatteryLevel(uint8_t pct);

    /**
     * Refresca TODOS los indicadores dinámicos:
     *   - Reloj (avanza con oscilador interno)
     *   - Modificador (lee KeyboardManager)
     *   - Modo angular (lee g_angleMode)
     *   - Batería (redibuja relleno)
     * Llamar periódicamente o tras cada tecla.
     */
    void update();

    /** Devuelve el objeto LVGL raíz (para set pos, etc.). */
    lv_obj_t* obj() const { return _bar; }

    /** Devuelve la altura del StatusBar (para calcular offsets). */
    static constexpr int height() { return HEIGHT; }

private:
    // ── Widgets LVGL ─────────────────────────────────────────────────────
    lv_obj_t* _bar        = nullptr;  ///< Contenedor principal
    lv_obj_t* _clockLabel = nullptr;  ///< "HH:MM"
    lv_obj_t* _titleLabel = nullptr;  ///< Título centrado
    lv_obj_t* _modLabel   = nullptr;  ///< Indicador de modificador
    lv_obj_t* _angleLabel = nullptr;  ///< "DEG" / "RAD"
    lv_obj_t* _batIcon    = nullptr;  ///< Canvas del icono batería
    lv_obj_t* _separator  = nullptr;  ///< Línea inferior 1 px

    uint8_t   _batLevel   = 100;      ///< Porcentaje de batería

    // ── Helpers ──────────────────────────────────────────────────────────
    void updateClock();
    void updateModifier();
    void updateAngleMode();
    void updateBatteryIcon();
};

} // namespace ui
