/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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
    ++_epoch;
    _root      = rootRow;
    _cur.row   = rootRow;
    _cur.index = 0;
}

bool CursorController::insertPrepared(NodePtr node, NodeRow* slot, int index) {
    if (!node || !_cur.row || _cur.index < 0 || _cur.index > _cur.row->childCount()) return false;
    if (slot) {
        const MathNode* ancestor=slot;
        unsigned depth=0;
        while(ancestor && ancestor!=node.get() && ++depth<=82) ancestor=ancestor->parent();
        if(ancestor!=node.get() || index<0 || index>slot->childCount()) return false;
    }
#if defined(__cpp_exceptions)
    try {
#endif
        // WHY: explicit multiplication also preserves adjacent mantissas in
        // receivers whose serializer does not synthesize implicit products.
        const bool multiply=isCapturable(_cur.nodeLeft()) || scanUnits(_cur.nodeLeft())==UnitScan::Present ||
            (_cur.nodeLeft() && _cur.nodeLeft()->type()==NodeType::Call);
        OpKind operation=OpKind::Mul;
        if(scanUnits(node.get())==UnitScan::Present) {
            operation=scanUnits(_cur.nodeLeft())==UnitScan::Present?OpKind::UnitProduct:OpKind::UnitAttach;
            if(operation==OpKind::UnitAttach && node->type()==NodeType::Unit &&
               numos::units::angularTight(static_cast<NodeUnit*>(node.get())->atom()))operation=OpKind::UnitAttachTight;
        }
        NodePtr product=multiply ? makeOperator(operation) : nullptr;
        auto* sequence=!slot && node->type()==NodeType::Row && scanUnits(node.get())==UnitScan::Present
            ? static_cast<NodeRow*>(node.get()):nullptr;
        // WHY: a unit product has editable components, not an invisible nested
        // row that the cursor cannot enter. Reserve the whole batch before
        // removing any authored node or publishing the first component.
        const int added=sequence?sequence->childCount():1;
        _cur.row->reserveChildren(_cur.row->childCount()+added+(multiply?1:0));
        replaceEmptyIfNeeded();
        if(product) _cur.row->insertChild(_cur.index++,std::move(product));
        if(sequence)while(sequence->childCount())_cur.row->insertChild(_cur.index++,sequence->removeChild(0));
        else _cur.row->insertChild(_cur.index++,std::move(node));
        if(slot) _cur={slot,index};
        ++_epoch;
        return true;
#if defined(__cpp_exceptions)
    } catch(const std::bad_alloc&) { return false; }
#endif
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
        case NodeType::Unit:
        case NodeType::Measurement:
        case NodeType::QuantityReference:
        case NodeType::Symbol:
        case NodeType::SpecialValue:
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
            auto* rad = r->hasDegree() ? r->degree() : r->radicand();
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
        case NodeType::Measurement:
        case NodeType::Call:
        case NodeType::DefIntegral: {
            auto* child=node->child(0);
            return child && child->type()==NodeType::Row ? static_cast<NodeRow*>(child) : nullptr;
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
        case NodeType::Measurement:
        case NodeType::Call:
        case NodeType::DefIntegral: {
            auto* child=node->child(node->childCount()-1);
            return child && child->type()==NodeType::Row ? static_cast<NodeRow*>(child) : nullptr;
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
    // WHY: a unary sign may precede the pending operand. Replace only the
    // placeholder at this insertion boundary, never unrelated incomplete slots.
    if (_cur.nodeRight() && _cur.nodeRight()->type() == NodeType::Empty) {
        _cur.row->removeChild(_cur.index);
    } else if (_cur.nodeLeft() && _cur.nodeLeft()->type() == NodeType::Empty) {
        _cur.row->removeChild(--_cur.index);
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
    // Aceptar dígitos '0'..'9' y el punto decimal '.'. OJO con el orden: '.' es
    // ASCII 46, MENOR que '0' (48), así que un guard `c < '0' || ...` rechazaría
    // '.' por la PRIMERA cláusula antes de llegar a la excepción. La forma correcta
    // exime '.' primero y solo entonces aplica el rango de dígitos.
    if (c != '.' && (c < '0' || c > '9')) return;

    // Si el nodo a la izquierda es un Number, extender ese número
    MathNode* left = _cur.nodeLeft();
    if (left && left->type() == NodeType::Number) {
        auto* num = static_cast<NodeNumber*>(left);
        num->appendChar(c);
        // El cursor no avanza: sigue a la derecha del mismo NodeNumber
        return;
    }

    // Prepare allocation before replacing the pending operand.
    std::string s(1, c);
    auto newNum = std::make_unique<NodeNumber>(s);
    replaceEmptyIfNeeded();
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
    if ((op == OpKind::Add || op == OpKind::Sub) &&
        _cur.row->childCount() == 1 &&
        _cur.row->child(0)->type() == NodeType::Empty) {
        // A leading sign belongs BEFORE its pending operand, including when
        // the user re-enters the empty slot from the right.
        _cur.index = 0;
    }
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
bool CursorController::insertPowerOfTen() {
    if (!_cur.isValid()) return false;
#if defined(__cpp_exceptions)
    try {
#endif
        const bool multiply = isCapturable(_cur.nodeLeft());
        auto base = std::make_unique<NodeRow>();
        base->appendChild(std::make_unique<NodeNumber>("10"));
        auto exponent = std::make_unique<NodeRow>();
        exponent->appendChild(std::make_unique<NodeEmpty>());
        NodeRow* exponentSlot = exponent.get();
        auto power = std::make_unique<NodePower>(std::move(base), std::move(exponent));
        NodePtr times = multiply ? std::make_unique<NodeOperator>(OpKind::Mul) : nullptr;
        // WHY: once storage is reserved, publication only moves unique_ptrs.
        // No orphan operator or half-power survives allocation failure.
        _cur.row->reserveChildren(_cur.row->childCount() + (multiply ? 2 : 1));
        replaceEmptyIfNeeded();
        if (times) _cur.row->insertChild(_cur.index++, std::move(times));
        _cur.row->insertChild(_cur.index, std::move(power));
        _cur.row = exponentSlot;
        _cur.index = 0;
        return true;
#if defined(__cpp_exceptions)
    } catch (const std::bad_alloc&) {
        return false;
    }
#endif
}

bool CursorController::insertFactorial() {
    if (!_cur.isValid()) return false;
#if defined(__cpp_exceptions)
    try {
#endif
        const bool capture = isCapturable(_cur.nodeLeft());
        auto argument = std::make_unique<NodeRow>();
        argument->reserveChildren(1);
        if (!capture) argument->appendChild(makeEmpty());
        auto* slot = argument.get();
        auto factorial = std::make_unique<NodeFunction>(FuncKind::Factorial, std::move(argument));
        // WHY: finish allocations before moving the user's operand.
        _cur.row->reserveChildren(_cur.row->childCount() + 1);
        if (capture) slot->appendChild(_cur.row->removeChild(--_cur.index));
        else replaceEmptyIfNeeded();
        _cur.row->insertChild(_cur.index++, std::move(factorial));
        if (!capture) _cur = {slot, 0};
        return true;
#if defined(__cpp_exceptions)
    } catch (const std::bad_alloc&) { return false; }
#endif
}

void CursorController::insertPower() {
    if (!_cur.isValid()) return;
#if defined(__cpp_exceptions)
    try {
#endif
        const bool capture = isCapturable(_cur.nodeLeft());
        auto base = std::make_unique<NodeRow>();
        base->reserveChildren(1);
        if (!capture) base->appendChild(std::make_unique<NodeEmpty>());
        auto* baseSlot = base.get();
        if (capture && _cur.nodeLeft()->type()==NodeType::SpecialValue) {
            const auto kind=static_cast<const NodeSpecialValue*>(_cur.nodeLeft())->specialKind();
            if (kind==SpecialValueKind::PositiveInfinity ||
                kind==SpecialValueKind::NegativeInfinity || kind==SpecialValueKind::BothInfinities) {
                // WHY: the whole signed atom is captured. Show (-infinity)^2,
                // not -infinity^2; use the ordinary MATH delimiter path.
                auto signedBase=std::make_unique<NodeRow>();
                signedBase->reserveChildren(1);
                baseSlot=signedBase.get();
                base->appendChild(std::make_unique<NodeParen>(std::move(signedBase)));
            }
        }
        auto exponent = std::make_unique<NodeRow>();
        exponent->appendChild(std::make_unique<NodeEmpty>());
        auto* exponentSlot = exponent.get();
        auto power = std::make_unique<NodePower>(std::move(base), std::move(exponent));
        // WHY: prepare all ownership/capacity before removing the user's base.
        _cur.row->reserveChildren(_cur.row->childCount() + 1);
        if (capture) baseSlot->appendChild(_cur.row->removeChild(--_cur.index));
        else replaceEmptyIfNeeded();
        _cur.row->insertChild(_cur.index, std::move(power));
        _cur = {exponentSlot, 0};
#if defined(__cpp_exceptions)
    } catch (const std::bad_alloc&) {
        // Allocation failure leaves the original expression and cursor intact.
    }
#endif
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
void CursorController::insertParen(DelimKind kind) {
    if (!_cur.isValid()) return;

    replaceEmptyIfNeeded();

    auto paren = std::make_unique<NodeParen>(kind);
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
void CursorController::closeBracket() {
    if (!_cur.isValid()) return;
    for (MathNode* node = _cur.row; node; node = node->parent()) {
        if (node->type() != NodeType::Paren ||
            static_cast<NodeParen*>(node)->delimKind() != DelimKind::Bracket) continue;
        auto* parent = node->parent();
        if (!parent || parent->type() != NodeType::Row) return;
        auto* row = static_cast<NodeRow*>(parent);
        for (int i = 0; i < row->childCount(); ++i) {
            if (row->child(i) == node) { _cur = {row, i + 1}; return; }
        }
        return;
    }
}

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
            right->type() == NodeType::Measurement || right->type() == NodeType::LogBase || right->type() == NodeType::Call || right->type() == NodeType::DefIntegral) {
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

    if (structParent && (structParent->type()==NodeType::Measurement || structParent->type()==NodeType::Call || structParent->type()==NodeType::DefIntegral)) {
        for(int i=0;i<structParent->childCount();++i) {
            if(structParent->child(i)!=_cur.row) continue;
            auto* sibling=structParent->child(i+1);
            if(sibling && sibling->type()==NodeType::Row) {
                _cur.row=static_cast<NodeRow*>(sibling);
                _cur.index=0;
                return;
            }
            break;
        }
    }
    if(structParent && structParent->type()==NodeType::Root) {
        auto* root=static_cast<NodeRoot*>(structParent);
        if(root->hasDegree() && _cur.row==root->degree()) {
            _cur.row=static_cast<NodeRow*>(root->radicand());
            _cur.index=0;
            return;
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
            left->type() == NodeType::Measurement || left->type() == NodeType::LogBase || left->type() == NodeType::Call || left->type() == NodeType::DefIntegral) {
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

    if (structParent && (structParent->type()==NodeType::Measurement || structParent->type()==NodeType::Call || structParent->type()==NodeType::DefIntegral)) {
        for(int i=0;i<structParent->childCount();++i) {
            if(structParent->child(i)!=_cur.row) continue;
            auto* sibling=structParent->child(i-1);
            if(sibling && sibling->type()==NodeType::Row) {
                _cur.row=static_cast<NodeRow*>(sibling);
                _cur.index=_cur.row->childCount();
                return;
            }
            break;
        }
    }
    if(structParent && structParent->type()==NodeType::Root) {
        auto* root=static_cast<NodeRoot*>(structParent);
        if(root->hasDegree() && _cur.row==root->radicand()) {
            _cur.row=static_cast<NodeRow*>(root->degree());
            _cur.index=_cur.row->childCount();
            return;
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
bool CursorController::unwrapStructure(NodeRow* row, int index, NodeRow* retained) {
    if (!row || !retained) return false;
#if defined(__cpp_exceptions)
    try {
#endif
        const int count = retained->childCount();
        // Retain the existing placeholder if this will be the sole empty row.
        // A populated slot keeps its order, including signs and operators.
        row->reserveChildren(row->childCount() - 1 + std::max(1, count));
        NodePtr empty;
        if (count == 0 && row->childCount() == 1) empty = makeEmpty();
        NodePtr owner = row->removeChild(index);
        _cur = {row, index};
        while (retained->childCount()) {
            auto node = retained->removeChild(0);
            if (node->type() != NodeType::Empty || (count == 1 && row->isEmpty()))
                row->insertChild(_cur.index++, std::move(node));
        }
        if (empty) row->insertChild(_cur.index, std::move(empty));
        if (row->childCount() == 1 && row->child(0)->type() == NodeType::Empty)
            _cur.index = 0;
        return true;
#if defined(__cpp_exceptions)
    } catch (const std::bad_alloc&) {
        return false; // Only preparation above can allocate.
    }
#endif
}

void CursorController::backspace() {
    if (!_cur.isValid()) return;

    // WHY: DEL in an empty trailing slot must undo the template, not leave an
    // invisible abandoned structure to its right. Never drop a populated slot.
    const bool emptySlot = _cur.row->childCount() == 1 &&
                           _cur.row->child(0)->type() == NodeType::Empty;
    if (emptySlot && !isRootRow(_cur.row)) {
        auto info = findParentSlot(_cur.row);
        auto* owner = _cur.row->parent();
        NodeRow* retained = nullptr;
        if (owner && owner->type() == NodeType::Power) {
            auto* power = static_cast<NodePower*>(owner);
            if (power->exponent() == _cur.row)
                retained = static_cast<NodeRow*>(power->base());
        } else if (owner && owner->type() == NodeType::Fraction) {
            auto* frac = static_cast<NodeFraction*>(owner);
            if (frac->denominator() == _cur.row)
                retained = static_cast<NodeRow*>(frac->numerator());
        } else if (owner && (owner->type() == NodeType::Paren ||
                            owner->type() == NodeType::Function ||
                            (owner->type() == NodeType::Root &&
                             !static_cast<NodeRoot*>(owner)->hasDegree()))) {
            retained = _cur.row;
        }
        if (retained && info.parentRow) {
            unwrapStructure(info.parentRow, info.indexInParent, retained);
            return;
        }
    }
    if (_cur.atStart()) {
        if (!isRootRow(_cur.row)) exitRowLeft();
        return;
    }

    MathNode* left = _cur.nodeLeft();
    if (!left) return;
    const int leftIdx = _cur.index - 1;
    NodeRow* retained = nullptr;
    switch (left->type()) {
        case NodeType::Power: retained = static_cast<NodeRow*>(static_cast<NodePower*>(left)->base()); break;
        case NodeType::Measurement: retained = static_cast<NodeRow*>(static_cast<NodeMeasurement*>(left)->nominal()); break;
        case NodeType::Fraction: retained = static_cast<NodeRow*>(static_cast<NodeFraction*>(left)->numerator()); break;
        case NodeType::Root: retained = static_cast<NodeRow*>(static_cast<NodeRoot*>(left)->radicand()); break;
        case NodeType::Paren: retained = static_cast<NodeRow*>(static_cast<NodeParen*>(left)->content()); break;
        case NodeType::Function: retained = static_cast<NodeRow*>(static_cast<NodeFunction*>(left)->argument()); break;
        case NodeType::LogBase: retained = static_cast<NodeRow*>(static_cast<NodeLogBase*>(left)->argument()); break;
        default: break;
    }
    if (retained) {
        unwrapStructure(_cur.row, leftIdx, retained);
        return;
    }
    if (left->type() == NodeType::Empty) {
        if (!isRootRow(_cur.row)) exitRowLeft();
        else if (_cur.row->childCount() > 1) {
            _cur.row->removeChild(leftIdx);
            _cur.index = leftIdx;
        }
        return;
    }
    if (left->type() == NodeType::Number) {
        auto* number = static_cast<NodeNumber*>(left);
        if (number->length() > 1) { number->deleteLastChar(); return; }
    }
    // Prepare the replacement before erasing the last leaf.
    NodePtr empty = _cur.row->childCount() == 1 ? makeEmpty() : nullptr;
    _cur.row->removeChild(leftIdx);
    _cur.index = leftIdx;
    if (empty) _cur.row->appendChild(std::move(empty));
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
