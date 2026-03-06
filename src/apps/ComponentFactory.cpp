/**
 * ComponentFactory.cpp — Central hub to spawn any component from the library.
 *
 * Factory Pattern implementation for 30+ component types.
 * Components are categorized by mathematical behavior:
 *   - Discrete Passives (R-based): Resistor, LDR, Thermistor, FSR, Flex
 *   - Semiconductors: BJT (NPN/PNP), MOSFET, Op-Amp
 *   - Logic Gates: AND, OR, NOT, NAND, XOR (digital sublayer)
 *   - Power: Batteries (1.5V, 3V, 9V)
 *   - Outputs: LED, Buzzer, 7-Segment
 *
 * Part of: NumOS — Circuit Core Module
 */

#include "ComponentFactory.h"
#include "LogicGates.h"
#include "PowerSystems.h"

// ══ Factory: Create Component by Type ═══════════════════════════════════════

CircuitComponent* ComponentFactory::create(CompType type, int gridX, int gridY) {
    switch (type) {
        // ── Original passives ──
        case CompType::WIRE:            return new WireComponent(gridX, gridY);
        case CompType::RESISTOR:        return new Resistor(gridX, gridY, 1000.0f);
        case CompType::VOLTAGE_SOURCE:  return new VoltageSource(gridX, gridY, 5.0f);
        case CompType::LED:             return new LEDComponent(gridX, gridY, 2.0f);
        case CompType::GROUND:          return new GroundNode(gridX, gridY);
        case CompType::MCU:             return new MCUComponent(gridX, gridY);
        case CompType::POTENTIOMETER:   return new Potentiometer(gridX, gridY, 10000.0f);
        case CompType::PUSH_BUTTON:     return new PushButton(gridX, gridY);
        case CompType::CAPACITOR:       return new Capacitor(gridX, gridY, 100e-6f);
        case CompType::DIODE:           return new Diode(gridX, gridY, 0.7f);

        // ── Sensors (Variable Resistance) ──
        case CompType::LDR:             return new LDR(gridX, gridY);
        case CompType::THERMISTOR:      return new Thermistor(gridX, gridY);
        case CompType::FLEX_SENSOR:     return new FlexSensor(gridX, gridY);
        case CompType::FSR:             return new FSRComponent(gridX, gridY);

        // ── Semiconductors ──
        case CompType::NPN_BJT:         return new NpnBjt(gridX, gridY);
        case CompType::PNP_BJT:         return new PnpBjt(gridX, gridY);
        case CompType::NMOS:            return new NmosFet(gridX, gridY);
        case CompType::OP_AMP:          return new OpAmp(gridX, gridY);

        // ── Logic Gates ──
        case CompType::AND_GATE:        return new AndGate(gridX, gridY);
        case CompType::OR_GATE:         return new OrGate(gridX, gridY);
        case CompType::NOT_GATE:        return new NotGate(gridX, gridY);
        case CompType::NAND_GATE:       return new NandGate(gridX, gridY);
        case CompType::XOR_GATE:        return new XorGate(gridX, gridY);

        // ── Power ──
        case CompType::BATTERY_AA:      return new BatteryAA(gridX, gridY);
        case CompType::BATTERY_COIN:    return new BatteryCoin(gridX, gridY);
        case CompType::BATTERY_9V:      return new Battery9V(gridX, gridY);

        // ── Outputs ──
        case CompType::BUZZER:          return new BuzzerComponent(gridX, gridY);
        case CompType::SEVEN_SEG:       return new SevenSegment(gridX, gridY);

        default: return nullptr;
    }
}

// ══ Labels ══════════════════════════════════════════════════════════════════

const char* ComponentFactory::label(CompType type) {
    switch (type) {
        case CompType::WIRE:            return "WIRE";
        case CompType::RESISTOR:        return "RES";
        case CompType::VOLTAGE_SOURCE:  return "VCC";
        case CompType::LED:             return "LED";
        case CompType::GROUND:          return "GND";
        case CompType::MCU:             return "MCU";
        case CompType::POTENTIOMETER:   return "POT";
        case CompType::PUSH_BUTTON:     return "BTN";
        case CompType::CAPACITOR:       return "CAP";
        case CompType::DIODE:           return "DIO";
        case CompType::LDR:             return "LDR";
        case CompType::THERMISTOR:      return "TMP";
        case CompType::FLEX_SENSOR:     return "FLX";
        case CompType::FSR:             return "FSR";
        case CompType::NPN_BJT:         return "NPN";
        case CompType::PNP_BJT:         return "PNP";
        case CompType::NMOS:            return "MOS";
        case CompType::OP_AMP:          return "OPA";
        case CompType::AND_GATE:        return "AND";
        case CompType::OR_GATE:         return "OR";
        case CompType::NOT_GATE:        return "NOT";
        case CompType::NAND_GATE:       return "NAN";
        case CompType::XOR_GATE:        return "XOR";
        case CompType::BATTERY_AA:      return "1V5";
        case CompType::BATTERY_COIN:    return "3V";
        case CompType::BATTERY_9V:      return "9V";
        case CompType::BUZZER:          return "BUZ";
        case CompType::SEVEN_SEG:       return "7SG";
        default:                        return "???";
    }
}

const char* ComponentFactory::tooltip(CompType type) {
    switch (type) {
        case CompType::WIRE:            return "WIRE - Connect two nodes";
        case CompType::RESISTOR:        return "RESISTOR - 1k Ohm default";
        case CompType::VOLTAGE_SOURCE:  return "VCC - 5V DC supply";
        case CompType::LED:             return "LED - Light-emitting diode";
        case CompType::GROUND:          return "GROUND - 0V reference";
        case CompType::MCU:             return "MCU - Lua microcontroller";
        case CompType::POTENTIOMETER:   return "POT - Adjustable resistor";
        case CompType::PUSH_BUTTON:     return "BUTTON - Toggle with F4";
        case CompType::CAPACITOR:       return "CAPACITOR - RC transient";
        case CompType::DIODE:           return "DIODE - Rectifier 0.7V";
        case CompType::LDR:             return "LDR - Light sensor (slider)";
        case CompType::THERMISTOR:      return "TMP36 - Temp sensor (slider)";
        case CompType::FLEX_SENSOR:     return "FLEX - Bend sensor (slider)";
        case CompType::FSR:             return "FSR - Force sensor (slider)";
        case CompType::NPN_BJT:         return "NPN BJT - Transistor (B/E/C)";
        case CompType::PNP_BJT:         return "PNP BJT - Transistor (B/E/C)";
        case CompType::NMOS:            return "N-MOSFET - Gate/Source/Drain";
        case CompType::OP_AMP:          return "OP-AMP uA741 - V+/V-/Out";
        case CompType::AND_GATE:        return "AND Gate (74HC08)";
        case CompType::OR_GATE:         return "OR Gate (74HC32)";
        case CompType::NOT_GATE:        return "NOT Gate (74HC04)";
        case CompType::NAND_GATE:       return "NAND Gate (74HC00)";
        case CompType::XOR_GATE:        return "XOR Gate (74HC86)";
        case CompType::BATTERY_AA:      return "BATTERY AA - 1.5V";
        case CompType::BATTERY_COIN:    return "BATTERY CR2032 - 3V";
        case CompType::BATTERY_9V:      return "BATTERY 9V";
        case CompType::BUZZER:          return "BUZZER - Piezo buzzer";
        case CompType::SEVEN_SEG:       return "7-SEG - 7-segment display";
        default:                        return "Unknown component";
    }
}

bool ComponentFactory::needsVsIndex(CompType type) {
    switch (type) {
        case CompType::VOLTAGE_SOURCE:
        case CompType::LED:
        case CompType::OP_AMP:
        case CompType::BATTERY_AA:
        case CompType::BATTERY_COIN:
        case CompType::BATTERY_9V:
        case CompType::AND_GATE:
        case CompType::OR_GATE:
        case CompType::NOT_GATE:
        case CompType::NAND_GATE:
        case CompType::XOR_GATE:
            return true;
        default:
            return false;
    }
}

int ComponentFactory::extraNodes(CompType type) {
    switch (type) {
        case CompType::NPN_BJT:
        case CompType::PNP_BJT:
        case CompType::NMOS:
        case CompType::OP_AMP:
            return 1;  // 3rd terminal node
        default:
            return 0;
    }
}

bool ComponentFactory::isWirePhase(CompType type) {
    return type == CompType::WIRE || type == CompType::GROUND;
}

bool ComponentFactory::isLogicGate(CompType type) {
    switch (type) {
        case CompType::AND_GATE:
        case CompType::OR_GATE:
        case CompType::NOT_GATE:
        case CompType::NAND_GATE:
        case CompType::XOR_GATE:
            return true;
        default:
            return false;
    }
}
