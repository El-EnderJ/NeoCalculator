/**
 * ExprNode.h — Natural V.P.A.M. expression tree.
 * Supports TEXT, FRACTION, ROOT and POWER nodes.
 */
#pragma once
#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif

enum class NodeType : uint8_t { TEXT, FRACTION, ROOT, POWER };

enum class CursorPart : uint8_t {
    MAIN, 
    FRAC_NUM, FRAC_DEN, 
    ROOT_INDEX, ROOT_RADICAND,
    POWER_BASE, POWER_EXP
};

struct ExprNode {
    NodeType type;
    String   text;
    bool     isBlock;       // true for "sin(", "cos(" etc.

    // FRACTION
    ExprNode* numerator;    
    ExprNode* denominator;

    // ROOT
    ExprNode* rootIndex;    // nullptr = square root
    ExprNode* radicand;

    // POWER
    ExprNode* exponent;     // The superscript part

    ExprNode* next;         // linked-list sibling

    /* ── Factories ── */
    static ExprNode* newText(const String& t = "", bool block = false) {
        ExprNode* n  = new ExprNode();
        n->type      = NodeType::TEXT;
        n->text      = t;
        n->isBlock   = block;
        n->numerator = n->denominator = nullptr;
        n->rootIndex = n->radicand    = nullptr;
        n->exponent  = nullptr;
        n->next      = nullptr;
        return n;
    }
    static ExprNode* newFraction() {
        ExprNode* n  = new ExprNode();
        n->type      = NodeType::FRACTION;
        n->isBlock   = false;
        n->numerator   = newText("");
        n->denominator = newText("");
        n->rootIndex = n->radicand = n->exponent = nullptr;
        n->next      = nullptr;
        return n;
    }
    static ExprNode* newRoot(bool withIndex) {
        ExprNode* n  = new ExprNode();
        n->type      = NodeType::ROOT;
        n->isBlock   = false;
        n->numerator = n->denominator = nullptr;
        n->rootIndex = withIndex ? newText("") : nullptr;
        n->radicand  = newText("");
        n->exponent  = nullptr;
        n->next      = nullptr;
        return n;
    }
    static ExprNode* newPower() {
        ExprNode* n  = new ExprNode();
        n->type      = NodeType::POWER;
        n->isBlock   = false;
        n->numerator = n->denominator = nullptr;
        n->rootIndex = n->radicand    = nullptr;
        n->exponent  = newText("");
        n->next      = nullptr;
        return n;
    }

    static void freeChain(ExprNode* h) {
        while (h) {
            ExprNode* nx = h->next;
            if (h->type == NodeType::FRACTION) {
                freeChain(h->numerator);
                freeChain(h->denominator);
            } else if (h->type == NodeType::ROOT) {
                freeChain(h->rootIndex);
                freeChain(h->radicand);
            } else if (h->type == NodeType::POWER) {
                freeChain(h->exponent);
            }
            delete h;
            h = nx;
        }
    }
};

struct CursorPos {
    ExprNode*  node;
    int        offset;
    CursorPart part;
    CursorPos() : node(nullptr), offset(0), part(CursorPart::MAIN) {}
};
