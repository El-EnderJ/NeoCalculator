/**
 * CursorController.cpp — Implementación del Motor de Cursor V.P.A.M.
 *
 * Fase 2: Inserción contextual, navegación inteligente y borrado.
 *
 * Principios:
 *   · El cursor SIEMPRE apunta a un {NodeRow*, index}. Nunca a coordenadas.
 *   · Las inserciones VPAM "capturan" el nodo a la izquierda del cursor
 *     cuando es un Number/Paren/estructura, emulando el comportamiento
 *     de las Casio ClassWiz / NumWorks.
 *   · La navegación sigue la estructura del árbol: al salir de una ranura
 *     hija, el cursor reaparece en la fila padre a la posición correcta.
 *   · Wrap-around: derecha al final → inicio, izquierda al inicio → final
 *     (solo en la fila raíz).
 */

#include "CursorController.h"
#include <algorithm>
#include <cassert>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Constructor e inicialización
// ════════════════════════════════════════════════════════════════════════════
CursorController::CursorController()
    : _root(nullptr)
{
    _cur.row   = nullptr;
    _cur.index = 0;
}

void CursorController::init(NodeRow* rootRow) {
    _root      = rootRow;
    _cur.row   = rootRow;
    _cur.index = 0;
}

// ════════════════════════════════════════════════════════════════════════════
// Helpers internos
// ════════════════════════════════════════════════════════════════════════════

bool CursorController::isRootRow(const NodeRow* row) const {
    return row == _root;
}

CursorController::ParentInfo
CursorController::findParentSlot(const NodeRow* childRow) const {
    if (!childRow || isRootRow(childRow)) return { nullptr, -1 };

    // El padre del NodeRow es un nodo estructural (Fraction, Power, Root, Paren).
    MathNode* structNode = childRow->parent();
    if (!structNode) return { nullptr, -1 };

    // El padre del nodo estructural debería ser un NodeRow (la fila contenedora).
    MathNode* grandparent = structNode->parent();
    if (!grandparent || grandparent->type() != NodeType::Row) return { nullptr, -1 };

    auto* parentRow = static_cast<NodeRow*>(grandparent);

    // Buscar el índice del nodo estructural en la fila padre
    for (int i = 0; i < parentRow->childCount(); ++i) {
        if (parentRow->child(i) == structNode) {
            return { parentRow, i };
        }
    }
    return { nullptr, -1 };
}

bool CursorController::isCapturable(const MathNode* node) {
    if (!node) return false;
    switch (node->type()) {
        case NodeType::Number:
        case NodeType::Paren:
        case NodeType::Fraction:
        case NodeType::Power:
        case NodeType::Root:
        case NodeType::Function:
        case NodeType::LogBase:
        case NodeType::Constant:
        case NodeType::Variable:
            return true;
        default:
            return false;
    }
}

NodeRow* CursorController::firstSlot(MathNode* node) {
    if (!node) return nullptr;
    switch (node->type()) {
        case NodeType::Fraction: {
            auto* f = static_cast<NodeFraction*>(node);
            auto* num = f->numerator();
            return (num && num->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(num)
                       : nullptr;
        }
        case NodeType::Power: {
            // Para Power, al entrar queremos ir al EXPONENTE (superíndice)
            // porque la base ya se capturó. Pero firstSlot debería ser la base.
            auto* p = static_cast<NodePower*>(node);
            auto* base = p->base();
            return (base && base->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(base)
                       : nullptr;
        }
        case NodeType::Root: {
            auto* r = static_cast<NodeRoot*>(node);
            auto* rad = r->radicand();
            return (rad && rad->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(rad)
                       : nullptr;
        }
        case NodeType::Paren: {
            auto* par = static_cast<NodeParen*>(node);
            auto* content = par->content();
            return (content && content->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(content)
                       : nullptr;
        }
        case NodeType::Function: {
            auto* func = static_cast<NodeFunction*>(node);
            auto* arg = func->argument();
            return (arg && arg->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(arg)
                       : nullptr;
        }
        case NodeType::LogBase: {
            // Para LogBase, la primera ranura es la base (subíndice)
            auto* lb = static_cast<NodeLogBase*>(node);
            auto* base = lb->base();
            return (base && base->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(base)
                       : nullptr;
        }
        default:
            return nullptr;
    }
}

NodeRow* CursorController::lastSlot(MathNode* node) {
    if (!node) return nullptr;
    switch (node->type()) {
        case NodeType::Fraction: {
            auto* f = static_cast<NodeFraction*>(node);
            auto* den = f->denominator();
            return (den && den->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(den)
                       : nullptr;
        }
        case NodeType::Power: {
            auto* p = static_cast<NodePower*>(node);
            auto* exp = p->exponent();
            return (exp && exp->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(exp)
                       : nullptr;
        }
        case NodeType::Root: {
            auto* r = static_cast<NodeRoot*>(node);
            // Si tiene grado, la última ranura es el radicando.
            // (Grado es la ranura "extra", radicando la principal).
            auto* rad = r->radicand();
            return (rad && rad->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(rad)
                       : nullptr;
        }
        case NodeType::Paren: {
            auto* par = static_cast<NodeParen*>(node);
            auto* content = par->content();
            return (content && content->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(content)
                       : nullptr;
        }
        case NodeType::Function: {
            auto* func = static_cast<NodeFunction*>(node);
            auto* arg = func->argument();
            return (arg && arg->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(arg)
                       : nullptr;
        }
        case NodeType::LogBase: {
            // Para LogBase, la última ranura es el argumento
            auto* lb = static_cast<NodeLogBase*>(node);
            auto* arg = lb->argument();
            return (arg && arg->type() == NodeType::Row)
                       ? static_cast<NodeRow*>(arg)
                       : nullptr;
        }
        default:
            return nullptr;
    }
}

NodeRow* CursorController::verticalSibling(NodeRow* currentSlot, bool goUp) {
    if (!currentSlot) return nullptr;

    MathNode* structNode = currentSlot->parent();
    if (!structNode) return nullptr;

    if (structNode->type() == NodeType::Fraction) {
        auto* f = static_cast<NodeFraction*>(structNode);
        if (goUp) {
            // Si estamos en el denominador → subir al numerador
            if (currentSlot == static_cast<NodeRow*>(f->denominator())) {
                auto* num = f->numerator();
                return (num && num->type() == NodeType::Row)
                           ? static_cast<NodeRow*>(num)
                           : nullptr;
            }
        } else {
            // Si estamos en el numerador → bajar al denominador
            if (currentSlot == static_cast<NodeRow*>(f->numerator())) {
                auto* den = f->denominator();
                return (den && den->type() == NodeType::Row)
                           ? static_cast<NodeRow*>(den)
                           : nullptr;
            }
        }
    }

    // Power: Arriba = exponente (ya estamos ahí si estamos en exp)
    // No hay "bajar" desde la base al exponente en sentido vertical clásico
    // pero podemos permitir: base ↑ → exponent
    if (structNode->type() == NodeType::Power) {
        auto* p = static_cast<NodePower*>(structNode);
        if (goUp) {
            if (currentSlot == static_cast<NodeRow*>(p->base())) {
                auto* exp = p->exponent();
                return (exp && exp->type() == NodeType::Row)
                           ? static_cast<NodeRow*>(exp)
                           : nullptr;
            }
        } else {
            if (currentSlot == static_cast<NodeRow*>(p->exponent())) {
                auto* base = p->base();
                return (base && base->type() == NodeType::Row)
                           ? static_cast<NodeRow*>(base)
                           : nullptr;
            }
        }
    }

    return nullptr;
}

bool CursorController::enterNodeFromLeft(MathNode* node) {
    NodeRow* slot = firstSlot(node);
    if (slot) {
        _cur.row   = slot;
        _cur.index = 0;
        return true;
    }
    return false;
}

bool CursorController::enterNodeFromRight(MathNode* node) {
    NodeRow* slot = lastSlot(node);
    if (slot) {
        _cur.row   = slot;
        _cur.index = slot->childCount();
        return true;
    }
    return false;
}

bool CursorController::exitRowRight() {
    auto info = findParentSlot(_cur.row);
    if (!info.parentRow) return false;   // Estamos en la raíz

    // Colocar el cursor DESPUÉS del nodo estructural en la fila padre
    _cur.row   = info.parentRow;
    _cur.index = info.indexInParent + 1;
    return true;
}

bool CursorController::exitRowLeft() {
    auto info = findParentSlot(_cur.row);
    if (!info.parentRow) return false;

    // Colocar el cursor ANTES del nodo estructural en la fila padre
    _cur.row   = info.parentRow;
    _cur.index = info.indexInParent;
    return true;
}

void CursorController::replaceEmptyIfNeeded() {
    // Si la fila tiene exactamente 1 NodeEmpty y vamos a insertar algo,
    // eliminar el Empty primero
    if (_cur.row->childCount() == 1 &&
        _cur.row->child(0)->type() == NodeType::Empty) {
        _cur.row->removeChild(0);
        _cur.index = 0;
    }
}

void CursorController::ensureNotEmpty(NodeRow* row) {
    if (row && row->isEmpty()) {
        row->appendChild(std::make_unique<NodeEmpty>());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Dígitos
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertDigit(char c) {
    if (!_cur.isValid()) return;
    if (c < '0' || (c > '9' && c != '.')) return;

    // Si el nodo a la izquierda es un Number, extender ese número
    MathNode* left = _cur.nodeLeft();
    if (left && left->type() == NodeType::Number) {
        auto* num = static_cast<NodeNumber*>(left);
        num->appendChar(c);
        // El cursor no avanza: sigue a la derecha del mismo NodeNumber
        return;
    }

    // Si estamos sobre un NodeEmpty solitario, reemplazarlo con el nuevo número
    replaceEmptyIfNeeded();

    // Crear nuevo NodeNumber
    std::string s(1, c);
    auto newNum = std::make_unique<NodeNumber>(s);
    _cur.row->insertChild(_cur.index, std::move(newNum));
    _cur.index++;
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Operador
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertOperator(OpKind op) {
    if (!_cur.isValid()) return;

    // Si sobre un Empty solitario, lo dejamos (el operador se pone aparte)
    // No reemplazamos el Empty para operadores — un operador se añade
    // junto al contenido existente.

    auto newOp = std::make_unique<NodeOperator>(op);
    _cur.row->insertChild(_cur.index, std::move(newOp));
    _cur.index++;
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Fracción (VPAM Capture)
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertFraction() {
    if (!_cur.isValid()) return;

    MathNode* left = _cur.nodeLeft();

    if (left && isCapturable(left)) {
        // ─── CAPTURA: mover el nodo izquierdo al numerador ───
        int captureIdx = _cur.index - 1;
        NodePtr captured = _cur.row->removeChild(captureIdx);
        _cur.index = captureIdx;   // Ajustar tras eliminación

        // Crear un Row para el numerador con el nodo capturado
        auto numRow = std::make_unique<NodeRow>();
        numRow->appendChild(std::move(captured));

        // Denominador vacío
        auto denRow = std::make_unique<NodeRow>();
        denRow->appendChild(std::make_unique<NodeEmpty>());

        // Crear la fracción
        auto frac = std::make_unique<NodeFraction>(std::move(numRow), std::move(denRow));
        NodeFraction* fracPtr = frac.get();

        _cur.row->insertChild(_cur.index, std::move(frac));

        // Cursor → dentro del denominador (el usuario debe escribir ahí)
        auto* den = fracPtr->denominator();
        if (den && den->type() == NodeType::Row) {
            _cur.row   = static_cast<NodeRow*>(den);
            _cur.index = 0;
        }

    } else {
        // ─── SIN CAPTURA: fracción con dos Empty ───
        replaceEmptyIfNeeded();

        auto frac = std::make_unique<NodeFraction>();
        NodeFraction* fracPtr = frac.get();

        _cur.row->insertChild(_cur.index, std::move(frac));

        // Cursor → dentro del numerador (el usuario escribe primero ahí)
        auto* num = fracPtr->numerator();
        if (num && num->type() == NodeType::Row) {
            _cur.row   = static_cast<NodeRow*>(num);
            _cur.index = 0;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Potencia (VPAM Capture + Jump)
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertPower() {
    if (!_cur.isValid()) return;

    MathNode* left = _cur.nodeLeft();

    NodePtr baseRow;
    if (left && isCapturable(left)) {
        // Capturar el nodo izquierdo como base
        int captureIdx = _cur.index - 1;
        NodePtr captured = _cur.row->removeChild(captureIdx);
        _cur.index = captureIdx;

        baseRow = std::make_unique<NodeRow>();
        static_cast<NodeRow*>(baseRow.get())->appendChild(std::move(captured));
    } else {
        // Base vacía
        replaceEmptyIfNeeded();
        baseRow = nullptr;   // Constructor por defecto creará Empty
    }

    // Exponente siempre vacío (el cursor saltará aquí)
    auto expRow = std::make_unique<NodeRow>();
    expRow->appendChild(std::make_unique<NodeEmpty>());

    auto power = std::make_unique<NodePower>(
        baseRow ? std::move(baseRow) : nullptr,
        std::move(expRow)
    );
    NodePower* powPtr = power.get();

    _cur.row->insertChild(_cur.index, std::move(power));

    // Cursor → dentro del exponente (salto automático)
    auto* exp = powPtr->exponent();
    if (exp && exp->type() == NodeType::Row) {
        _cur.row   = static_cast<NodeRow*>(exp);
        _cur.index = 0;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Raíz cuadrada
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertRoot() {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto root = std::make_unique<NodeRoot>();
    NodeRoot* rootPtr = root.get();

    _cur.row->insertChild(_cur.index, std::move(root));

    // Cursor → dentro del radicando
    auto* rad = rootPtr->radicand();
    if (rad && rad->type() == NodeType::Row) {
        _cur.row   = static_cast<NodeRow*>(rad);
        _cur.index = 0;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Paréntesis
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertParen() {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto paren = std::make_unique<NodeParen>();
    NodeParen* parPtr = paren.get();

    _cur.row->insertChild(_cur.index, std::move(paren));

    // Cursor → dentro del contenido
    auto* content = parPtr->content();
    if (content && content->type() == NodeType::Row) {
        _cur.row   = static_cast<NodeRow*>(content);
        _cur.index = 0;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// NAVEGACIÓN — moveRight
//
// Lógica:
//   1. Si nodo a la derecha es una estructura → entrar por la izquierda
//   2. Si nodo a la derecha es un leaf → saltar por encima (index++)
//   3. Si al final de la fila:
//      a. Si es fila hija → salir a la derecha del nodo padre
//      b. Si es la fila raíz → wrap al inicio
// ════════════════════════════════════════════════════════════════════════════
void CursorController::moveRight() {
    if (!_cur.isValid()) return;

    // ── Caso 1: Hay un nodo a la derecha ──
    if (!_cur.atEnd()) {
        MathNode* right = _cur.nodeRight();
        if (!right) { _cur.index++; return; }

        // ¿Es un nodo con ranuras internas? → entrar
        if (right->type() == NodeType::Fraction ||
            right->type() == NodeType::Power    ||
            right->type() == NodeType::Root     ||
            right->type() == NodeType::Paren    ||
            right->type() == NodeType::Function ||
            right->type() == NodeType::LogBase) {
            if (enterNodeFromLeft(right)) return;
        }

        // Nodo hoja (Number, Operator, Empty) → saltar
        _cur.index++;
        return;
    }

    // ── Caso 2: Al final de la fila ──
    if (isRootRow(_cur.row)) {
        // Wrap-around: volver al inicio
        _cur.index = 0;
        return;
    }

    // Salir de esta ranura hija a la fila padre
    // Caso especial: si estamos al final del numerador de una fracción,
    // saltar al inicio del denominador (no salir de la fracción todavía)
    MathNode* structParent = _cur.row->parent();
    if (structParent && structParent->type() == NodeType::Fraction) {
        auto* frac = static_cast<NodeFraction*>(structParent);
        if (_cur.row == static_cast<NodeRow*>(frac->numerator())) {
            // Estamos al final del numerador → saltar al denominador
            auto* den = frac->denominator();
            if (den && den->type() == NodeType::Row) {
                _cur.row   = static_cast<NodeRow*>(den);
                _cur.index = 0;
                return;
            }
        }
    }

    // Para Power: al final de la base → saltar al exponente
    if (structParent && structParent->type() == NodeType::Power) {
        auto* pow = static_cast<NodePower*>(structParent);
        if (_cur.row == static_cast<NodeRow*>(pow->base())) {
            auto* exp = pow->exponent();
            if (exp && exp->type() == NodeType::Row) {
                _cur.row   = static_cast<NodeRow*>(exp);
                _cur.index = 0;
                return;
            }
        }
    }

    // Para LogBase: al final de la base/subíndice → saltar al argumento
    if (structParent && structParent->type() == NodeType::LogBase) {
        auto* lb = static_cast<NodeLogBase*>(structParent);
        if (_cur.row == static_cast<NodeRow*>(lb->base())) {
            auto* arg = lb->argument();
            if (arg && arg->type() == NodeType::Row) {
                _cur.row   = static_cast<NodeRow*>(arg);
                _cur.index = 0;
                return;
            }
        }
    }

    // Caso general: salir a la derecha del nodo padre
    exitRowRight();
}

// ════════════════════════════════════════════════════════════════════════════
// NAVEGACIÓN — moveLeft
//
// Lógica simétrica a moveRight:
//   1. Si nodo a la izquierda es una estructura → entrar por la derecha
//   2. Si nodo a la izquierda es un leaf → saltar (index--)
//   3. Si al inicio de la fila:
//      a. Si es fila hija → salir a la izquierda del nodo padre
//      b. Si es la fila raíz → wrap al final
// ════════════════════════════════════════════════════════════════════════════
void CursorController::moveLeft() {
    if (!_cur.isValid()) return;

    // ── Caso 1: Hay un nodo a la izquierda ──
    if (!_cur.atStart()) {
        MathNode* left = _cur.nodeLeft();
        if (!left) { _cur.index--; return; }

        // ¿Nodo con ranuras internas? → entrar por la derecha
        if (left->type() == NodeType::Fraction ||
            left->type() == NodeType::Power    ||
            left->type() == NodeType::Root     ||
            left->type() == NodeType::Paren    ||
            left->type() == NodeType::Function ||
            left->type() == NodeType::LogBase) {
            _cur.index--;   // "Saltar por encima" para posicionarnos
            if (enterNodeFromRight(left)) return;
            // Si falló, quedamos a la izquierda del nodo (index ya decrementado)
            return;
        }

        // Nodo hoja → saltar
        _cur.index--;
        return;
    }

    // ── Caso 2: Al inicio de la fila ──
    if (isRootRow(_cur.row)) {
        // Wrap-around: ir al final
        _cur.index = _cur.row->childCount();
        return;
    }

    // Salir de esta ranura hija a la fila padre
    // Caso especial: si estamos al inicio del denominador de una fracción,
    // saltar al final del numerador
    MathNode* structParent = _cur.row->parent();
    if (structParent && structParent->type() == NodeType::Fraction) {
        auto* frac = static_cast<NodeFraction*>(structParent);
        if (_cur.row == static_cast<NodeRow*>(frac->denominator())) {
            auto* num = frac->numerator();
            if (num && num->type() == NodeType::Row) {
                _cur.row   = static_cast<NodeRow*>(num);
                _cur.index = _cur.row->childCount();
                return;
            }
        }
    }

    // Para Power: al inicio del exponente → saltar al final de la base
    if (structParent && structParent->type() == NodeType::Power) {
        auto* pow = static_cast<NodePower*>(structParent);
        if (_cur.row == static_cast<NodeRow*>(pow->exponent())) {
            auto* base = pow->base();
            if (base && base->type() == NodeType::Row) {
                _cur.row   = static_cast<NodeRow*>(base);
                _cur.index = _cur.row->childCount();
                return;
            }
        }
    }

    // Para LogBase: al inicio del argumento → saltar al final de la base
    if (structParent && structParent->type() == NodeType::LogBase) {
        auto* lb = static_cast<NodeLogBase*>(structParent);
        if (_cur.row == static_cast<NodeRow*>(lb->argument())) {
            auto* base = lb->base();
            if (base && base->type() == NodeType::Row) {
                _cur.row   = static_cast<NodeRow*>(base);
                _cur.index = _cur.row->childCount();
                return;
            }
        }
    }

    // Caso general: salir a la izquierda del nodo padre
    exitRowLeft();
}

// ════════════════════════════════════════════════════════════════════════════
// NAVEGACIÓN — moveUp / moveDown
// ════════════════════════════════════════════════════════════════════════════
void CursorController::moveUp() {
    if (!_cur.isValid()) return;

    NodeRow* sibling = verticalSibling(_cur.row, true);
    if (sibling) {
        // Preservar la posición horizontal lo mejor posible
        int newIdx = std::min(_cur.index, sibling->childCount());
        _cur.row   = sibling;
        _cur.index = newIdx;
    }
}

void CursorController::moveDown() {
    if (!_cur.isValid()) return;

    NodeRow* sibling = verticalSibling(_cur.row, false);
    if (sibling) {
        int newIdx = std::min(_cur.index, sibling->childCount());
        _cur.row   = sibling;
        _cur.index = newIdx;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// BORRADO — Backspace inteligente
//
// Lógica:
//   · Si el cursor está en index > 0, miramos el nodo a la izquierda.
//     - NodeNumber: borrar último dígito. Si queda vacío, eliminar nodo.
//     - NodeOperator: eliminar directamente.
//     - NodeEmpty: si estamos en una ranura hija, subir al padre.
//     - Estructura (Fraction/Power/Root/Paren): DESHACER (flatten).
//       → Extraer los contenidos de la primera ranura a la fila padre.
//   · Si el cursor está en index==0:
//     - En ranura hija: subir al padre (salir a la izquierda)
//     - En raíz: no hacer nada
// ════════════════════════════════════════════════════════════════════════════
void CursorController::backspace() {
    if (!_cur.isValid()) return;

    // ── Caso: cursor al inicio ──
    if (_cur.atStart()) {
        if (!isRootRow(_cur.row)) {
            // Subir al padre (sale de la ranura hija)
            exitRowLeft();
        }
        return;
    }

    // ── Hay algo a la izquierda ──
    MathNode* left = _cur.nodeLeft();
    if (!left) return;
    int leftIdx = _cur.index - 1;

    switch (left->type()) {

        // ── NodeNumber: borrar último dígito ──
        case NodeType::Number: {
            auto* num = static_cast<NodeNumber*>(left);
            num->deleteLastChar();
            if (num->length() == 0) {
                // Número vacío → eliminar nodo
                _cur.row->removeChild(leftIdx);
                _cur.index = leftIdx;
                ensureNotEmpty(_cur.row);
            }
            break;
        }

        // ── NodeOperator: eliminar directamente ──
        case NodeType::Operator: {
            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;
            ensureNotEmpty(_cur.row);
            break;
        }

        // ── NodeEmpty: En ranura, subir al padre. En raíz, eliminar. ──
        case NodeType::Empty: {
            if (!isRootRow(_cur.row)) {
                exitRowLeft();
            } else {
                // En raíz, si hay más nodos además del Empty, eliminarlo
                if (_cur.row->childCount() > 1) {
                    _cur.row->removeChild(leftIdx);
                    _cur.index = leftIdx;
                }
            }
            break;
        }

        // ── Estructuras: DESHACER (flatten) ──
        case NodeType::Fraction: {
            auto* frac = static_cast<NodeFraction*>(left);

            // Extraer el contenido del numerador a la fila padre
            auto* numRow = static_cast<NodeRow*>(frac->numerator());

            // Guardamos los nodos del numerador
            std::vector<NodePtr> extracted;
            while (numRow->childCount() > 0) {
                auto node = numRow->removeChild(numRow->childCount() - 1);
                // Descartamos NodeEmpty solitarios del numerador
                if (node->type() != NodeType::Empty) {
                    extracted.push_back(std::move(node));
                }
            }

            // Eliminar la fracción de la fila padre
            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;

            // Insertar los nodos extraídos (en orden inverso, los sacamos de atrás)
            int insertPos = _cur.index;
            for (auto it = extracted.rbegin(); it != extracted.rend(); ++it) {
                _cur.row->insertChild(insertPos, std::move(*it));
            }
            _cur.index = insertPos + static_cast<int>(extracted.size());

            ensureNotEmpty(_cur.row);
            break;
        }

        case NodeType::Power: {
            auto* pow = static_cast<NodePower*>(left);
            auto* baseRow = static_cast<NodeRow*>(pow->base());

            // Extraer contenido de la base
            std::vector<NodePtr> extracted;
            while (baseRow->childCount() > 0) {
                auto node = baseRow->removeChild(baseRow->childCount() - 1);
                if (node->type() != NodeType::Empty) {
                    extracted.push_back(std::move(node));
                }
            }

            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;

            int insertPos = _cur.index;
            for (auto it = extracted.rbegin(); it != extracted.rend(); ++it) {
                _cur.row->insertChild(insertPos, std::move(*it));
            }
            _cur.index = insertPos + static_cast<int>(extracted.size());

            ensureNotEmpty(_cur.row);
            break;
        }

        case NodeType::Root: {
            auto* root = static_cast<NodeRoot*>(left);
            auto* radRow = static_cast<NodeRow*>(root->radicand());

            std::vector<NodePtr> extracted;
            while (radRow->childCount() > 0) {
                auto node = radRow->removeChild(radRow->childCount() - 1);
                if (node->type() != NodeType::Empty) {
                    extracted.push_back(std::move(node));
                }
            }

            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;

            int insertPos = _cur.index;
            for (auto it = extracted.rbegin(); it != extracted.rend(); ++it) {
                _cur.row->insertChild(insertPos, std::move(*it));
            }
            _cur.index = insertPos + static_cast<int>(extracted.size());

            ensureNotEmpty(_cur.row);
            break;
        }

        case NodeType::Paren: {
            auto* paren = static_cast<NodeParen*>(left);
            auto* contentRow = static_cast<NodeRow*>(paren->content());

            std::vector<NodePtr> extracted;
            while (contentRow->childCount() > 0) {
                auto node = contentRow->removeChild(contentRow->childCount() - 1);
                if (node->type() != NodeType::Empty) {
                    extracted.push_back(std::move(node));
                }
            }

            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;

            int insertPos = _cur.index;
            for (auto it = extracted.rbegin(); it != extracted.rend(); ++it) {
                _cur.row->insertChild(insertPos, std::move(*it));
            }
            _cur.index = insertPos + static_cast<int>(extracted.size());

            ensureNotEmpty(_cur.row);
            break;
        }

        // ── Función: deshacer → extraer contenido del argumento ──
        case NodeType::Function: {
            auto* func = static_cast<NodeFunction*>(left);
            auto* argRow = static_cast<NodeRow*>(func->argument());

            std::vector<NodePtr> extracted;
            while (argRow->childCount() > 0) {
                auto node = argRow->removeChild(argRow->childCount() - 1);
                if (node->type() != NodeType::Empty) {
                    extracted.push_back(std::move(node));
                }
            }

            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;

            int insertPos = _cur.index;
            for (auto it = extracted.rbegin(); it != extracted.rend(); ++it) {
                _cur.row->insertChild(insertPos, std::move(*it));
            }
            _cur.index = insertPos + static_cast<int>(extracted.size());

            ensureNotEmpty(_cur.row);
            break;
        }

        // ── LogBase: deshacer → extraer contenido del argumento ──
        case NodeType::LogBase: {
            auto* lb = static_cast<NodeLogBase*>(left);
            auto* argRow = static_cast<NodeRow*>(lb->argument());

            std::vector<NodePtr> extracted;
            while (argRow->childCount() > 0) {
                auto node = argRow->removeChild(argRow->childCount() - 1);
                if (node->type() != NodeType::Empty) {
                    extracted.push_back(std::move(node));
                }
            }

            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;

            int insertPos = _cur.index;
            for (auto it = extracted.rbegin(); it != extracted.rend(); ++it) {
                _cur.row->insertChild(insertPos, std::move(*it));
            }
            _cur.index = insertPos + static_cast<int>(extracted.size());

            ensureNotEmpty(_cur.row);
            break;
        }

        // ── Constante (π, e): eliminar directamente ──
        case NodeType::Constant: {
            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;
            ensureNotEmpty(_cur.row);
            break;
        }

        // ── Variable (x, y, z, A-F, Ans, PreAns): eliminar directamente ──
        case NodeType::Variable: {
            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;
            ensureNotEmpty(_cur.row);
            break;
        }

        default:
            // Nodo desconocido: eliminar directamente
            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;
            ensureNotEmpty(_cur.row);
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Función trigonométrica / logarítmica (sin captura)
//
// Crea NodeFunction(kind) con argumento vacío y posiciona el cursor
// dentro del argumento para que el usuario escriba.
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertFunction(FuncKind kind) {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto func = std::make_unique<NodeFunction>(kind);
    NodeFunction* funcPtr = func.get();

    _cur.row->insertChild(_cur.index, std::move(func));

    // Cursor → dentro del argumento
    auto* arg = funcPtr->argument();
    if (arg && arg->type() == NodeType::Row) {
        _cur.row   = static_cast<NodeRow*>(arg);
        _cur.index = 0;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — LogBase: log_n(x)
//
// Crea NodeLogBase con base (subíndice) y argumento vacíos.
// Cursor → dentro de la base (subíndice) para que el usuario escriba n.
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertLogBase() {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto lb = std::make_unique<NodeLogBase>();
    NodeLogBase* lbPtr = lb.get();

    _cur.row->insertChild(_cur.index, std::move(lb));

    // Cursor → dentro de la base (subíndice)
    auto* base = lbPtr->base();
    if (base && base->type() == NodeType::Row) {
        _cur.row   = static_cast<NodeRow*>(base);
        _cur.index = 0;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Constante (π, e)
//
// Inserta un NodeConstant (sin estructura interna, como un Number atómico).
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertConstant(ConstKind kind) {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto cst = std::make_unique<NodeConstant>(kind);
    _cur.row->insertChild(_cur.index, std::move(cst));
    _cur.index++;
}

// ════════════════════════════════════════════════════════════════════════════
// INSERCIÓN — Variable (x, y, z, A-F, Ans, PreAns)
//
// Inserta un NodeVariable (sin estructura interna, como un Number atómico).
// ════════════════════════════════════════════════════════════════════════════
void CursorController::insertVariable(char name) {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto var = std::make_unique<NodeVariable>(name);
    _cur.row->insertChild(_cur.index, std::move(var));
    _cur.index++;
}

} // namespace vpam
