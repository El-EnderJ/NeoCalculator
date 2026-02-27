/**
 * LvglKeypad.cpp
 * Implementación del driver de teclado para LVGL 9.x
 */

#include "LvglKeypad.h"

// ── Definición de estáticos ─────────────────────────────────────────────────
lv_indev_t*             LvglKeypad::_indev        = nullptr;
LvglKeypad::KeyEvent    LvglKeypad::_queue[LvglKeypad::QUEUE_SIZE] = {};
volatile uint8_t        LvglKeypad::_head          = 0;
volatile uint8_t        LvglKeypad::_tail          = 0;
uint32_t                LvglKeypad::_currentKey    = 0;
lv_indev_state_t        LvglKeypad::_currentState  = LV_INDEV_STATE_RELEASED;

// ── init() ──────────────────────────────────────────────────────────────────
void LvglKeypad::init() {
    _indev = lv_indev_create();
    lv_indev_set_type(_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(_indev, readCb);
}

// ── pushKey() ───────────────────────────────────────────────────────────────
void LvglKeypad::pushKey(KeyCode code, bool pressed) {
    uint32_t lk = toLvKey(code);
    if (lk == 0) return; // Tecla sin mapping LVGL → ignorar

    uint8_t nextHead = ((_head + 1) & (QUEUE_SIZE - 1));
    if (nextHead == _tail) return; // Buffer lleno → descartamos (tecla perdida)

    _queue[_head].lvKey = lk;
    _queue[_head].state = pressed ? LV_INDEV_STATE_PRESSED
                                  : LV_INDEV_STATE_RELEASED;
    _head = nextHead;
}

// ── readCb() ────────────────────────────────────────────────────────────────
/**
 * LVGL llama a esta función en cada tick de su timer handler.
 * Devuelve la tecla actual y su estado. Si hay eventos en la cola, los consume
 * uno a uno. Si la cola está vacía, reporta RELEASED con la última tecla.
 */
void LvglKeypad::readCb(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    if (_head != _tail) {
        // Hay al menos un evento nuevo: consúmelo
        _currentKey   = _queue[_tail].lvKey;
        _currentState = _queue[_tail].state;
        _tail = ((_tail + 1) & (QUEUE_SIZE - 1));
    } else {
        // Sin eventos nuevos → tecla liberada (para evitar repetición no deseada)
        _currentState = LV_INDEV_STATE_RELEASED;
    }

    data->key   = _currentKey;
    data->state = _currentState;

    // Si todavía hay más eventos pendientes, LVGL nos llamará de nuevo
    // en el mismo tick hasta que la cola se vacíe.
    data->continue_reading = (_head != _tail);
}

// ── toLvKey() ───────────────────────────────────────────────────────────────
uint32_t LvglKeypad::toLvKey(KeyCode code) {
    switch (code) {
        // Navegación → mapeo directo a LV_KEY de LVGL
        case KeyCode::LEFT:   return LV_KEY_LEFT;
        case KeyCode::RIGHT:  return LV_KEY_RIGHT;
        case KeyCode::UP:     return LV_KEY_UP;
        case KeyCode::DOWN:   return LV_KEY_DOWN;

        // Confirmación / Cancelación
        case KeyCode::ENTER:  return LV_KEY_ENTER;
        case KeyCode::AC:     return LV_KEY_ESC;
        case KeyCode::DEL:    return LV_KEY_BACKSPACE;

        // Función: avanzar al siguiente / anterior elemento del grupo
        case KeyCode::F1:     return LV_KEY_NEXT;
        case KeyCode::F2:     return LV_KEY_PREV;

        // El resto de teclas no tienen mapping LVGL → 0 = ignorar
        default: return 0;
    }
}
