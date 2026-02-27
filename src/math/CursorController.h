/**
 * CursorController.h — Motor de Cursor e Inserción Contextual V.P.A.M.
 *
 * Fase 2 del Motor Matemático.
 *
 * El cursor NO es un índice de String ni coordenadas de pantalla.
 * Es un puntero estructural: {NodeRow* row, int index} que indica
 * la posición ENTRE nodos dentro de una fila del AST.
 *
 *   Ejemplo: Row [ Number"2", Operator"+", Number"3" ]
 *            Posiciones:  ^0       ^1          ^2      ^3
 *   index=0 → antes del "2"
 *   index=2 → entre "+" y "3"
 *   index=3 → después del "3"
 *
 * Inserción VPAM:
 *   · insertFraction(): Si hay un Number/Paren a la izquierda, CAPTURA
 *     ese nodo y lo mueve al numerador. Si no, crea fracción vacía.
 *   · insertPower(): Captura el nodo izquierdo como base, cursor salta
 *     automáticamente al exponente (NodeEmpty).
 *   · insertRoot(): Crea √ con radicando vacío, cursor entra dentro.
 *
 * Navegación inteligente:
 *   · moveRight al final del numerador → salta al inicio del denominador
 *   · moveRight al final del denominador → sale de la fracción
 *   · moveRight al final de toda la expresión → wrap al inicio
 *   · moveLeft al inicio de toda la expresión → wrap al final
 *
 * Borrado (backspace):
 *   · Sobre un NodeEmpty: no hace nada (o sube al padre)
 *   · Sobre un Number: borra último dígito; si queda vacío, borra el nodo
 *   · Sobre un Fraction/Power/Root: deshace la estructura, extrae su
 *     contenido a la fila padre (flatten)
 *
 * Dependencias: MathAST.h (Fase 1), C++ estándar.
 */

#pragma once

#include "MathAST.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Cursor — Posición estructural en el AST
// ════════════════════════════════════════════════════════════════════════════
struct Cursor {
    NodeRow* row   = nullptr;   ///< Fila donde está el cursor
    int      index = 0;         ///< Posición entre nodos [0..row->childCount()]

    bool isValid() const { return row != nullptr; }

    /// ¿Está al final de la fila?
    bool atEnd() const { return row && index >= row->childCount(); }

    /// ¿Está al inicio de la fila?
    bool atStart() const { return index <= 0; }

    /// Nodo inmediatamente a la IZQUIERDA del cursor (nullptr si index==0)
    MathNode* nodeLeft() const {
        if (!row || index <= 0) return nullptr;
        return row->child(index - 1);
    }

    /// Nodo inmediatamente a la DERECHA del cursor (nullptr si al final)
    MathNode* nodeRight() const {
        if (!row || index >= row->childCount()) return nullptr;
        return row->child(index);
    }
};

// ════════════════════════════════════════════════════════════════════════════
// CursorController — Motor de cursor, inserción y borrado
// ════════════════════════════════════════════════════════════════════════════
class CursorController {
public:
    CursorController();

    /// Inicializa el cursor al inicio de la fila raíz
    void init(NodeRow* rootRow);

    /// Acceso de lectura al cursor
    const Cursor& cursor() const { return _cur; }

    /// Acceso a la fila raíz (la expresión completa)
    NodeRow* rootRow() const { return _root; }

    // ── Inserción de contenido ──────────────────────────────────────────

    /**
     * Inserta un dígito o punto decimal.
     * Si el nodo a la izquierda es un NodeNumber, EXTIENDE ese número.
     * Si no, crea un nuevo NodeNumber.
     */
    void insertDigit(char c);

    /**
     * Inserta un operador binario (+, −, ×).
     * Siempre crea un nuevo NodeOperator en la posición del cursor.
     */
    void insertOperator(OpKind op);

    /**
     * Inserta una fracción con lógica VPAM de captura:
     *  · Si el nodo izquierdo es capturable (Number o Paren), lo mueve
     *    al numerador y crea Empty en el denominador. Cursor → denominador.
     *  · Si no hay nada capturable, crea fracción con dos Empty.
     *    Cursor → numerador.
     */
    void insertFraction();

    /**
     * Inserta una potencia con lógica VPAM:
     *  · Si hay un nodo capturable a la izquierda, lo usa como base.
     *  · Si no, crea la potencia con base vacía.
     *  · El cursor salta automáticamente al exponente (NodeEmpty).
     */
    void insertPower();

    /**
     * Inserta una raíz cuadrada √.
     *  · Crea NodeRoot con radicando vacío.
     *  · El cursor entra dentro del radicando (NodeEmpty).
     */
    void insertRoot();

    /**
     * Inserta paréntesis.
     *  · Crea NodeParen con contenido vacío.
     *  · El cursor entra dentro.
     */
    void insertParen();

    /**
     * Inserta una función matemática (sin, cos, tan, ln, log, etc.).
     *  · Crea NodeFunction con argumento vacío.
     *  · El cursor entra dentro del argumento.
     */
    void insertFunction(FuncKind kind);

    /**
     * Inserta un logaritmo de base custom: log_n(x).
     *  · Crea NodeLogBase con base y argumento vacíos.
     *  · El cursor entra en la base (subíndice).
     */
    void insertLogBase();

    /**
     * Inserta una constante algebraica (π o e).
     *  · Crea NodeConstant en la posición del cursor.
     */
    void insertConstant(ConstKind kind);

    /**
     * Inserta una variable (x, y, z, A-F, Ans, PreAns).
     *  · Crea NodeVariable en la posición del cursor.
     * @param name  Identificador: 'x','y','z','A'-'F','#' (Ans),'$' (PreAns)
     */
    void insertVariable(char name);

    // ── Navegación ──────────────────────────────────────────────────────

    /** Mueve el cursor a la izquierda (con wrap-around en raíz) */
    void moveLeft();

    /** Mueve el cursor a la derecha (con wrap-around en raíz) */
    void moveRight();

    /**
     * Mueve el cursor arriba.
     *  · Dentro de un denominador → salta al numerador
     *  · Dentro de un exponente  → no hace nada (ya está arriba)
     */
    void moveUp();

    /**
     * Mueve el cursor abajo.
     *  · Dentro de un numerador → salta al denominador
     *  · Dentro del contenido de una raíz → no hace nada
     */
    void moveDown();

    // ── Borrado ─────────────────────────────────────────────────────────

    /**
     * Backspace inteligente.
     *  · Dentro de un NodeNumber: borra último dígito.
     *    Si el número queda vacío, elimina el nodo.
     *  · A la derecha de una estructura (Fraction/Power/Root/Paren):
     *    deshace la estructura → extrae el contenido a la fila padre.
     *  · Sobre un NodeEmpty: sube al padre (sale de la ranura).
     *  · A la derecha de un Operator: elimina el operador.
     */
    void backspace();

private:
    Cursor   _cur;
    NodeRow* _root;

    // ── Helpers de navegación ───────────────────────────────────────────

    /// ¿Es la fila raíz (nivel más exterior)?
    bool isRootRow(const NodeRow* row) const;

    /// Dado un NodeRow hijo, encuentra qué nodo estructural lo contiene
    /// y en qué índice de su fila padre.
    /// Retorna {parentRow, indexInParent} o {nullptr, -1} si es la raíz.
    struct ParentInfo { NodeRow* parentRow; int indexInParent; };
    ParentInfo findParentSlot(const NodeRow* childRow) const;

    /// Entra dentro de un nodo estructural desde la izquierda
    /// (mueve el cursor al inicio de su primera ranura hija).
    /// Si el nodo no tiene ranuras, no hace nada.
    bool enterNodeFromLeft(MathNode* node);

    /// Entra dentro de un nodo estructural desde la derecha
    /// (mueve el cursor al final de su última ranura hija).
    bool enterNodeFromRight(MathNode* node);

    /// Sale del row actual hacia la derecha del nodo padre en su
    /// fila contenedora. Retorna false si ya estamos en la raíz.
    bool exitRowRight();

    /// Sale del row actual hacia la izquierda del nodo padre en su
    /// fila contenedora. Retorna false si ya estamos en la raíz.
    bool exitRowLeft();

    /// ¿Es un nodo "capturable" para VPAM? (Number, Paren, Fraction, Power, Root)
    static bool isCapturable(const MathNode* node);

    /// Busca la primera ranura (NodeRow) dentro de un nodo estructural.
    /// Para Fraction → numerator, para Power → exponent, etc.
    static NodeRow* firstSlot(MathNode* node);

    /// Busca la última ranura (NodeRow) dentro de un nodo estructural.
    static NodeRow* lastSlot(MathNode* node);

    /// Obtiene la ranura "hermana" para navegación vertical.
    /// Para Fraction: numerator ↔ denominator.
    /// Retorna nullptr si no hay ranura vertical disponible.
    static NodeRow* verticalSibling(NodeRow* currentSlot, bool goUp);

    /// Reemplaza un NodeEmpty solitario en la fila actual por contenido real
    void replaceEmptyIfNeeded();

    /// Si la fila queda vacía tras un borrado, inserta un NodeEmpty
    void ensureNotEmpty(NodeRow* row);
};

} // namespace vpam
