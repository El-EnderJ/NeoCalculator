/**
 * MathAST.cpp — Implementación del AST y Layout Recursivo V.P.A.M.
 *
 * Fase 1+5: Cálculo geométrico puro (sin LVGL).
 *
 * Cada nodo calcula su bounding box {width, ascent, descent} de forma
 * recursiva bottom-up. Los nodos contenedores (Fraction, Power, Root, Paren,
 * Function, LogBase) primero calculan el layout de sus hijos y luego
 * componen el suyo propio.
 *
 * Convenciones:
 *   · ascent  = píxeles SOBRE el baseline (positivo hacia arriba)
 *   · descent = píxeles BAJO  el baseline (positivo hacia abajo)
 *   · El baseline es la línea imaginaria donde "se apoyan" los dígitos.
 *   · El eje matemático (axis) está a fontSize/2 sobre el baseline:
 *     ahí se dibujan las barras de fracción y se centra ±×.
 */

#include "MathAST.h"
#include <algorithm>

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// MathNode — PSRAM allocation
// ════════════════════════════════════════════════════════════════════════════
void* MathNode::operator new(std::size_t size) {
    void* p = nullptr;
#ifdef ARDUINO
    p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(size, MALLOC_CAP_8BIT); // fallback
#else
    p = std::malloc(size);
#endif
    if (!p) throw std::bad_alloc();
    return p;
}

void MathNode::operator delete(void* ptr) noexcept {
    if (!ptr) return;
#ifdef ARDUINO
    heap_caps_free(ptr);
#else
    std::free(ptr);
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// Utilidad interna
// ════════════════════════════════════════════════════════════════════════════
static NodePtr makeEmptyRow() {
    auto row = std::make_unique<NodeRow>();
    row->appendChild(std::make_unique<NodeEmpty>());
    return row;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e R o w
// ════════════════════════════════════════════════════════════════════════════
NodeRow::NodeRow() : MathNode(NodeType::Row) {}

void NodeRow::calculateLayout(const FontMetrics& fm) {
    if (_children.empty()) {
        // Fila vacía: tamaño cero pero conserva baseline coherente
        _layout.width   = 0;
        _layout.ascent  = fm.ascent;
        _layout.descent = fm.descent;
        return;
    }

    int16_t totalW     = 0;
    int16_t maxAscent  = 0;
    int16_t maxDescent = 0;

    for (size_t i = 0; i < _children.size(); ++i) {
        _children[i]->calculateLayout(fm);
        const auto& cl = _children[i]->layout();

        if (i > 0) totalW += CHILD_GAP;
        totalW     += cl.width;
        maxAscent   = std::max(maxAscent,  cl.ascent);
        maxDescent  = std::max(maxDescent, cl.descent);
    }

    _layout.width   = totalW;
    _layout.ascent  = maxAscent;
    _layout.descent = maxDescent;
}

int NodeRow::childCount() const {
    return static_cast<int>(_children.size());
}

MathNode* NodeRow::child(int index) const {
    if (index < 0 || index >= static_cast<int>(_children.size())) return nullptr;
    return _children[static_cast<size_t>(index)].get();
}

void NodeRow::appendChild(NodePtr node) {
    if (!node) return;
    node->setParent(this);
    _children.push_back(std::move(node));
}

void NodeRow::insertChild(int index, NodePtr node) {
    if (!node) return;
    node->setParent(this);
    if (index >= static_cast<int>(_children.size())) {
        _children.push_back(std::move(node));
    } else {
        _children.insert(_children.begin() + index, std::move(node));
    }
}

NodePtr NodeRow::removeChild(int index) {
    if (index < 0 || index >= static_cast<int>(_children.size())) return nullptr;
    NodePtr node = std::move(_children[static_cast<size_t>(index)]);
    _children.erase(_children.begin() + index);
    node->setParent(nullptr);
    return node;
}

void NodeRow::clear() {
    for (auto& c : _children) c->setParent(nullptr);
    _children.clear();
}

void NodeRow::replaceChild(int index, NodePtr node) {
    if (index < 0 || index >= static_cast<int>(_children.size())) return;
    if (!node) return;
    _children[static_cast<size_t>(index)]->setParent(nullptr);
    node->setParent(this);
    _children[static_cast<size_t>(index)] = std::move(node);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e N u m b e r
// ════════════════════════════════════════════════════════════════════════════
NodeNumber::NodeNumber(const std::string& value)
    : MathNode(NodeType::Number), _value(value)
{
}

void NodeNumber::calculateLayout(const FontMetrics& fm) {
    int len = static_cast<int>(_value.size());
    if (len == 0) len = 1;   // Mínimo 1 carácter de ancho (para cursor)

    _layout.width   = fm.charWidth * static_cast<int16_t>(len);
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
}

void NodeNumber::appendChar(char c) {
    // Solo aceptar dígitos y punto decimal (máximo un punto)
    if (c >= '0' && c <= '9') {
        _value += c;
    } else if (c == '.' && !hasDecimalPoint()) {
        _value += c;
    }
}

void NodeNumber::deleteLastChar() {
    if (!_value.empty()) _value.pop_back();
}

bool NodeNumber::hasDecimalPoint() const {
    return _value.find('.') != std::string::npos;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e O p e r a t o r
// ════════════════════════════════════════════════════════════════════════════
NodeOperator::NodeOperator(OpKind op)
    : MathNode(NodeType::Operator), _op(op)
{
}

void NodeOperator::calculateLayout(const FontMetrics& fm) {
    // Ancho: 1 carácter + padding a cada lado
    _layout.width   = fm.charWidth + OP_PAD * 2;
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
}

const char* NodeOperator::symbol() const {
    switch (_op) {
        case OpKind::Add:       return "+";
        case OpKind::Sub:       return "-";              // ASCII hyphen-minus (U+002D)
        case OpKind::Mul:       return "\xc3\x97";       // × (U+00D7)
        case OpKind::PlusMinus: return "\xc2\xb1";       // ± (U+00B1)
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e E m p t y
// ════════════════════════════════════════════════════════════════════════════
NodeEmpty::NodeEmpty() : MathNode(NodeType::Empty) {}

void NodeEmpty::calculateLayout(const FontMetrics& fm) {
    // El placeholder se alinea con el texto normal por baseline.
    // Su ancho/alto es al menos el mínimo visual, pero no menor que la fuente.
    _layout.width   = std::max(MIN_WIDTH,  fm.charWidth);
    _layout.ascent  = std::max(MIN_HEIGHT, fm.ascent);
    _layout.descent = fm.descent;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e F r a c t i o n
// ════════════════════════════════════════════════════════════════════════════
NodePtr NodeFraction::makeEmptySlot() {
    return makeEmptyRow();
}

NodeFraction::NodeFraction() : MathNode(NodeType::Fraction) {
    _numerator   = makeEmptySlot();
    _numerator->setParent(this);
    _denominator = makeEmptySlot();
    _denominator->setParent(this);
}

NodeFraction::NodeFraction(NodePtr numerator, NodePtr denominator)
    : MathNode(NodeType::Fraction)
{
    setNumerator(std::move(numerator));
    setDenominator(std::move(denominator));
}

void NodeFraction::calculateLayout(const FontMetrics& fm) {
    // 1. Calcular layout de hijos
    _numerator->calculateLayout(fm);
    _denominator->calculateLayout(fm);

    const auto& numL = _numerator->layout();
    const auto& denL = _denominator->layout();

    // 2. Ancho: el mayor de los dos hijos + padding horizontal
    int16_t contentW = std::max(numL.width, denL.width);
    _layout.width = contentW + BAR_H_PAD * 2;

    // 3. Posición vertical relativa al eje matemático
    //
    //    La barra de fracción se dibuja en el eje matemático
    //    (axis = ascent/2 sobre el baseline).
    //
    //         ┌── numH ──┐
    //         │numerador  │    ↑ aboveAxis
    //    ─────┼──barra────┼─── eje (axis sobre baseline)
    //         │denominador│    ↓ belowAxis
    //         └── denH ──┘
    //
    int16_t axis = fm.axisHeight();

    // Mitad superior de la barra (redondeo hacia arriba)
    int16_t barHalfUp   = (BAR_THICK + 1) / 2;   // 1 para grosor 1
    // Mitad inferior de la barra (redondeo hacia abajo)
    int16_t barHalfDown = BAR_THICK / 2;           // 0 para grosor 1

    int16_t numH = numL.height();
    int16_t denH = denL.height();

    int16_t aboveAxis = barHalfUp   + BAR_V_GAP + numH;
    int16_t belowAxis = barHalfDown + BAR_V_GAP + denH;

    // Convertir de eje-relativo a baseline-relativo
    _layout.ascent  = axis + aboveAxis;
    _layout.descent = belowAxis - axis;

    // Seguridad: descent nunca negativo
    if (_layout.descent < 0) {
        _layout.ascent += -_layout.descent;
        _layout.descent = 0;
    }
}

MathNode* NodeFraction::child(int index) const {
    if (index == 0) return _numerator.get();
    if (index == 1) return _denominator.get();
    return nullptr;
}

void NodeFraction::setNumerator(NodePtr node) {
    _numerator = node ? std::move(node) : makeEmptySlot();
    _numerator->setParent(this);
}

void NodeFraction::setDenominator(NodePtr node) {
    _denominator = node ? std::move(node) : makeEmptySlot();
    _denominator->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e P o w e r
// ════════════════════════════════════════════════════════════════════════════
NodePower::NodePower() : MathNode(NodeType::Power) {
    _base     = makeEmptyRow();
    _base->setParent(this);
    _exponent = makeEmptyRow();
    _exponent->setParent(this);
}

NodePower::NodePower(NodePtr base, NodePtr exponent)
    : MathNode(NodeType::Power)
{
    setBase(std::move(base));
    setExponent(std::move(exponent));
}

void NodePower::calculateLayout(const FontMetrics& fm) {
    // 1. Base en fuente normal
    _base->calculateLayout(fm);
    const auto& baseL = _base->layout();

    // 2. Exponente en fuente reducida (superscript)
    FontMetrics fmSup = fm.superscript();
    _exponent->calculateLayout(fmSup);
    const auto& expL = _exponent->layout();

    // 3. Ancho: base + exponente (pegados, sin gap)
    _layout.width = baseL.width + expL.width;

    // 4. Elevación del exponente:
    //    El fondo del exponente se sitúa a EXP_RAISE_NUM/EXP_RAISE_DEN
    //    (≈60%) del ascent de la base sobre el baseline.
    //
    //    expShift = distancia del baseline al fondo del exponente
    int16_t expShift = (baseL.ascent * EXP_RAISE_NUM) / EXP_RAISE_DEN;

    // El tope del exponente está a expShift + expL.ascent sobre el baseline
    _layout.ascent  = std::max(baseL.ascent,
                               static_cast<int16_t>(expShift + expL.ascent));
    // El exponente no extiende hacia abajo
    _layout.descent = baseL.descent;
}

MathNode* NodePower::child(int index) const {
    if (index == 0) return _base.get();
    if (index == 1) return _exponent.get();
    return nullptr;
}

void NodePower::setBase(NodePtr node) {
    _base = node ? std::move(node) : makeEmptyRow();
    _base->setParent(this);
}

void NodePower::setExponent(NodePtr node) {
    _exponent = node ? std::move(node) : makeEmptyRow();
    _exponent->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e R o o t
// ════════════════════════════════════════════════════════════════════════════
NodeRoot::NodeRoot() : MathNode(NodeType::Root) {
    _radicand = makeEmptyRow();
    _radicand->setParent(this);
    _degree = nullptr;
}

NodeRoot::NodeRoot(NodePtr radicand, NodePtr degree)
    : MathNode(NodeType::Root)
{
    setRadicand(std::move(radicand));
    if (degree) setDegree(std::move(degree));
}

void NodeRoot::calculateLayout(const FontMetrics& fm) {
    // 1. Radicando (contenido bajo la raíz)
    _radicand->calculateLayout(fm);
    const auto& radL = _radicand->layout();

    // 2. Ancho del símbolo radical: gancho + pendiente ascendente
    int16_t radSymW = HOOK_W + SLOPE_W;

    // 3. Índice de raíz (si existe), fuente superscript
    if (_degree) {
        FontMetrics fmDeg = fm.superscript();
        _degree->calculateLayout(fmDeg);
        // El índice se coloca sobre el gancho; si es más ancho, ampliar
        radSymW = std::max(radSymW,
                           static_cast<int16_t>(_degree->layout().width + 2));
    }

    // 4. Ancho total: símbolo + contenido + padding derecho
    _layout.width = radSymW + radL.width + RIGHT_PAD;

    // 5. Altura:
    //    Sobre el baseline: contenido.ascent + gap + overline + margen
    _layout.ascent = radL.ascent + OVERLINE_GAP + OVERLINE_T + 1;

    //    Bajo el baseline: igual que el contenido
    _layout.descent = radL.descent;

    // 6. Si hay índice de raíz, su parte alta puede requerir más ascent
    if (_degree) {
        const auto& degL = _degree->layout();
        // El índice se coloca arriba-izquierda, con su fondo tocando
        // la mitad de la pendiente. Su tope puede superar el ascent.
        int16_t degTop = _layout.ascent - degL.descent + degL.ascent;
        _layout.ascent = std::max(_layout.ascent, degTop);
    }
}

int NodeRoot::childCount() const {
    return _degree ? 2 : 1;
}

MathNode* NodeRoot::child(int index) const {
    if (index == 0) return _radicand.get();
    if (index == 1 && _degree) return _degree.get();
    return nullptr;
}

void NodeRoot::setRadicand(NodePtr node) {
    _radicand = node ? std::move(node) : makeEmptyRow();
    _radicand->setParent(this);
}

void NodeRoot::setDegree(NodePtr node) {
    if (node) {
        _degree = std::move(node);
        _degree->setParent(this);
    } else {
        _degree.reset();
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e P a r e n
// ════════════════════════════════════════════════════════════════════════════
NodeParen::NodeParen() : MathNode(NodeType::Paren) {
    _content = makeEmptyRow();
    _content->setParent(this);
}

NodeParen::NodeParen(NodePtr content)
    : MathNode(NodeType::Paren)
{
    setContent(std::move(content));
}

void NodeParen::calculateLayout(const FontMetrics& fm) {
    _content->calculateLayout(fm);
    const auto& cl = _content->layout();

    // Ancho: paréntesis izquierdo + pad + contenido + pad + paréntesis derecho
    _layout.width = PAREN_W * 2 + INNER_PAD * 2 + cl.width;

    // Los paréntesis se estiran para cubrir todo el contenido + un poco más
    _layout.ascent  = cl.ascent  + VERT_PAD;
    _layout.descent = cl.descent + VERT_PAD;
}

MathNode* NodeParen::child(int index) const {
    if (index == 0) return _content.get();
    return nullptr;
}

void NodeParen::setContent(NodePtr node) {
    _content = node ? std::move(node) : makeEmptyRow();
    _content->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e F u n c t i o n
// ════════════════════════════════════════════════════════════════════════════
NodeFunction::NodeFunction()
    : MathNode(NodeType::Function), _kind(FuncKind::Sin), _labelWidth(0)
{
    _argument = makeEmptyRow();
    _argument->setParent(this);
}

NodeFunction::NodeFunction(FuncKind kind, NodePtr argument)
    : MathNode(NodeType::Function), _kind(kind), _labelWidth(0)
{
    setArgument(std::move(argument));
}

const char* NodeFunction::label() const {
    switch (_kind) {
        case FuncKind::Sin:    return "sin";
        case FuncKind::Cos:    return "cos";
        case FuncKind::Tan:    return "tan";
        case FuncKind::ArcSin: return "sin\xe2\x81\xbb\xc2\xb9";  // sin⁻¹
        case FuncKind::ArcCos: return "cos\xe2\x81\xbb\xc2\xb9";  // cos⁻¹
        case FuncKind::ArcTan: return "tan\xe2\x81\xbb\xc2\xb9";  // tan⁻¹
        case FuncKind::Ln:     return "ln";
        case FuncKind::Log:    return "log";
    }
    return "?";
}

void NodeFunction::calculateLayout(const FontMetrics& fm) {
    // 1. Etiqueta: ancho basado en charWidth × longitud del texto visible
    // Para funciones cortas (sin, cos, ln) es suficiente
    const char* lbl = label();
    int labelChars = 0;
    // Contar caracteres visibles (no bytes UTF-8)
    const char* p = lbl;
    while (*p) {
        if ((*p & 0xC0) != 0x80) labelChars++;  // No es byte de continuación
        p++;
    }
    _labelWidth = static_cast<int16_t>(fm.charWidth * labelChars);

    // 2. Argumento entre paréntesis
    _argument->calculateLayout(fm);
    const auto& argL = _argument->layout();

    // 3. Ancho total: etiqueta + gap + ( + pad + argumento + pad + )
    _layout.width = _labelWidth + LABEL_GAP
                  + PAREN_W + INNER_PAD + argL.width + INNER_PAD + PAREN_W;

    // 4. Ascent/descent: máximo entre etiqueta y contenido + paren
    _layout.ascent  = std::max(fm.ascent,
                               static_cast<int16_t>(argL.ascent + VERT_PAD));
    _layout.descent = std::max(fm.descent,
                               static_cast<int16_t>(argL.descent + VERT_PAD));
}

MathNode* NodeFunction::child(int index) const {
    if (index == 0) return _argument.get();
    return nullptr;
}

void NodeFunction::setArgument(NodePtr node) {
    _argument = node ? std::move(node) : makeEmptyRow();
    _argument->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e L o g B a s e
// ════════════════════════════════════════════════════════════════════════════
NodeLogBase::NodeLogBase()
    : MathNode(NodeType::LogBase), _labelWidth(0)
{
    _base = makeEmptyRow();
    _base->setParent(this);
    _argument = makeEmptyRow();
    _argument->setParent(this);
}

NodeLogBase::NodeLogBase(NodePtr base, NodePtr argument)
    : MathNode(NodeType::LogBase), _labelWidth(0)
{
    setBase(std::move(base));
    setArgument(std::move(argument));
}

void NodeLogBase::calculateLayout(const FontMetrics& fm) {
    // 1. Etiqueta "log": 3 chars
    _labelWidth = fm.charWidth * 3;

    // 2. Subíndice (base) en fuente reducida
    FontMetrics fmSub = fm.superscript();   // misma escala que superscript
    _base->calculateLayout(fmSub);
    const auto& baseL = _base->layout();

    // 3. Argumento entre paréntesis
    _argument->calculateLayout(fm);
    const auto& argL = _argument->layout();

    // 4. Ancho total: "log" + base_subscript + gap + ( + pad + arg + pad + )
    _layout.width = _labelWidth + baseL.width + LABEL_GAP
                  + PAREN_W + INNER_PAD + argL.width + INNER_PAD + PAREN_W;

    // 5. Descenso del subíndice:
    //    El tope del subíndice queda a SUB_DROP_NUM/SUB_DROP_DEN del descent
    //    bajo el baseline. Similar inverso a NodePower.
    int16_t subDrop = (fm.descent * SUB_DROP_NUM) / SUB_DROP_DEN;
    // Si descent es cero o muy pequeño, usar un mínimo
    if (subDrop < 2) subDrop = 2;

    // El fondo del subíndice está a subDrop + baseL.descent bajo el baseline
    int16_t subBottom = subDrop + baseL.descent;

    // 6. Ascent: mayor entre texto normal y paréntesis
    _layout.ascent = std::max(fm.ascent,
                              static_cast<int16_t>(argL.ascent + VERT_PAD));
    // 7. Descent: mayor entre normal, subíndice, y paréntesis
    _layout.descent = std::max({fm.descent, subBottom,
                                static_cast<int16_t>(argL.descent + VERT_PAD)});
}

MathNode* NodeLogBase::child(int index) const {
    if (index == 0) return _base.get();
    if (index == 1) return _argument.get();
    return nullptr;
}

void NodeLogBase::setBase(NodePtr node) {
    _base = node ? std::move(node) : makeEmptyRow();
    _base->setParent(this);
}

void NodeLogBase::setArgument(NodePtr node) {
    _argument = node ? std::move(node) : makeEmptyRow();
    _argument->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e C o n s t a n t
// ════════════════════════════════════════════════════════════════════════════
NodeConstant::NodeConstant(ConstKind kind)
    : MathNode(NodeType::Constant), _kind(kind)
{
}

const char* NodeConstant::symbol() const {
    switch (_kind) {
        case ConstKind::Pi:   return "\xcf\x80";  // π (U+03C0)
        case ConstKind::E:    return "e";          // Euler's number (blue via drawConstant)
        case ConstKind::Imag: return "i";          // imaginary unit (blue via drawConstant)
    }
    return "?";
}

void NodeConstant::calculateLayout(const FontMetrics& fm) {
    // La constante ocupa exactamente 1 carácter de ancho
    _layout.width   = fm.charWidth;
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e V a r i a b l e
// ════════════════════════════════════════════════════════════════════════════
NodeVariable::NodeVariable(char name)
    : MathNode(NodeType::Variable), _name(name), _labelWidth(0)
{
}

const char* NodeVariable::label() const {
    switch (_name) {
        case '#': return "Ans";
        case '$': return "PreAns";
        case 'A': return "A";
        case 'B': return "B";
        case 'C': return "C";
        case 'D': return "D";
        case 'E': return "E";
        case 'F': return "F";
        case 'G': return "G";
        case 'H': return "H";
        case 'I': return "I";
        case 'J': return "J";
        case 'K': return "K";
        case 'L': return "L";
        case 'M': return "M";
        case 'N': return "N";
        case 'O': return "O";
        case 'P': return "P";
        case 'Q': return "Q";
        case 'R': return "R";
        case 'S': return "S";
        case 'T': return "T";
        case 'U': return "U";
        case 'V': return "V";
        case 'W': return "W";
        case 'X': return "X";
        case 'Y': return "Y";
        case 'Z': return "Z";
        case 'a': return "a";
        case 'b': return "b";
        case 'c': return "c";
        case 'd': return "d";
        case 'e': return "e";
        case 'f': return "f";
        case 'g': return "g";
        case 'h': return "h";
        case 'i': return "i";
        case 'j': return "j";
        case 'k': return "k";
        case 'l': return "l";
        case 'm': return "m";
        case 'n': return "n";
        case 'o': return "o";
        case 'p': return "p";
        case 'q': return "q";
        case 'r': return "r";
        case 's': return "s";
        case 't': return "t";
        case 'u': return "u";
        case 'v': return "v";
        case 'w': return "w";
        case 'x': return "x";
        case 'y': return "y";
        case 'z': return "z";
        case '=': return "=";
        default:  return "?";
    }
}

void NodeVariable::calculateLayout(const FontMetrics& fm) {
    // Ancho = charWidth × longitud del label
    const char* lbl = label();
    int len = 0;
    while (lbl[len]) ++len;
    _labelWidth = fm.charWidth * static_cast<int16_t>(len);
    if (_labelWidth < fm.charWidth) _labelWidth = fm.charWidth;
    _layout.width   = _labelWidth;
    _layout.ascent  = fm.ascent;
    _layout.descent = fm.descent;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e P e r i o d i c D e c i m a l
// ════════════════════════════════════════════════════════════════════════════
NodePeriodicDecimal::NodePeriodicDecimal(const std::string& intPart,
                                         const std::string& nonRepeat,
                                         const std::string& repeat,
                                         bool negative)
    : MathNode(NodeType::PeriodicDecimal)
    , _intPart(intPart), _nonRepeat(nonRepeat)
    , _repeat(repeat), _negative(negative)
{
}

void NodePeriodicDecimal::calculateLayout(const FontMetrics& fm) {
    // Contar caracteres totales:
    //   [signo] + intPart + "." + nonRepeat + repeat
    int chars = 0;
    if (_negative) chars += 1;  // "-"
    chars += static_cast<int>(_intPart.size());
    if (!_nonRepeat.empty() || !_repeat.empty()) {
        chars += 1;  // "."
        chars += static_cast<int>(_nonRepeat.size());
        chars += static_cast<int>(_repeat.size());
    }
    if (chars == 0) chars = 1;

    _layout.width = fm.charWidth * static_cast<int16_t>(chars);

    // Si hay dígitos periódicos, necesitamos espacio extra encima para la overline
    if (!_repeat.empty()) {
        _layout.ascent = fm.ascent + OVERLINE_GAP + OVERLINE_T;
    } else {
        _layout.ascent = fm.ascent;
    }
    _layout.descent = fm.descent;
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e D e f I n t e g r a l
// ════════════════════════════════════════════════════════════════════════════
NodeDefIntegral::NodeDefIntegral()
    : MathNode(NodeType::DefIntegral)
    , _lower(makeEmptyRow())
    , _upper(makeEmptyRow())
    , _body(makeEmptyRow())
    , _variable(makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
    _variable->setParent(this);
}

NodeDefIntegral::NodeDefIntegral(NodePtr lower, NodePtr upper,
                                 NodePtr body, NodePtr variable)
    : MathNode(NodeType::DefIntegral)
    , _lower(lower ? std::move(lower) : makeEmptyRow())
    , _upper(upper ? std::move(upper) : makeEmptyRow())
    , _body(body ? std::move(body) : makeEmptyRow())
    , _variable(variable ? std::move(variable) : makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
    _variable->setParent(this);
}

MathNode* NodeDefIntegral::child(int index) const {
    switch (index) {
        case 0: return _lower.get();
        case 1: return _upper.get();
        case 2: return _body.get();
        case 3: return _variable.get();
        default: return nullptr;
    }
}

void NodeDefIntegral::setLower(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _lower = std::move(node);
    _lower->setParent(this);
}

void NodeDefIntegral::setUpper(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _upper = std::move(node);
    _upper->setParent(this);
}

void NodeDefIntegral::setBody(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _body = std::move(node);
    _body->setParent(this);
}

void NodeDefIntegral::setVariable(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _variable = std::move(node);
    _variable->setParent(this);
}

void NodeDefIntegral::calculateLayout(const FontMetrics& fm) {
    FontMetrics fmLimit = fm.superscript();

    _lower->calculateLayout(fmLimit);
    _upper->calculateLayout(fmLimit);
    _body->calculateLayout(fm);
    _variable->calculateLayout(fm);

    const auto& lowerL = _lower->layout();
    const auto& upperL = _upper->layout();
    const auto& bodyL  = _body->layout();
    const auto& varL   = _variable->layout();

    // Symbol column width = max(SYMBOL_W, lowerL.width, upperL.width)
    int16_t symColW = std::max({SYMBOL_W, lowerL.width, upperL.width});

    // "d" text width ≈ 1 char + variable
    int16_t dVarW = static_cast<int16_t>(fm.charWidth + D_GAP + varL.width);

    // Total width
    _layout.width = symColW + BODY_GAP + bodyL.width + D_GAP + dVarW;

    // Vertical: upper limit + gap + symbol/body + gap + lower limit
    int16_t bodyAscent  = std::max(bodyL.ascent, fm.ascent);
    int16_t bodyDescent = std::max(bodyL.descent, fm.descent);

    _layout.ascent  = bodyAscent + SYMBOL_H_PAD + LIMIT_GAP + upperL.height();
    _layout.descent = bodyDescent + SYMBOL_H_PAD + LIMIT_GAP + lowerL.height();
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e S u m m a t i o n
// ════════════════════════════════════════════════════════════════════════════
NodeSummation::NodeSummation()
    : MathNode(NodeType::Summation)
    , _lower(makeEmptyRow())
    , _upper(makeEmptyRow())
    , _body(makeEmptyRow())
    , _variable(makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
    _variable->setParent(this);
}

NodeSummation::NodeSummation(NodePtr lower, NodePtr upper,
                             NodePtr body, NodePtr variable)
    : MathNode(NodeType::Summation)
    , _lower(lower ? std::move(lower) : makeEmptyRow())
    , _upper(upper ? std::move(upper) : makeEmptyRow())
    , _body(body ? std::move(body) : makeEmptyRow())
    , _variable(variable ? std::move(variable) : makeEmptyRow())
{
    _lower->setParent(this);
    _upper->setParent(this);
    _body->setParent(this);
    _variable->setParent(this);
}

MathNode* NodeSummation::child(int index) const {
    switch (index) {
        case 0: return _lower.get();
        case 1: return _upper.get();
        case 2: return _body.get();
        case 3: return _variable.get();
        default: return nullptr;
    }
}

void NodeSummation::setLower(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _lower = std::move(node);
    _lower->setParent(this);
}

void NodeSummation::setUpper(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _upper = std::move(node);
    _upper->setParent(this);
}

void NodeSummation::setBody(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _body = std::move(node);
    _body->setParent(this);
}

void NodeSummation::setVariable(NodePtr node) {
    if (!node) node = makeEmptyRow();
    _variable = std::move(node);
    _variable->setParent(this);
}

void NodeSummation::calculateLayout(const FontMetrics& fm) {
    FontMetrics fmLimit = fm.superscript();

    _lower->calculateLayout(fmLimit);
    _upper->calculateLayout(fmLimit);
    _body->calculateLayout(fm);
    _variable->calculateLayout(fm);

    const auto& lowerL = _lower->layout();
    const auto& upperL = _upper->layout();
    const auto& bodyL  = _body->layout();

    // Symbol column width = max(SYMBOL_W, lowerL.width, upperL.width)
    int16_t symColW = std::max({SYMBOL_W, lowerL.width, upperL.width});

    // Total width
    _layout.width = symColW + BODY_GAP + bodyL.width;

    // Vertical: upper limit + gap + symbol/body + gap + lower limit
    int16_t bodyAscent  = std::max(bodyL.ascent, fm.ascent);
    int16_t bodyDescent = std::max(bodyL.descent, fm.descent);

    _layout.ascent  = bodyAscent + SYMBOL_H_PAD + LIMIT_GAP + upperL.height();
    _layout.descent = bodyDescent + SYMBOL_H_PAD + LIMIT_GAP + lowerL.height();
}

// ════════════════════════════════════════════════════════════════════════════
//  N o d e S u b s c r i p t
// ════════════════════════════════════════════════════════════════════════════
NodeSubscript::NodeSubscript() : MathNode(NodeType::Subscript) {
    _base = makeEmptyRow();
    _base->setParent(this);
    _subscript = makeEmptyRow();
    _subscript->setParent(this);
}

NodeSubscript::NodeSubscript(NodePtr base, NodePtr subscript)
    : MathNode(NodeType::Subscript)
{
    setBase(std::move(base));
    setSubscript(std::move(subscript));
}

void NodeSubscript::calculateLayout(const FontMetrics& fm) {
    // 1. Base en fuente normal
    _base->calculateLayout(fm);
    const auto& baseL = _base->layout();

    // 2. Subíndice en fuente reducida (same scale as superscript, ~70%)
    FontMetrics fmSub = fm.superscript();
    _subscript->calculateLayout(fmSub);
    const auto& subL = _subscript->layout();

    // 3. Ancho: base + subíndice (pegados, sin gap)
    _layout.width = baseL.width + subL.width;

    // 4. Descenso del subíndice:
    //    El tope del subíndice queda a SUB_DROP_NUM/SUB_DROP_DEN del descent
    //    bajo el baseline. (Mirrors NodeLogBase layout)
    int16_t subDrop = (fm.descent * SUB_DROP_NUM) / SUB_DROP_DEN;
    if (subDrop < 2) subDrop = 2;  // mínimo sensato

    // Reservar todo el alto del subíndice una vez bajado, no solo su descent.
    // Esto evita clipping y colisión con el siguiente carácter.
    int16_t subBottom = subDrop + subL.ascent + subL.descent;

    // 5. Ascent: de la base (el subíndice no sube)
    _layout.ascent = baseL.ascent;

    // 6. Descent: mayor entre base y subíndice
    _layout.descent = std::max(baseL.descent, subBottom);
}

MathNode* NodeSubscript::child(int index) const {
    if (index == 0) return _base.get();
    if (index == 1) return _subscript.get();
    return nullptr;
}

void NodeSubscript::setBase(NodePtr node) {
    _base = node ? std::move(node) : makeEmptyRow();
    _base->setParent(this);
}

void NodeSubscript::setSubscript(NodePtr node) {
    _subscript = node ? std::move(node) : makeEmptyRow();
    _subscript->setParent(this);
}

// ════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════
//  F a c t o r y   H e l p e r s
// ════════════════════════════════════════════════════════════════════════════
NodePtr makeRow() {
    return std::make_unique<NodeRow>();
}

NodePtr makeNumber(const std::string& value) {
    return std::make_unique<NodeNumber>(value);
}

NodePtr makeOperator(OpKind op) {
    return std::make_unique<NodeOperator>(op);
}

NodePtr makeEmpty() {
    return std::make_unique<NodeEmpty>();
}

NodePtr makeFraction(NodePtr num, NodePtr den) {
    if (num || den) {
        return std::make_unique<NodeFraction>(std::move(num), std::move(den));
    }
    return std::make_unique<NodeFraction>();   // Dos slots vacíos
}

NodePtr makePower(NodePtr base, NodePtr exp) {
    if (base || exp) {
        return std::make_unique<NodePower>(std::move(base), std::move(exp));
    }
    return std::make_unique<NodePower>();
}

NodePtr makeRoot(NodePtr radicand, NodePtr degree) {
    if (radicand || degree) {
        return std::make_unique<NodeRoot>(std::move(radicand), std::move(degree));
    }
    return std::make_unique<NodeRoot>();
}

NodePtr makeParen(NodePtr content) {
    if (content) {
        return std::make_unique<NodeParen>(std::move(content));
    }
    return std::make_unique<NodeParen>();
}

NodePtr makeFunction(FuncKind kind, NodePtr argument) {
    return std::make_unique<NodeFunction>(kind, std::move(argument));
}

NodePtr makeLogBase(NodePtr base, NodePtr argument) {
    if (base || argument) {
        return std::make_unique<NodeLogBase>(std::move(base), std::move(argument));
    }
    return std::make_unique<NodeLogBase>();
}

NodePtr makeConstant(ConstKind kind) {
    return std::make_unique<NodeConstant>(kind);
}

NodePtr makeVariable(char name) {
    return std::make_unique<NodeVariable>(name);
}

NodePtr makePeriodicDecimal(const std::string& intPart,
                            const std::string& nonRepeat,
                            const std::string& repeat,
                            bool negative) {
    return std::make_unique<NodePeriodicDecimal>(intPart, nonRepeat, repeat, negative);
}

NodePtr makeDefIntegral(NodePtr lower, NodePtr upper,
                        NodePtr body, NodePtr variable) {
    if (lower || upper || body || variable) {
        return std::make_unique<NodeDefIntegral>(std::move(lower), std::move(upper),
                                                 std::move(body), std::move(variable));
    }
    return std::make_unique<NodeDefIntegral>();
}

NodePtr makeSummation(NodePtr lower, NodePtr upper,
                      NodePtr body, NodePtr variable) {
    if (lower || upper || body || variable) {
        return std::make_unique<NodeSummation>(std::move(lower), std::move(upper),
                                               std::move(body), std::move(variable));
    }
    return std::make_unique<NodeSummation>();
}

NodePtr makeSubscript(NodePtr base, NodePtr subscript) {
    if (base || subscript) {
        return std::make_unique<NodeSubscript>(std::move(base), std::move(subscript));
    }
    return std::make_unique<NodeSubscript>();
}

// ════════════════════════════════════════════════════════════════════════════
//  D e b u g   D u m p
// ════════════════════════════════════════════════════════════════════════════
static void appendIndent(std::string& out, int indent) {
    for (int i = 0; i < indent; ++i) out += "  ";
}

std::string dumpTree(const MathNode* node, int indent) {
    if (!node) return "(null)\n";

    std::string out;
    appendIndent(out, indent);

    const auto& l = node->layout();
    // Sufijo con métricas: [w×h  asc/desc]
    auto metrics = [&]() -> std::string {
        return "  [" + std::to_string(l.width) + "x" + std::to_string(l.height())
             + " a" + std::to_string(l.ascent) + "/d" + std::to_string(l.descent)
             + "]";
    };

    switch (node->type()) {
        case NodeType::Row: {
            out += "Row" + metrics() + "\n";
            auto* row = static_cast<const NodeRow*>(node);
            for (int i = 0; i < row->childCount(); ++i) {
                out += dumpTree(row->child(i), indent + 1);
            }
            break;
        }
        case NodeType::Number: {
            auto* num = static_cast<const NodeNumber*>(node);
            out += "Number \"" + num->value() + "\"" + metrics() + "\n";
            break;
        }
        case NodeType::Operator: {
            auto* op = static_cast<const NodeOperator*>(node);
            out += std::string("Operator ") + op->symbol() + metrics() + "\n";
            break;
        }
        case NodeType::Empty: {
            out += "Empty \xe2\x96\xa1" + metrics() + "\n";  // □ U+25A1
            break;
        }
        case NodeType::Fraction: {
            out += "Fraction" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "num:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "den:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::Power: {
            out += "Power" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "base:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "exp:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::Root: {
            auto* root = static_cast<const NodeRoot*>(node);
            out += std::string("Root") + (root->hasDegree() ? "(n)" : "") + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "radicand:\n";
            out += dumpTree(node->child(0), indent + 2);
            if (root->hasDegree()) {
                appendIndent(out, indent + 1);
                out += "degree:\n";
                out += dumpTree(node->child(1), indent + 2);
            }
            break;
        }
        case NodeType::Paren: {
            out += "Paren" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "content:\n";
            out += dumpTree(node->child(0), indent + 2);
            break;
        }
        case NodeType::Function: {
            auto* func = static_cast<const NodeFunction*>(node);
            out += std::string("Function ") + func->label() + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "arg:\n";
            out += dumpTree(node->child(0), indent + 2);
            break;
        }
        case NodeType::LogBase: {
            out += "LogBase" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "base:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "arg:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
        case NodeType::Constant: {
            auto* cnst = static_cast<const NodeConstant*>(node);
            out += std::string("Constant ") + cnst->symbol() + metrics() + "\n";
            break;
        }
        case NodeType::Variable: {
            auto* vr = static_cast<const NodeVariable*>(node);
            out += std::string("Variable \"") + vr->label() + "\"" + metrics() + "\n";
            break;
        }
        case NodeType::PeriodicDecimal: {
            auto* pd = static_cast<const NodePeriodicDecimal*>(node);
            out += "PeriodicDecimal " + pd->intPart() + "."
                 + pd->nonRepeat() + "[" + pd->repeat() + "]"
                 + metrics() + "\n";
            break;
        }
        case NodeType::DefIntegral: {
            out += "DefIntegral" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "lower:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "upper:\n";
            out += dumpTree(node->child(1), indent + 2);
            appendIndent(out, indent + 1);
            out += "body:\n";
            out += dumpTree(node->child(2), indent + 2);
            appendIndent(out, indent + 1);
            out += "var:\n";
            out += dumpTree(node->child(3), indent + 2);
            break;
        }
        case NodeType::Summation: {
            out += "Summation" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "lower:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "upper:\n";
            out += dumpTree(node->child(1), indent + 2);
            appendIndent(out, indent + 1);
            out += "body:\n";
            out += dumpTree(node->child(2), indent + 2);
            appendIndent(out, indent + 1);
            out += "var:\n";
            out += dumpTree(node->child(3), indent + 2);
            break;
        }
        case NodeType::Subscript: {
            out += "Subscript" + metrics() + "\n";
            appendIndent(out, indent + 1);
            out += "base:\n";
            out += dumpTree(node->child(0), indent + 2);
            appendIndent(out, indent + 1);
            out += "sub:\n";
            out += dumpTree(node->child(1), indent + 2);
            break;
        }
    }

    return out;
}

// ════════════════════════════════════════════════════════════════════════════
// cloneNode() — Deep clone recursivo del AST
// ════════════════════════════════════════════════════════════════════════════

NodePtr cloneNode(const MathNode* node) {
    if (!node) return nullptr;

    switch (node->type()) {
        case NodeType::Row: {
            auto* row = static_cast<const NodeRow*>(node);
            auto out = makeRow();
            auto* outRow = static_cast<NodeRow*>(out.get());
            for (int i = 0; i < row->childCount(); ++i) {
                outRow->appendChild(cloneNode(row->child(i)));
            }
            return out;
        }
        case NodeType::Number: {
            auto* num = static_cast<const NodeNumber*>(node);
            return makeNumber(num->value());
        }
        case NodeType::Operator: {
            auto* op = static_cast<const NodeOperator*>(node);
            return makeOperator(op->op());
        }
        case NodeType::Empty:
            return makeEmpty();

        case NodeType::Fraction: {
            auto* frac = static_cast<const NodeFraction*>(node);
            return makeFraction(cloneNode(frac->numerator()),
                                cloneNode(frac->denominator()));
        }
        case NodeType::Power: {
            auto* pow = static_cast<const NodePower*>(node);
            return makePower(cloneNode(pow->base()),
                             cloneNode(pow->exponent()));
        }
        case NodeType::Root: {
            auto* root = static_cast<const NodeRoot*>(node);
            return makeRoot(cloneNode(root->radicand()),
                            root->hasDegree() ? cloneNode(root->degree()) : nullptr);
        }
        case NodeType::Paren: {
            auto* paren = static_cast<const NodeParen*>(node);
            return makeParen(cloneNode(paren->content()));
        }
        case NodeType::Function: {
            auto* fn = static_cast<const NodeFunction*>(node);
            return makeFunction(fn->funcKind(), cloneNode(fn->argument()));
        }
        case NodeType::LogBase: {
            auto* lb = static_cast<const NodeLogBase*>(node);
            return makeLogBase(cloneNode(lb->base()), cloneNode(lb->argument()));
        }
        case NodeType::Constant: {
            auto* cnst = static_cast<const NodeConstant*>(node);
            return makeConstant(cnst->constKind());
        }
        case NodeType::Variable: {
            auto* var = static_cast<const NodeVariable*>(node);
            return makeVariable(var->name());
        }
        case NodeType::PeriodicDecimal: {
            auto* pd = static_cast<const NodePeriodicDecimal*>(node);
            return makePeriodicDecimal(pd->intPart(), pd->nonRepeat(),
                                       pd->repeat(), pd->isNegative());
        }
        case NodeType::DefIntegral: {
            auto* di = static_cast<const NodeDefIntegral*>(node);
            return makeDefIntegral(cloneNode(di->lower()), cloneNode(di->upper()),
                                   cloneNode(di->body()), cloneNode(di->variable()));
        }
        case NodeType::Summation: {
            auto* sm = static_cast<const NodeSummation*>(node);
            return makeSummation(cloneNode(sm->lower()), cloneNode(sm->upper()),
                                 cloneNode(sm->body()), cloneNode(sm->variable()));
        }
        case NodeType::Subscript: {
            auto* sub = static_cast<const NodeSubscript*>(node);
            return makeSubscript(cloneNode(sub->base()),
                                 cloneNode(sub->subscript()));
        }
    }
    return nullptr;  // unreachable
}

} // namespace vpam
