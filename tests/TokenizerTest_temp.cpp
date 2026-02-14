/**
 * TokenizerTest_temp.cpp
 *
 * Archivo temporal de pruebas para el Tokenizer.
 * No se compila en el firmware principal; está pensado para ejecutarse
 * en un entorno de pruebas (por ejemplo, adaptándolo a PlatformIO test/Unity)
 * o copiando fragmentos a un sketch de prueba.
 */

#include <Arduino.h>
#include "math/Tokenizer.h"

static void printTokens(const TokenizeResult &res) {
    Serial.println(F("Tokens:"));
    for (size_t i = 0; i < res.tokens.size(); ++i) {
        const Token &t = res.tokens[i];
        Serial.print("#");
        Serial.print(i);
        Serial.print(" type=");
        Serial.print(static_cast<int>(t.type));
        Serial.print(" text='");
        Serial.print(t.text);
        Serial.print("' value=");
        Serial.println(t.value, 10);
    }
}

// Prueba principal: 2(x+3)^2
static void test_Tokenizer_ImplicitMul_And_Power() {
    Tokenizer tz;
    String expr = "2(x+3)^2";
    TokenizeResult res = tz.tokenize(expr);

    Serial.println(F("=== Test Tokenizer: 2(x+3)^2 ==="));
    if (!res.ok) {
        Serial.print(F("Error: "));
        Serial.println(res.errorMessage);
        return;
    }

    printTokens(res);

    // Comprobación mínima de la secuencia esperada:
    // 2 * ( x + 3 ) ^ 2
    if (res.tokens.size() < 9) {
        Serial.println(F("Fallo: número de tokens inesperado"));
        return;
    }

    bool ok = true;
    ok &= res.tokens[0].type == TokenType::Number && res.tokens[0].text == "2";
    ok &= res.tokens[1].type == TokenType::Operator && res.tokens[1].text == "*";
    ok &= res.tokens[2].type == TokenType::LParen;
    ok &= res.tokens[3].type == TokenType::Identifier && res.tokens[3].text == "x";
    ok &= res.tokens[4].type == TokenType::Operator && res.tokens[4].text == "+";
    ok &= res.tokens[5].type == TokenType::Number && res.tokens[5].text == "3";
    ok &= res.tokens[6].type == TokenType::RParen;
    ok &= res.tokens[7].type == TokenType::Operator && res.tokens[7].text == "^";
    ok &= res.tokens[8].type == TokenType::Number && res.tokens[8].text == "2";

    if (ok) {
        Serial.println(F("OK: tokenización de 2(x+3)^2 correcta (incluyendo multiplicación implícita)."));
    } else {
        Serial.println(F("Fallo: la secuencia de tokens no coincide con la esperada."));
    }
}

// Función de entrada sugerida para pruebas manuales:
void runTokenizerTempTests() {
    Serial.begin(115200);
    delay(200);
    test_Tokenizer_ImplicitMul_And_Power();
}

