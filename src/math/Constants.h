/**
 * Constants.h — Constantes Matemáticas de Alta Precisión (PROGMEM)
 *
 * Contiene los primeros 1000 dígitos decimales de π y e,
 * almacenados en flash (PROGMEM) para no consumir RAM.
 *
 * Uso:
 *   - El evaluador extrae dígitos bajo demanda vía getConstantDigits()
 *   - El modo Extendido (Estado 3 S⇔D) usa estos dígitos para scroll
 *   - La División Larga Procedural genera dígitos para racionales
 *
 * Fuentes verificadas:
 *   π: OEIS A000796
 *   e: OEIS A001113
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#ifdef ARDUINO
#include <pgmspace.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif
#endif

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// π = 3.14159265358979323846...  (1000 dígitos tras el punto)
// ════════════════════════════════════════════════════════════════════════════
static const char PI_INT[] PROGMEM = "3";
static const char PI_DEC[] PROGMEM =
    "14159265358979323846264338327950288419716939937510"  // 50
    "58209749445923078164062862089986280348253421170679"  // 100
    "82148086513282306647093844609550582231725359408128"  // 150
    "48111745028410270193852110555964462294895493038196"  // 200
    "44288109756659334461284756482337867831652712019091"  // 250
    "45648566923460348610454326648213393607260249141273"  // 300
    "72458700660631558817488152092096282925409171536436"  // 350
    "78925903600113305305488204665213841469519415116094"  // 400
    "33057270365759591953092186117381932611793105118548"  // 450
    "07446237996274956735188575272489122793818301194912"  // 500
    "98336733624406566430860213949463952247371907021798"  // 550
    "60943702770539217176293176752384674818467669405132"  // 600
    "00056812714526356082778577134275778960917363717872"  // 650
    "14684409012249534301465495853710507922796892589235"  // 700
    "42019956112129021960864034418159813629774771309960"  // 750
    "51870721134999999837297804995105973173281609631859"  // 800
    "50244594553469083026425223082533446850352619311881"  // 850
    "71010003137838752886587533208381420617177669147303"  // 900
    "59825349042875546873115956286388235378759375195778"  // 950
    "18577805321712268066130019278766111959092164201989"; // 1000

// ════════════════════════════════════════════════════════════════════════════
// e = 2.71828182845904523536...  (1000 dígitos tras el punto)
// ════════════════════════════════════════════════════════════════════════════
static const char E_INT[] PROGMEM = "2";
static const char E_DEC[] PROGMEM =
    "71828182845904523536028747135266249775724709369995"  // 50
    "95749669676277240766303535475945713821785251664274"  // 100
    "27466391932003059921817413596629043572900334295260"  // 150
    "59563073813232862794349076323382988075319525101901"  // 200
    "15738341879307021540891499348841675092447614606680"  // 250
    "82264800168477411853742345442437107539077744992069"  // 300
    "55170276183860626133138458300075204493382656029760"  // 350
    "67371132007093287091274437470472306969772093101416"  // 400
    "92836819025515108657463772111252389784425056953696"  // 450
    "77078544996996794686445490598793163688923009879312"  // 500
    "77361782154249992295763514822082698951936680331825"  // 550
    "28869398496465105820939239829488793320362509443117"  // 600
    "30123819706841614039701983767932068328237646480429"  // 650
    "53118023287825098194558153017567677700353751620851"  // 700
    "41521209816731405888946767827800956699791413472837"  // 750
    "89971014286866945638995926993698176050738564142718"  // 800
    "08628228821936569463689606291388828560566293014105"  // 850
    "39753515361594744063219991798174307074785648066236"  // 900
    "07895365437952487696510924523791529468527559281120"  // 950
    "23453464240519912804681898609038095298024209592488"; // 1000

// ════════════════════════════════════════════════════════════════════════════
// API de acceso a dígitos
// ════════════════════════════════════════════════════════════════════════════

/**
 * Extrae dígitos de una constante almacenada en PROGMEM.
 *
 * @param intPart  Parte entera en PROGMEM ("3" para π, "2" para e)
 * @param decPart  Dígitos decimales en PROGMEM (hasta 1000)
 * @param maxDec   Número máximo de dígitos decimales a extraer
 * @param negative Si es negativo (prefijo '-')
 * @return         String con el número formateado: "3.14159..."
 */
inline std::string getConstantDigits(const char* intPart, const char* decPart,
                                     int maxDec, bool negative = false) {
    std::string result;
    if (negative) result += '-';

    // Leer parte entera desde PROGMEM
    const char* p = intPart;
    while (pgm_read_byte(p)) {
        result += static_cast<char>(pgm_read_byte(p));
        p++;
    }

    if (maxDec <= 0) return result;

    result += '.';

    // Leer dígitos decimales desde PROGMEM
    p = decPart;
    int count = 0;
    while (count < maxDec && pgm_read_byte(p)) {
        result += static_cast<char>(pgm_read_byte(p));
        p++;
        count++;
    }

    return result;
}

/**
 * Obtiene dígitos de π hasta un máximo dado.
 * @param maxDecimals  Número de dígitos decimales (1-1000)
 * @param negative     Prefijo negativo
 */
inline std::string getPiDigits(int maxDecimals, bool negative = false) {
    return getConstantDigits(PI_INT, PI_DEC, maxDecimals, negative);
}

/**
 * Obtiene dígitos de e hasta un máximo dado.
 * @param maxDecimals  Número de dígitos decimales (1-1000)
 * @param negative     Prefijo negativo
 */
inline std::string getEDigits(int maxDecimals, bool negative = false) {
    return getConstantDigits(E_INT, E_DEC, maxDecimals, negative);
}

// ════════════════════════════════════════════════════════════════════════════
// División Larga Procedural — Genera dígitos para racionales sin double
//
// Usa el algoritmo clásico: residuo = residuo * 10; dígito = residuo / den
// Detecta el período (ciclo del residuo) para el modo periódico.
// ════════════════════════════════════════════════════════════════════════════

/**
 * Resultado de la división larga con detección de período.
 */
struct LongDivResult {
    std::string intPart;      ///< Parte entera (sin signo)
    std::string nonRepeat;    ///< Dígitos no periódicos
    std::string repeat;       ///< Patrón periódico (vacío si terminante)
    bool        negative;     ///< ¿Resultado negativo?
    bool        terminating;  ///< ¿Decimal terminante (finito)?
};

/**
 * Realiza división larga de num/den, generando hasta maxDigits decimales.
 * Detecta automáticamente el período de repetición.
 *
 * @param num       Numerador (puede ser negativo)
 * @param den       Denominador (debe ser != 0, positivo tras normalize)
 * @param maxDigits Número máximo de dígitos decimales a generar
 * @return          Estructura con parte entera, no periódica y periódica
 */
inline LongDivResult longDivision(int64_t num, int64_t den, int maxDigits = 500) {
    LongDivResult r;
    r.negative    = false;
    r.terminating = false;

    if (den == 0) {
        r.intPart = "0";
        r.terminating = true;
        return r;
    }

    // Normalizar signos
    if ((num < 0) != (den < 0)) r.negative = true;
    if (num < 0) num = -num;
    if (den < 0) den = -den;

    // Parte entera
    r.intPart = std::to_string(num / den);
    int64_t remainder = num % den;

    if (remainder == 0) {
        r.terminating = true;
        return r;
    }

    // Generar dígitos decimales y detectar período
    std::string digits;
    // Mapa: residuo → posición donde apareció por primera vez
    // Usamos un array simple para residuos < den (máx ~1000 iteraciones)
    // Para evitar overhead de std::unordered_map en ESP32
    struct RemPos { int64_t rem; int pos; };
    std::vector<RemPos> seen;
    seen.reserve(maxDigits);

    int pos = 0;
    while (remainder != 0 && pos < maxDigits) {
        // ¿Ya vimos este residuo?
        for (const auto& s : seen) {
            if (s.rem == remainder) {
                // ¡Período encontrado!
                r.nonRepeat = digits.substr(0, s.pos);
                r.repeat    = digits.substr(s.pos);
                return r;
            }
        }

        seen.push_back({remainder, pos});
        remainder *= 10;
        int digit = static_cast<int>(remainder / den);
        digits += static_cast<char>('0' + digit);
        remainder %= den;
        pos++;
    }

    if (remainder == 0) {
        r.nonRepeat   = digits;
        r.terminating = true;
    } else {
        // No se encontró período dentro de maxDigits
        r.nonRepeat = digits;
    }

    return r;
}

/**
 * Genera una cadena decimal extendida (para scroll) de un racional.
 * No detecta período; simplemente genera dígitos paso a paso.
 *
 * @param num       Numerador (positivo)
 * @param den       Denominador (positivo, != 0)
 * @param maxDigits Cantidad de dígitos decimales a generar
 * @param negative  ¿Prefijo negativo?
 * @return          String: "-1.333333..."
 */
inline std::string longDivisionExtended(int64_t num, int64_t den,
                                        int maxDigits, bool negative = false) {
    if (den == 0) return "0";
    if (num < 0) num = -num;
    if (den < 0) den = -den;

    std::string result;
    if (negative) result += '-';

    result += std::to_string(num / den);
    int64_t remainder = num % den;

    if (remainder == 0 || maxDigits <= 0) return result;

    result += '.';
    for (int i = 0; i < maxDigits && remainder != 0; ++i) {
        remainder *= 10;
        int digit = static_cast<int>(remainder / den);
        result += static_cast<char>('0' + digit);
        remainder %= den;
    }

    return result;
}

} // namespace vpam
