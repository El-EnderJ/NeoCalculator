/**
 * LvglKeypad.h
 * Driver de entrada de teclado para LVGL 9.x  (tipo KEYPAD)
 *
 * Conecta el KeyMatrix físico con el sistema de grupos/foco de LVGL.
 * Actúa como un anillo de eventos: SystemApp (o cualquier otro módulo)
 * encola pulsaciones con pushKey(); LVGL las consume en su tick via
 * el read_cb estático.
 *
 * Uso:
 *   1. Llamar LvglKeypad::init() después de lv_init().
 *   2. Asignar el indev al grupo de la pantalla activa:
 *        lv_indev_set_group(LvglKeypad::indev(), myGroup);
 *   3. Cuando se detecta una tecla, llamar:
 *        LvglKeypad::pushKey(code, pressed);
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include <cstdint>
#endif
#include <lvgl.h>
#include "../input/KeyCodes.h"

class LvglKeypad {
public:
    // ── API pública ──────────────────────────────────────────────────────

    /** Inicializa el indev LVGL. Llamar después de lv_init(). */
    static void init();

    /**
     * Encola un evento de tecla para que LVGL lo procese en su próximo tick.
     * Llamar desde SystemApp::handleKeyMenu() (o similar) cuando el modo
     * activo es MENU.
     *
     * @param code     KeyCode físico de la tecla
     * @param pressed  true = tecla pulsada, false = tecla liberada
     */
    static void pushKey(KeyCode code, bool pressed);

    /** Devuelve el lv_indev_t creado por init(), útil para asignarlo a grupos */
    static lv_indev_t* indev() { return _indev; }

private:
    // ── Implementación ───────────────────────────────────────────────────

    /** Callback que LVGL invoca en cada tick para leer el estado del teclado */
    static void readCb(lv_indev_t* indev, lv_indev_data_t* data);

    /** Traduce un KeyCode a la constante LV_KEY_xxx correspondiente */
    static uint32_t toLvKey(KeyCode code);

    // ── Estado interno ───────────────────────────────────────────────────

    static lv_indev_t* _indev;

    /** Ring buffer de eventos pendientes (tamaño potencia de 2) */
    static constexpr uint8_t QUEUE_SIZE = 8;

    struct KeyEvent {
        uint32_t lvKey;
        lv_indev_state_t state;
    };

    static KeyEvent _queue[QUEUE_SIZE];
    static volatile uint8_t _head;   // Índice de escritura (productor)
    static volatile uint8_t _tail;   // Índice de lectura  (consumidor)

    /** Clave y estado que LVGL leyó la última vez */
    static uint32_t         _currentKey;
    static lv_indev_state_t _currentState;
};
