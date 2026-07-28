#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../src/input/KeySemanticResolver.h"
#include "../../src/input/ProductionKeypadScanner.h"

namespace {

using numos::input::InputContext;
using numos::input::KeyPlane;
using numos::input::KeySemanticResolver;
using numos::input::ProductionKeypadScanner;
using numos::input::SemanticId;
using numos::input::kProductionKeypadMap;

int g_failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #condition);                                            \
            ++g_failures;                                                       \
        }                                                                       \
    } while (false)

std::size_t mappingIndex(const KeyCode code) {
    for (std::size_t i = 0; i < kProductionKeypadMap.size(); ++i) {
        if (kProductionKeypadMap[i].keyCode == code) return i;
    }
    return kProductionKeypadMap.size();
}

void scan(ProductionKeypadScanner& scanner,
          const std::array<uint16_t, 5>& rows,
          const uint32_t nowMs) {
    for (uint8_t row = 0; row < rows.size(); ++row) {
        scanner.ingestRow(row, rows[row], nowMs);
    }
}

void settle(ProductionKeypadScanner& scanner,
            const std::array<uint16_t, 5>& rows,
            uint32_t& nowMs) {
    for (uint8_t i = 0; i < scanner.config().debounceIntegratorMax; ++i) {
        scan(scanner, rows, nowMs);
        nowMs += 5;
    }
}

std::size_t drain(ProductionKeypadScanner& scanner,
                  const KeyAction action,
                  std::array<bool, 50>* seen = nullptr) {
    std::size_t count = 0;
    KeyEvent event;
    while (scanner.pollEvent(event)) {
        if (event.action != action) continue;
        ++count;
        if (seen != nullptr && event.row >= 0 && event.col >= 0) {
            (*seen)[static_cast<std::size_t>(event.row) * 10U +
                    static_cast<std::size_t>(event.col)] = true;
        }
    }
    return count;
}

void testMapping() {
    static_assert(static_cast<uint8_t>(KeyCode::GREATER) == 68);
    static_assert(static_cast<uint8_t>(KeyCode::HOME) == 69);
    static_assert(static_cast<uint8_t>(KeyCode::EXP) == 79);
    static_assert(static_cast<uint16_t>(SemanticId::none) == 0);

    std::array<bool, 50> electrical{};
    std::array<bool, 50> visual{};
    std::array<bool, 52> switches{};
    std::array<bool, 256> keyCodes{};
    for (const auto& key : kProductionKeypadMap) {
        const std::size_t electricalIndex =
            static_cast<std::size_t>(key.electricalRow) * 10U +
            key.electricalColumn;
        const std::size_t visualIndex =
            static_cast<std::size_t>(key.visualRow) * 5U + key.visualColumn;
        CHECK(electricalIndex < electrical.size());
        CHECK(visualIndex < visual.size());
        CHECK(!electrical[electricalIndex]);
        CHECK(!visual[visualIndex]);
        electrical[electricalIndex] = true;
        visual[visualIndex] = true;
        CHECK(key.switchNumber >= 2 && key.switchNumber <= 51);
        CHECK(!switches[key.switchNumber]);
        switches[key.switchNumber] = true;
        const auto code = static_cast<uint8_t>(key.keyCode);
        CHECK(key.keyCode != KeyCode::NONE);
        CHECK(!keyCodes[code]);
        keyCodes[code] = true;
        CHECK(key.electricalRow == 4U - key.visualColumn);
        CHECK(key.electricalColumn == key.visualRow);
        CHECK(key.switchNumber ==
              42 - 10 * static_cast<int>(key.visualColumn) +
              static_cast<int>(key.visualRow));
    }
    for (const bool present : electrical) CHECK(present);
    for (const bool present : visual) CHECK(present);
    for (std::size_t sw = 2; sw <= 51; ++sw) CHECK(switches[sw]);
}

void testIndividualWaveforms() {
    ProductionKeypadScanner scanner;
    uint32_t now = 10;
    std::array<uint16_t, 5> rows{};
    scan(scanner, rows, now);
    CHECK(scanner.queuedEventCount() == 0);

    for (std::size_t i = 0; i < kProductionKeypadMap.size(); ++i) {
        scanner.reset();
        rows = {};
        const auto& key = kProductionKeypadMap[i];
        rows[key.electricalRow] =
            static_cast<uint16_t>(1U << key.electricalColumn);
        settle(scanner, rows, now);
        KeyEvent event{};
        CHECK(scanner.pollEvent(event));
        CHECK(event.action == KeyAction::PRESS);
        CHECK(event.code == key.keyCode);
        CHECK(event.row == key.electricalRow);
        CHECK(event.col == key.electricalColumn);
        CHECK(!scanner.pollEvent(event));
        rows = {};
        settle(scanner, rows, now);
        CHECK(scanner.pollEvent(event));
        CHECK(event.action == KeyAction::RELEASE);
        CHECK(event.code == key.keyCode);
        CHECK(!scanner.pollEvent(event));
    }
}

void testChordsAndBounce() {
    ProductionKeypadScanner scanner;
    uint32_t now = 100;
    std::array<uint16_t, 5> rows{};

    // Same row, same column, diagonal, and several simultaneous keys.
    rows[0] = (1U << 0) | (1U << 9);
    rows[1] = (1U << 0);
    rows[3] = (1U << 4);
    settle(scanner, rows, now);
    std::array<bool, 50> seen{};
    CHECK(drain(scanner, KeyAction::PRESS, &seen) == 4);
    CHECK(seen[0] && seen[9] && seen[10] && seen[34]);
    CHECK(scanner.activeColumns(0) == ((1U << 0) | (1U << 9)));

    rows = {};
    settle(scanner, rows, now);
    CHECK(drain(scanner, KeyAction::RELEASE) == 4);

    // Alternating bounce never reaches the independent integrator threshold.
    for (int i = 0; i < 3; ++i) {
        rows[2] = (1U << 5);
        scan(scanner, rows, now++);
        rows[2] = 0;
        scan(scanner, rows, now++);
    }
    CHECK(scanner.queuedEventCount() == 0);
    rows[2] = (1U << 5);
    settle(scanner, rows, now);
    CHECK(drain(scanner, KeyAction::PRESS) == 1);
    for (int i = 0; i < 3; ++i) {
        rows[2] = 0;
        scan(scanner, rows, now++);
        rows[2] = (1U << 5);
        scan(scanner, rows, now++);
    }
    CHECK(scanner.queuedEventCount() == 0);
    rows[2] = 0;
    settle(scanner, rows, now);
    CHECK(drain(scanner, KeyAction::RELEASE) == 1);
}

void testRepeatRecoveryAndWrap() {
    ProductionKeypadScanner scanner;
    const std::size_t up = mappingIndex(KeyCode::UP);
    CHECK(up < kProductionKeypadMap.size());
    const auto& key = kProductionKeypadMap[up];
    std::array<uint16_t, 5> rows{};
    rows[key.electricalRow] = 1U << key.electricalColumn;
    uint32_t now = UINT32_MAX - 20U;
    settle(scanner, rows, now);
    CHECK(drain(scanner, KeyAction::PRESS) == 1);
    const uint32_t pressTime =
        scanner.state(key.electricalRow, key.electricalColumn).lastRepeatMs;
    scanner.ingestRow(key.electricalRow,
                      rows[key.electricalRow],
                      pressTime + 499U);
    CHECK(scanner.queuedEventCount() == 0);
    scanner.ingestRow(key.electricalRow,
                      rows[key.electricalRow],
                      pressTime + 500U);
    CHECK(drain(scanner, KeyAction::REPEAT) == 1);
    scanner.ingestRow(key.electricalRow,
                      rows[key.electricalRow],
                      pressTime + 5000U);
    CHECK(drain(scanner, KeyAction::REPEAT) == 1); // no catch-up burst

    scanner.forceReleaseAll(pressTime + 5001U);
    CHECK(drain(scanner, KeyAction::RELEASE) == 1);
    // A still-held switch is inhibited until an observed release.
    settle(scanner, rows, now);
    CHECK(scanner.queuedEventCount() == 0);
    rows = {};
    scan(scanner, rows, now++);
    rows[key.electricalRow] = 1U << key.electricalColumn;
    settle(scanner, rows, now);
    CHECK(drain(scanner, KeyAction::PRESS) == 1);

    // Non-repeatable modifier remains silent through a long hold.
    scanner.reset();
    rows = {};
    const auto shift = kProductionKeypadMap[mappingIndex(KeyCode::SHIFT)];
    rows[shift.electricalRow] = 1U << shift.electricalColumn;
    now = 1000;
    settle(scanner, rows, now);
    CHECK(drain(scanner, KeyAction::PRESS) == 1);
    scanner.ingestRow(shift.electricalRow,
                      rows[shift.electricalRow], now + 10000U);
    CHECK(scanner.queuedEventCount() == 0);
}

void testOverflowPolicy() {
    ProductionKeypadScanner scanner;
    std::array<uint16_t, 5> rows{};
    rows.fill(0x03FFU);
    uint32_t now = 100;
    settle(scanner, rows, now); // 50 queued PRESS events
    CHECK(scanner.queuedEventCount() == 50);

    // Generate enough repeat traffic to overflow the fixed queue.
    for (int cycle = 0; cycle < 10; ++cycle) {
        now += cycle == 0 ? 500U : 80U;
        scan(scanner, rows, now);
    }
    CHECK(scanner.queuedEventCount() <=
          ProductionKeypadScanner::kQueueCapacity);
    CHECK(scanner.overflowCount() > 0);

    // Release before dispatch collapses undispatched pairs; no fabricated
    // release-without-down may escape the queue.
    rows.fill(0);
    settle(scanner, rows, now);
    std::array<bool, 50> down{};
    KeyEvent event;
    while (scanner.pollEvent(event)) {
        const std::size_t i =
            static_cast<std::size_t>(event.row) * 10U + event.col;
        if (event.action == KeyAction::PRESS) down[i] = true;
        if (event.action == KeyAction::RELEASE) CHECK(down[i]);
    }
}

void testModifiersAndContexts() {
    auto& manager = vpam::KeyboardManager::instance();
    manager.reset();
    manager.pressShift();
    CHECK(manager.shiftPhase() == vpam::ModifierPhase::Once);
    manager.pressShift();
    CHECK(manager.shiftPhase() == vpam::ModifierPhase::Locked);
    manager.pressShift();
    CHECK(manager.shiftPhase() == vpam::ModifierPhase::Off);
    manager.pressAlpha();
    manager.pressAlpha();
    CHECK(manager.alphaPhase() == vpam::ModifierPhase::Locked);
    manager.pressAlpha();
    CHECK(manager.alphaPhase() == vpam::ModifierPhase::Off);

    KeySemanticResolver::resolve(KeyCode::SHIFT, InputContext::Math,
                                 KeyAction::PRESS);
    auto resolved = KeySemanticResolver::resolve(
        KeyCode::SIN, InputContext::Math, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::asin);
    CHECK(resolved.plane == KeyPlane::Shift);
    CHECK(!manager.isShift());

    KeySemanticResolver::resolve(KeyCode::SHIFT, InputContext::Math,
                                 KeyAction::PRESS);
    resolved = KeySemanticResolver::resolve(
        KeyCode::UP, InputContext::Math, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::cursor_page_up);
    CHECK(manager.isShift()); // navigation preserves the armed modifier
    manager.reset();

    KeySemanticResolver::resolve(KeyCode::ALPHA, InputContext::Math,
                                 KeyAction::PRESS);
    resolved = KeySemanticResolver::resolve(
        KeyCode::VAR_X, InputContext::Math, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::alpha_A);
    CHECK(!manager.isAlpha());

    KeySemanticResolver::resolve(KeyCode::ALPHA, InputContext::Code,
                                 KeyAction::PRESS);
    resolved = KeySemanticResolver::resolve(
        KeyCode::VAR_X, InputContext::Code, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::code_a);
    CHECK(std::strcmp(resolved.text, "a") == 0);

    KeySemanticResolver::resolve(KeyCode::SHIFT, InputContext::Code,
                                 KeyAction::PRESS);
    KeySemanticResolver::resolve(KeyCode::ALPHA, InputContext::Code,
                                 KeyAction::PRESS);
    resolved = KeySemanticResolver::resolve(
        KeyCode::VAR_X, InputContext::Code, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::code_A);
    CHECK(resolved.plane == KeyPlane::ShiftAlpha);
    CHECK(std::strcmp(resolved.text, "A") == 0);
    CHECK(!manager.isShift() && !manager.isAlpha());

    resolved = KeySemanticResolver::resolve(
        KeyCode::MUL, InputContext::Code, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::code_multiply);
    CHECK(std::strcmp(resolved.text, "*") == 0);
    resolved = KeySemanticResolver::resolve(
        KeyCode::DIVIDE, InputContext::Code, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::code_divide);
    CHECK(std::strcmp(resolved.text, "/") == 0);
    resolved = KeySemanticResolver::resolve(
        KeyCode::POW, InputContext::Code, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::code_power_caret);
    CHECK(std::strcmp(resolved.text, "^") == 0);

    KeySemanticResolver::resolve(KeyCode::SHIFT, InputContext::Code,
                                 KeyAction::PRESS);
    resolved = KeySemanticResolver::resolve(
        KeyCode::EQUAL, InputContext::Code, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::code_not_equal);
    CHECK(std::strcmp(resolved.text, "!=") == 0);

    manager.reset();
    KeySemanticResolver::resolve(KeyCode::SHIFT, InputContext::Math,
                                 KeyAction::PRESS);
    resolved = KeySemanticResolver::resolve(
        KeyCode::AC, InputContext::Math, KeyAction::PRESS);
    CHECK(resolved.semantic == SemanticId::deep_sleep_off);
    CHECK(resolved.code == KeyCode::NONE);
    CHECK(!manager.isShift());

    manager.reset();
    manager.pressShift();
    manager.pressAlpha();
    KeySemanticResolver::reset(); // recovery/context reset
    CHECK(!manager.isShift() && !manager.isAlpha());
}

} // namespace

int main() {
    testMapping();
    testIndividualWaveforms();
    testChordsAndBounce();
    testRepeatRecoveryAndWrap();
    testOverflowPolicy();
    testModifiersAndContexts();
    if (g_failures != 0) {
        std::fprintf(stderr, "production keypad suite: %d failure(s)\n",
                     g_failures);
        return 1;
    }
    std::printf(
        "production keypad suite: PASS (50 mappings, scanner=%zu bytes, "
        "event=%zu bytes, queue=%zu)\n",
        sizeof(ProductionKeypadScanner), sizeof(KeyEvent),
        ProductionKeypadScanner::kQueueCapacity);
    return 0;
}
