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

#include "EquationsApp.h"
#include "../input/KeyboardManager.h"
#include "../input/KeySemanticResolver.h"
#include "../ui/MathTypography.h"
#include "../utils/HwUxProbe.h"
#include "../math/AngleModeRuntime.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string_view>
#include <chrono>

extern bool setting_complex_enabled;
using namespace vpam;
namespace {
constexpr int W = 320, CONTENT_Y = 54, CONTENT_H = 166;
constexpr uint32_t ACCENT = 0xE05500, FOCUS = 0x4A90D9;
constexpr const char* TEMPLATES[] = {"Blank equation", "Polynomial", "Exponential", "Logarithmic"};
// Fixed widgets never enter the scroll area: header 0..24, title 28..49,
// content 54..219, footer 225..237. Math retains its normal STIX metrics.
lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h) {
    auto* obj = lv_obj_create(parent);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_pad_all(obj,0,0); lv_obj_set_style_border_width(obj,0,0);
    lv_obj_set_style_radius(obj,4,0); lv_obj_set_style_bg_color(obj,lv_color_white(),0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
bool navigation(KeyCode c) { return c==KeyCode::LEFT || c==KeyCode::RIGHT || c==KeyCode::UP || c==KeyCode::DOWN; }
bool isEquality(const MathNode* node) {
    return node && ((node->type()==NodeType::Variable &&
                    static_cast<const NodeVariable*>(node)->name()=='=') ||
                   (node->type()==NodeType::Operator &&
                    static_cast<const NodeOperator*>(node)->op()==OpKind::Eq));
}
bool mayHaveDomainConditions(const MathNode* node, int depth=0) {
    if (!node || depth>28) return true;
    if (node->type()==NodeType::Fraction || node->type()==NodeType::Function ||
        node->type()==NodeType::Root || node->type()==NodeType::LogBase || node->type()==NodeType::Power) return true;
    if (node->type()==NodeType::Operator && static_cast<const NodeOperator*>(node)->op()==OpKind::Div) return true;
    for(int i=0;i<node->childCount();++i) if(mayHaveDomainConditions(node->child(i),depth+1)) return true;
    return false;
}
bool hasDecimal(const numos::EngineResultNode& node, int depth=0) {
    if(depth>28 || node.kind==numos::EngineNodeKind::Decimal) return true;
    for(const auto& child:node.children) if(hasDecimal(child,depth+1)) return true;
    return false;
}
bool needsTextDisplay(const numos::EngineResultNode& node, int depth=0) {
    if(depth>28) return true;
    // The shared converter treats Giac's generic division as a function call
    // "/(a,b)". Preserve the engine's exact text instead of mis-typesetting it.
    if(node.kind==numos::EngineNodeKind::Function && node.text=="/") return true;
    for(const auto& child:node.children) if(needsTextDisplay(child,depth+1)) return true;
    return false;
}
NodePtr resultFormula(const numos::EngineResultNode& node, int depth=0,
                      numos::ProductNotation notation=numos::ProductNotation::ScalarNatural) {
    if(depth>28) return nullptr;
    if(node.kind==numos::EngineNodeKind::Equation && node.children.size()==2) {
        auto lhs=resultFormula(node.children[0],depth+1,notation),rhs=resultFormula(node.children[1],depth+1,notation);
        if(!lhs||!rhs)return nullptr;
        // WHY: a display sub-row beginning with unary minus inherits BINARY
        // from the legacy AST. REL/BINARY is a forbidden TeX pair and can
        // overlap '=' with '-'. Authentic parentheses give this signed
        // subformula its existing OPEN/CLOSE classes without duplicating any
        // layout geometry or changing the mathematical state.
        auto signedSubformula=[](NodePtr& value) {
            if(value->type()!=NodeType::Row || !value->childCount())return;
            auto* first=value->child(0);
            if(first->type()==NodeType::Operator) {
                const auto op=static_cast<NodeOperator*>(first)->op();
                if(op==OpKind::Sub||op==OpKind::Add)value=makeParen(std::move(value));
            }
        };
        signedSubformula(lhs);signedSubformula(rhs);
        auto row=makeRow();auto* r=static_cast<NodeRow*>(row.get());
        r->appendChild(std::move(lhs));r->appendChild(makeRelation(OpKind::Eq));r->appendChild(std::move(rhs));
        return row;
    }
    if(node.kind==numos::EngineNodeKind::Function && node.text=="/" && node.children.size()==2) {
        // Presentation only: the authoritative structured division maps to a
        // VPAM fraction. No parsing of printed text or arithmetic is involved.
        auto numerator=resultFormula(node.children[0],depth+1,notation);
        auto denominator=resultFormula(node.children[1],depth+1,notation);
        if(!numerator || !denominator) return nullptr;
        return makeFraction(std::move(numerator),std::move(denominator));
    }
    return needsTextDisplay(node)?nullptr:numos::CalculationEngine::resultTreeToAST(node,notation);
}
}

#include "TutorPresentation.inc"

lv_obj_t* EquationsApp::text(lv_obj_t* parent, const char* value, int x, int y, int width) {
    auto* label=lv_label_create(parent);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_12,0);
    lv_obj_set_style_text_color(label,lv_color_hex(0x333333),0);
    lv_obj_set_pos(label,x,y);
    if (width) { lv_obj_set_width(label,width); lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP); }
    lv_label_set_text(label,value); return label;
}
void EquationsApp::header(const char* title, const char* hint) {
    lv_label_set_text(_title,title); lv_label_set_text(_hint,hint);
    _statusBar.setTitle("Equations"); _statusBar.update();
}
void EquationsApp::begin() {
    if (_screen) return;
    _screen=box(nullptr,0,0,320,240);
    _statusBar.create(_screen); _statusBar.setTitle("Equations"); _statusBar.setBatteryLevel(100);
    _title=text(_screen,"",8,30,304);
    _body=box(_screen,6,CONTENT_Y,308,CONTENT_H);
    lv_obj_add_flag(_body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_body,LV_DIR_VER); lv_obj_set_scrollbar_mode(_body,LV_SCROLLBAR_MODE_AUTO);
    _hint=text(_screen,"",6,225,308);
    lv_obj_set_style_text_font(_hint,&lv_font_montserrat_10,0);
    showEqList();
}
void EquationsApp::load() {
    begin(); lv_screen_load_anim(_screen,LV_SCREEN_LOAD_ANIM_FADE_IN,200,0,false); _statusBar.update();
}
void EquationsApp::clearView() {
    closeVariables();
    // WHY: LVGL callbacks and borrowed AST pointers die before any owning tree.
    for (auto& canvas : _canvas) { canvas.setExpression(nullptr,nullptr); canvas.destroy(); }
    for (auto& node : _viewNodes) node.reset();
    _rows.fill(nullptr); _followCursor=0; _stepProse=_stepConditions=nullptr; _stepLabels.fill(nullptr);
    if (_stepVerified) lv_obj_delete(_stepVerified);
    _stepVerified=nullptr; _stepFormulaCount=0;
    if (_title) lv_obj_set_width(_title,304);
    if (_body) { lv_obj_clean(_body); lv_obj_scroll_to(_body,0,0,LV_ANIM_OFF); }
}
void EquationsApp::end() {
    clearView(); _editCursor.init(nullptr); _editNode.reset(); _editRow=nullptr;
    for (int i=0;i<MAX_EQS;++i) { _eqNode[i].reset(); _eqRowData[i]=nullptr; }
    _giacResult={}; _derivation={}; _stepIndex=0; _teachingPage=0; _stepDetail=true;
    _statusBar.destroy();
    if (_screen) lv_obj_delete(_screen);
    _screen=_title=_body=_hint=nullptr;
    _numEquations=0; _listFocus=0; _editingIndex=-1; _page=0; _errorRow=-1;
    _equationEpoch=1; _solveEpoch=_stepsEpoch=0;
    _state=State::EQ_LIST; _resultKind=ResultKind::None; _tutorStatus=TutorStatus::Disabled;
    _tutorDiagnostic.clear();
    _tutorBuilds=0;
}
void EquationsApp::update() {
    if (!_screen) return;
    _statusBar.update();
    // The renderer has already calculated the cursor using its authoritative
    // geometry. Follow those bounds next tick; never duplicate slot formulas.
    if (_state==State::EDITING && _followCursor>0 && --_followCursor%2==0) {
        lv_area_t cursor{}, area{};
        if (_canvas[0].cursorBounds(cursor)) {
            lv_obj_get_coords(_body,&area);
            int delta=cursor.y1<area.y1+4 ? cursor.y1-area.y1-4 :
                      cursor.y2>area.y2-4 ? cursor.y2-area.y2+4 : 0;
            if (delta) lv_obj_scroll_to_y(_body,lv_obj_get_scroll_y(_body)+delta,LV_ANIM_OFF);
        }
    }
}
int EquationsApp::formula(int slot, NodeRow* row, int y, const char* label) {
    auto& canvas=_canvas[slot];
    auto* surface=box(_body,2,y,300,36); _rows[slot]=surface;
    lv_obj_set_style_bg_color(surface,lv_color_hex(0xF5F5F5),0);
    const int top=label?22:4;
    if (label) text(surface,label,8,4,284);
    canvas.create(surface); canvas.setAutoHeightEnabled(false); canvas.setExpression(row,nullptr);
    row->calculateLayout(canvas.normalMetrics());
    const int height=std::max(30,int(mathObjectHeightPx(row->layout(),canvas.normalMetrics(),8)));
    lv_obj_set_size(surface,300,height+top+4);
    lv_obj_set_pos(canvas.obj(),4,top); lv_obj_set_size(canvas.obj(),292,height);
    canvas.invalidate(); return height+top+10;
}
int EquationsApp::listItemCount() const { return _numEquations+(_numEquations<3)+(_numEquations>0); }
void EquationsApp::showEqList() {
    clearView(); _state=State::EQ_LIST;
    char title[80];
    std::snprintf(title,sizeof(title),"%d/3 equations  |  %s  |  %s",_numEquations,
        _numEquations<2?"x":_numEquations==2?"x, y":"x, y, z",setting_complex_enabled?"Complex":"Real");
    header(title,"EXE Select   DEL Remove   BACK Menu");
    int y=0;
    for(int i=0;i<_numEquations;++i) {
        char id[8]; std::snprintf(id,sizeof(id),"E%d",i+1);
        y+=formula(i,_eqRowData[i],y,id);
    }
    int idx=_numEquations;
    if (_numEquations<MAX_EQS) {
        _rows[idx]=box(_body,2,y,300,34); text(_rows[idx],"+ Add an equation",10,9); ++idx; y+=40;
    }
    if (_numEquations) {
        _rows[idx]=box(_body,2,y,300,34);
        auto* label=text(_rows[idx],_numEquations==1?"Solve equation":"Solve system",10,9);
        lv_obj_set_style_bg_color(_rows[idx],lv_color_hex(ACCENT),0);
        lv_obj_set_style_text_color(label,lv_color_white(),0);
    } else text(_body,"Choose a template or start blank.",10,y+10,280);
    updateFocus();
}
void EquationsApp::updateFocus() {
    const bool templates=_state==State::TEMPLATE;
    int& focus=templates?_templateFocus:_listFocus;
    const int count=templates?NUM_TEMPLATES:listItemCount();
    focus=std::clamp(focus,0,count-1);
    for(int i=0;i<count;++i) if(_rows[i]) {
        lv_obj_set_style_border_width(_rows[i],2,0);
        lv_obj_set_style_border_color(_rows[i],lv_color_hex(i==focus?FOCUS:0xEEEEEE),0);
        if(templates || i<_numEquations || (_numEquations<3 && i==_numEquations))
            lv_obj_set_style_bg_color(_rows[i],lv_color_hex(i==focus?0xE3F2FD:0xF5F5F5),0);
    }
    lv_obj_update_layout(_body);
    if (_rows[focus]) lv_obj_scroll_to_view(_rows[focus],LV_ANIM_OFF);
}
void EquationsApp::showTemplate() {
    clearView(); _state=State::TEMPLATE; _templateFocus=0;
    header("Choose a template","EXE Use   UP/DOWN Browse   BACK Cancel");
    int y=0;
    for(int i=0;i<NUM_TEMPLATES;++i) {
        // WHY: one constructor owns both preview and insertion semantics.
        _viewNodes[i]=buildTemplateAST(i);
        if(i) y+=formula(i,static_cast<NodeRow*>(_viewNodes[i].get()),y,TEMPLATES[i]);
        else { _rows[0]=box(_body,2,0,300,34); text(_rows[0],TEMPLATES[0],8,9); y=40; }
    }
    updateFocus();
}
void EquationsApp::showEditing(int index, int templateIndex) {
    clearView(); _state=State::EDITING; _editingIndex=index;
    _editCursor.init(nullptr);
    _editNode=templateIndex>=0?buildTemplateAST(templateIndex):cloneNode(_eqRowData[index]);
    _editRow=static_cast<NodeRow*>(_editNode.get()); _editCursor.init(_editRow);
    _draftTouched=false;
    char title[48]; std::snprintf(title,sizeof(title),"%s E%d",templateIndex>=0?"New equation":"Edit equation",index+1);
    header(title,"EXE Save   = Equality   VAR x/y/z   BACK Cancel");
    _canvas[0].create(_body); _canvas[0].setAutoHeightEnabled(false);
    lv_obj_set_style_bg_opa(_canvas[0].obj(),LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(_canvas[0].obj(),lv_color_hex(0xF5F9FE),0);
    lv_obj_set_style_border_width(_canvas[0].obj(),1,0);
    lv_obj_set_style_border_color(_canvas[0].obj(),lv_color_hex(FOCUS),0);
    refreshEditor(); _canvas[0].startCursorBlink();
}
void EquationsApp::refreshEditor() {
    _canvas[0].setExpression(_editRow,&_editCursor);
    _editRow->calculateLayout(_canvas[0].normalMetrics());
    int h=std::max(CONTENT_H-4,int(mathObjectHeightPx(_editRow->layout(),_canvas[0].normalMetrics(),16)));
    lv_obj_set_pos(_canvas[0].obj(),2,0); lv_obj_set_size(_canvas[0].obj(),300,h);
    // LVGL may draw after this tick. Four bounded checks allow the cached
    // cursor to settle after both the edit and the enclosing scroll change.
    _canvas[0].invalidate(); _canvas[0].resetCursorBlink(); _followCursor=8;
}
void EquationsApp::invalidateAnswer() {
    ++_equationEpoch; _solveEpoch=_stepsEpoch=0; _giacResult={}; _page=0;
    _resultKind=ResultKind::None; _tutorStatus=TutorStatus::Disabled;
    _derivation={}; _stepIndex=0; _teachingPage=0; _stepDetail=true; _tutorDiagnostic.clear();
}
void EquationsApp::confirmDraft() {
    // An explicitly saved incomplete row stays visible and is rejected at solve.
    // An untouched new blank draft is unused, so confirming it returns to Add.
    const int index=_editingIndex;
    if(index==_numEquations && _editRow->isEmpty() && !_draftTouched) { cancelDraft(); return; }
    clearView(); _editCursor.init(nullptr);
    _eqNode[index]=std::move(_editNode); _eqRowData[index]=static_cast<NodeRow*>(_eqNode[index].get());
    if(index==_numEquations) ++_numEquations;
    _editRow=nullptr; _editingIndex=-1; _listFocus=index;
    invalidateAnswer(); showEqList();
}
void EquationsApp::cancelDraft() {
    clearView(); _editCursor.init(nullptr); _editNode.reset(); _editRow=nullptr;
    _listFocus=std::min(_editingIndex,listItemCount()-1); _editingIndex=-1; showEqList();
}
void EquationsApp::closeVariables() {
    if(_variableMenu) lv_obj_delete(_variableMenu);
    _variableMenu=nullptr;
}
void EquationsApp::showVariables() {
    closeVariables(); _variableFocus=0;
    _variableMenu=box(_screen,42,78,236,114);
    lv_obj_set_style_border_width(_variableMenu,2,0);
    lv_obj_set_style_border_color(_variableMenu,lv_color_hex(FOCUS),0);
    text(_variableMenu,"Insert variable",10,8);
    for(int i=0;i<3;++i) { char v[2]={"xyz"[i],0}; auto* label=text(_variableMenu,v,30+70*i,38);
        lv_obj_set_style_text_color(label,lv_color_hex(i==0?FOCUS:0x333333),0); }
    text(_variableMenu,"LEFT/RIGHT   EXE   BACK",10,76);
}
void EquationsApp::handleEditor(const KeyEvent& ev) {
    auto& cc=_editCursor; auto& km=KeyboardManager::instance();
    using numos::input::SemanticId;
    const auto semantic=static_cast<SemanticId>(ev.semanticId);
    bool changed=true;
    if(semantic>=SemanticId::alpha_A && semantic<=SemanticId::alpha_Z)
        cc.insertVariable('A'+int(semantic)-int(SemanticId::alpha_A));
    else if(semantic==SemanticId::asin || semantic==SemanticId::acos || semantic==SemanticId::atan)
        cc.insertFunction(semantic==SemanticId::asin?FuncKind::ArcSin:semantic==SemanticId::acos?FuncKind::ArcCos:FuncKind::ArcTan);
    else if(semantic==SemanticId::pow_e) { cc.insertConstant(ConstKind::E); cc.insertPower(); }
    else if(semantic==SemanticId::pow10) { cc.insertDigit('1'); cc.insertDigit('0'); cc.insertPower(); }
    else if(semantic==SemanticId::absolute_value) {
        const auto cursor=cc.cursor();
        cursor.row->insertChild(cursor.index,makeParen(nullptr,DelimKind::Bar));
        cc.moveRight();
    }
    else if(semantic==SemanticId::nth_root_template) {
        cc.insertRoot();
        auto* root=static_cast<NodeRoot*>(cc.cursor().row->parent());
        auto degree=makeRow(); static_cast<NodeRow*>(degree.get())->appendChild(makeEmpty());
        root->setDegree(std::move(degree)); cc.focusSlot(static_cast<NodeRow*>(root->degree()));
    }
    else {
        const int digit=keyCodeDigitValue(ev.code);
        if(digit>=0) cc.insertDigit('0'+digit);
        else switch(ev.code) {
            case KeyCode::LEFT: cc.moveLeft(); break;
            case KeyCode::RIGHT:
            case KeyCode::UP:
            case KeyCode::DOWN: {
                const auto cursor=cc.cursor(); auto* parent=cursor.row?cursor.row->parent():nullptr;
                auto* root=parent && parent->type()==NodeType::Root?static_cast<NodeRoot*>(parent):nullptr;
                if(root && root->hasDegree() && ev.code==KeyCode::UP)
                    cc.focusSlot(static_cast<NodeRow*>(root->degree()));
                else if(root && root->hasDegree() && (ev.code==KeyCode::DOWN || (cursor.row==root->degree() && cursor.atEnd())))
                    cc.focusSlot(static_cast<NodeRow*>(root->radicand()));
                else if(ev.code==KeyCode::RIGHT) cc.moveRight();
                else if(ev.code==KeyCode::UP) cc.moveUp(); else cc.moveDown();
                break;
            }
            case KeyCode::DOT: cc.insertDigit('.'); break;
            case KeyCode::ADD: cc.insertOperator(OpKind::Add); break;
            case KeyCode::SUB: case KeyCode::NEG: case KeyCode::NEGATE: cc.insertOperator(OpKind::Sub); break;
            case KeyCode::MUL: cc.insertOperator(OpKind::Mul); break;
            case KeyCode::DIVIDE: cc.insertOperator(OpKind::Div); break;
            case KeyCode::FRAC: case KeyCode::DIV: cc.insertFraction(); break;
            case KeyCode::POW: cc.insertPower(); break;
            case KeyCode::SQUARE: cc.insertPower(); cc.insertDigit('2'); cc.moveRight(); break;
            case KeyCode::SQRT: cc.insertRoot(); break;
            case KeyCode::LPAREN: cc.insertParen(); break;
            case KeyCode::RPAREN: cc.moveRight(); break;
            case KeyCode::VAR_X: cc.insertVariable('x'); break;
            case KeyCode::VAR_Y: cc.insertVariable(semantic==SemanticId::none && km.isAlpha()?'z':'y'); break;
            case KeyCode::VAR: showVariables(); return;
            case KeyCode::SIN: cc.insertFunction(semantic==SemanticId::none && km.isShift()?FuncKind::ArcSin:FuncKind::Sin); break;
            case KeyCode::COS: cc.insertFunction(semantic==SemanticId::none && km.isShift()?FuncKind::ArcCos:FuncKind::Cos); break;
            case KeyCode::TAN: cc.insertFunction(semantic==SemanticId::none && km.isShift()?FuncKind::ArcTan:FuncKind::Tan); break;
            case KeyCode::LN: cc.insertFunction(FuncKind::Ln); break;
            case KeyCode::LOG: cc.insertFunction(FuncKind::Log); break;
            case KeyCode::LOG_BASE: cc.insertLogBase(); break;
            case KeyCode::CONST_PI: cc.insertConstant(ConstKind::Pi); break;
            case KeyCode::CONST_E: cc.insertConstant(ConstKind::E); break;
            case KeyCode::EXP:
                cc.insertOperator(OpKind::Mul); cc.insertDigit('1'); cc.insertDigit('0'); cc.insertPower(); break;
            case KeyCode::EQUAL: case KeyCode::FREE_EQ: cc.insertOperator(OpKind::Eq); break;
            case KeyCode::DEL: cc.backspace(); break;
            case KeyCode::AC:
                _canvas[0].setExpression(nullptr,nullptr); cc.init(nullptr);
                _editNode=makeRow(); _editRow=static_cast<NodeRow*>(_editNode.get()); cc.init(_editRow); break;
            case KeyCode::EXE: case KeyCode::ENTER: confirmDraft(); return;
            default: changed=false; break;
        }
    }
    // WHY: resolved events have already consumed precisely their active plane.
    // Legacy serial aliases still own consumption here; navigation preserves it.
    if(changed && semantic==SemanticId::none && !navigation(ev.code)) km.consumeModifier();
    if(changed) {
        if(!navigation(ev.code)) _draftTouched=true;
        refreshEditor();
    }
}
void EquationsApp::handleKey(const KeyEvent& ev) {
    if(ev.action!=KeyAction::PRESS && ev.action!=KeyAction::REPEAT) return;
    if(ev.action==KeyAction::REPEAT && !navigation(ev.code) && !(ev.code==KeyCode::DEL && _state==State::EDITING && !_variableMenu)) return;
    if(ev.code==KeyCode::SHIFT || ev.code==KeyCode::ALPHA) {
        auto& km=KeyboardManager::instance();
        if(ev.code==KeyCode::SHIFT) km.pressShift(); else km.pressAlpha();
        _statusBar.update(); return;
    }
    if(ev.code==KeyCode::BACK) { navigateBack(); return; }
    if(_variableMenu) {
        if(ev.code==KeyCode::LEFT || ev.code==KeyCode::UP) _variableFocus=std::max(0,_variableFocus-1);
        if(ev.code==KeyCode::RIGHT || ev.code==KeyCode::DOWN) _variableFocus=std::min(2,_variableFocus+1);
        if(ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER) {
            _editCursor.insertVariable("xyz"[_variableFocus]); _draftTouched=true;
            closeVariables(); refreshEditor(); return;
        }
        if(ev.code==KeyCode::AC) { closeVariables(); return; }
        for(int i=0;i<3;++i) lv_obj_set_style_text_color(lv_obj_get_child(_variableMenu,i+1),lv_color_hex(i==_variableFocus?FOCUS:0x333333),0);
        return;
    }
    switch(_state) {
        case State::EQ_LIST:
            if(ev.code==KeyCode::UP || ev.code==KeyCode::DOWN) {
                lv_area_t row{},view{};
                lv_obj_get_coords(_rows[_listFocus],&row); lv_obj_get_coords(_body,&view);
                const bool down=ev.code==KeyCode::DOWN;
                // A single tall row can exceed the viewport; traverse its
                // remaining content before advancing to the next list action.
                if(lv_obj_get_height(_rows[_listFocus])>CONTENT_H &&
                   (down ? row.y2>view.y2 : row.y1<view.y1))
                    lv_obj_scroll_by(_body,0,down?-28:28,LV_ANIM_OFF);
                else { _listFocus+=down?1:-1; updateFocus(); }
            }
            else if(ev.code==KeyCode::LEFT || ev.code==KeyCode::RIGHT) {
                if(_listFocus<_numEquations) _canvas[_listFocus].scrollBounded(ev.code==KeyCode::LEFT?24:-24);
            } else if(ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER) {
                if(_listFocus<_numEquations) showEditing(_listFocus);
                else if(_numEquations<3 && _listFocus==_numEquations) showTemplate();
                else solveEquations();
            } else if(ev.code==KeyCode::DEL && _listFocus<_numEquations) {
                clearView();
                for(int i=_listFocus;i<_numEquations-1;++i) { _eqNode[i]=std::move(_eqNode[i+1]); _eqRowData[i]=static_cast<NodeRow*>(_eqNode[i].get()); }
                --_numEquations; _eqNode[_numEquations].reset(); _eqRowData[_numEquations]=nullptr;
                invalidateAnswer(); showEqList();
            }
            break;
        case State::TEMPLATE:
            if(ev.code==KeyCode::UP) { --_templateFocus; updateFocus(); }
            else if(ev.code==KeyCode::DOWN) { ++_templateFocus; updateFocus(); }
            else if(ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER) showEditing(_numEquations,_templateFocus);
            else if(ev.code==KeyCode::AC || ev.code==KeyCode::DEL) showEqList();
            break;
        case State::EDITING: handleEditor(ev); break;
        case State::STEPS:
            if(ev.code==KeyCode::LEFT || ev.code==KeyCode::RIGHT) {
                const auto count=numos::tutor::teachingPageCount(_derivation,_stepDetail);
                if(count) {
                    _teachingPage=unsigned(std::clamp(int(_teachingPage)+(ev.code==KeyCode::RIGHT?1:-1),0,int(count)-1));
                    drawStep();
                }
            } else if(ev.code==KeyCode::UP || ev.code==KeyCode::DOWN)
                scrollTeaching(ev.code==KeyCode::UP?-28:28);
            else if(ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER) {
                const auto current=numos::tutor::teachingPageAt(_derivation,_stepDetail,_teachingPage);
                _stepDetail=!_stepDetail;
                _teachingPage=numos::tutor::teachingPageFor(_derivation,_stepDetail,current);
                drawStep(true);
            } else if(ev.code==KeyCode::VAR) {
                // Cycle each wide formula back to its start after its last
                // segment. Page arrows retain their established meaning.
                for(auto& canvas:_canvas)if(canvas.obj()&&!canvas.scrollBounded(-24))canvas.scrollBounded(10000);
            } else if(ev.code==KeyCode::AC || ev.code==KeyCode::DEL) navigateBack();
            break;
        case State::RESULT:
            if(navigation(ev.code)) {
                if(ev.code==KeyCode::UP || ev.code==KeyCode::DOWN) lv_obj_scroll_by(_body,0,ev.code==KeyCode::UP?28:-28,LV_ANIM_OFF);
                else for(auto& canvas:_canvas) if(canvas.obj()) canvas.scrollBounded(ev.code==KeyCode::LEFT?24:-24);
            } else if(_state==State::RESULT && (ev.code==KeyCode::TOOLBOX || ev.code==KeyCode::SHOW_STEPS)) showSteps();
            else if(_state==State::RESULT && ev.code==KeyCode::VAR && _giacResult.groups.size()>1) {
                _page=(_page+1)%_giacResult.groups.size(); showResult();
            } else if(ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER || ev.code==KeyCode::AC || ev.code==KeyCode::DEL) navigateBack();
            break;
        case State::SOLVING: break;
    }
    _statusBar.update();
}
bool EquationsApp::navigateBack() {
    if(_variableMenu) { closeVariables(); return true; }
    switch(_state) {
        case State::TEMPLATE: showEqList(); return true;
        case State::EDITING: cancelDraft(); return true;
        case State::RESULT: showEqList(); return true;
        case State::STEPS: showResult(); return true;
        case State::SOLVING: return true; // synchronous; dispatch resumes after solve
        case State::EQ_LIST: return false;
    }
    return false;
}
void EquationsApp::solveEquations() {
    numos::HwUxProbe probe("equations","solve");
    clearView(); _solveEpoch=_stepsEpoch=0; _page=0; _errorRow=-1;
    _giacResult={}; _resultKind=ResultKind::None; _tutorStatus=TutorStatus::Unavailable;
    _derivation={}; _stepIndex=0; _teachingPage=0; _stepDetail=true; _tutorDiagnostic.clear();
    _state=State::SOLVING; header("Solving with Giac...","Please wait"); lv_refr_now(nullptr);
    std::vector<numos::SolveEquation> equations;
    equations.reserve(_numEquations);
    for(int i=0;i<_numEquations;++i) {
        NodePtr lhs,rhs; numos::SolveEquation eq; std::string error;
        if(!splitAtEquals(_eqRowData[i],lhs,rhs)) error="Use one equality with both sides filled.";
        else if(!numos::CalculationEngine::serializeForGiac(lhs.get(),eq.lhs,error) ||
                !numos::CalculationEngine::serializeForGiac(rhs.get(),eq.rhs,error)) { if(error.empty()) error="Incomplete equation."; }
        if(!error.empty()) {
            _giacResult.status=numos::MathEngineStatus::ParseError;
            _giacResult.diagnostic="E"+std::to_string(i+1)+": "+error;
            _errorRow=i; _listFocus=i; showResult(); return;
        }
        equations.push_back(std::move(eq));
    }
    const auto policy=setting_complex_enabled?numos::SolveDomainPolicy::RealAndComplex:numos::SolveDomainPolicy::RealOnly;
    if(_numEquations==1) _giacResult=numos::GiacEngine::instance().solveStructured(equations[0],"x",policy);
    else {
        std::vector<std::string> vars={"x","y"}; if(_numEquations==3) vars.emplace_back("z");
        _giacResult=numos::GiacEngine::instance().solveSystemStructured(equations,vars,policy);
    }
    _solveEpoch=_equationEpoch;
    // The source/answer are independent owners. An unsupported tutor cannot
    // suppress or rewrite the ordinary result, including adapter refusals.
    numos::tutor::Snapshot snapshot;
    snapshot.inputEpoch=_equationEpoch;
    snapshot.engineGeneration=numos::GiacEngine::instance().generation();
    snapshot.complex=setting_complex_enabled;
    snapshot.degrees=numos::angleModeIsDeg();
    for(const auto& eq:equations) snapshot.authored.push_back({eq.lhs,eq.rhs});
    for(int i=0;i<_numEquations;++i) snapshot.variables.push_back(std::string(1,"xyz"[i]));
    ++_tutorBuilds;
    _derivation=numos::GiacEngine::instance().explainEquations(snapshot,_giacResult);
    _tutorStatus=_derivation.status==numos::tutor::Status::Complete?TutorStatus::Complete:TutorStatus::Unavailable;
    showResult(); probe.finish(_giacResult.ok()?"ok":"error","result","giac",0);
}
void EquationsApp::showResult() {
    clearView(); _state=State::RESULT; _resultKind=ResultKind::None;
    header("Result","Arrows Scroll   EXE Equations   TOOLBOX Steps");
    auto info=[&](const char* title,const std::string& message) {
        lv_label_set_text(_title,title);
        // A pathological printed value must not exhaust the firmware LVGL pool.
        // This is a presentation limit, never a truncation of the engine result.
        if(message.size()>3072) {
            const auto limited=message.substr(0,3072)+"\n[Display limit: remaining text is not shown.]";
            text(_body,limited.c_str(),8,8,284);
        } else text(_body,message.c_str(),8,8,284);
    };
    if(!_giacResult.ok()) {
        info(_giacResult.status==numos::MathEngineStatus::ParseError?"Invalid equation":
             _giacResult.status==numos::MathEngineStatus::Unsupported?"Unresolved / unsupported":
             _giacResult.status==numos::MathEngineStatus::OutOfMemory?"Not enough memory":"Solve failed",_giacResult.diagnostic);
        return;
    }
    _resultKind=ResultKind::Structured;
    if(_giacResult.setKind==numos::SolutionSetKind::NoSolution) {
        info("No solutions",setting_complex_enabled?"No solutions in the complex domain.":"No solutions in the real domain."); return;
    }
    if(_giacResult.setKind==numos::SolutionSetKind::AllValues) {
        // The original adapter conflates free variables and independent values.
        // Preserve its raw relation and avoid claiming an unrestricted set.
        if(_numEquations>1) {
            bool conditional=false;
            for(int i=0;i<_numEquations;++i) conditional=conditional || mayHaveDomainConditions(_eqRowData[i]);
            info(conditional?"Conditional family":"Dependent system / family",
                std::string(_numEquations==2?"Tuple order: x, y\n":"Tuple order: x, y, z\n")+
                _giacResult.rawExactText+"\nParameters must satisfy these relations."+
                (conditional?" Only where the original equations are defined; exclusions remain unresolved.":""));
        }
        else if(mayHaveDomainConditions(_eqRowData[0]))
            info("Conditional identity","Equality holds where the original expression is defined. Domain exclusions remain unresolved; this is not an unrestricted solution set.");
        else info("Identity / all values",setting_complex_enabled?"Every complex x satisfies the equation.":"Every real x satisfies the equation.");
        return;
    }
    if(_giacResult.groups.empty()) { _resultKind=ResultKind::TextFallback; info("Unresolved result",_giacResult.rawExactText); return; }
    bool numerical=false;
    for(const auto& group:_giacResult.groups) for(const auto& value:group.values)
        numerical=numerical || hasDecimal(value.exactValue);
    if(numerical) {
        _resultKind=ResultKind::TextFallback;
        std::string visible="Numerical candidates from Giac. Completeness is not established.\n";
        for(size_t i=0;i<_giacResult.groups.size();++i) {
            visible+="\nCandidate "+std::to_string(i+1)+":\n";
            for(const auto& value:_giacResult.groups[i].values) visible+=value.variable+" = "+value.exactText+"\n";
        }
        info("Numerical candidates",visible); return;
    }
    // A page is a complete solution group. No root or assignment is dropped by
    // the fixed widget budget; VAR advances through every engine-owned group.
    const bool system=_numEquations>1;
    const bool pages=system || _giacResult.groups.size()>MAX_RESULTS;
    char title[96];
    if(pages) std::snprintf(title,sizeof(title),"Solution %d/%u  |  %s",_page+1,unsigned(_giacResult.groups.size()),setting_complex_enabled?"Complex":"Real");
    else std::snprintf(title,sizeof(title),"%u %s  |  x  |  %s",unsigned(_giacResult.groups.size()),_giacResult.groups.size()==1?"solution":"solutions",setting_complex_enabled?"Complex":"Real");
    header(title,pages && _giacResult.groups.size()>1?"VAR Next set   Arrows Scroll   BACK Equations":"Arrows Scroll   EXE Equations   TOOLBOX Steps");
    int slot=0,y=0;
    const size_t first=pages?size_t(_page):0, last=pages?first+1:_giacResult.groups.size();
    for(size_t g=first;g<last;++g) {
        for(const auto& solution:_giacResult.groups[g].values) {
            auto value=resultFormula(solution.exactValue);
            if(!value || slot>=MAX_RESULTS) {
                clearView(); _resultKind=ResultKind::TextFallback;
                std::string fallback;
                for(size_t j=0;j<_giacResult.groups.size();++j) {
                    fallback+="Solution "+std::to_string(j+1)+":\n";
                    for(const auto& v:_giacResult.groups[j].values) fallback+=v.variable+" = "+v.exactText+"\n";
                }
                info("Exact results (text)",fallback); return;
            }
            auto row=makeRow(); auto* r=static_cast<NodeRow*>(row.get());
            r->appendChild(std::move(value));
            // WHY: assignment labels remain readable during horizontal math scroll.
            const std::string label=solution.variable+" =";
            _viewNodes[slot]=std::move(row); y+=formula(slot,r,y,label.c_str()); ++slot;
        }
    }
}
#include "TutorStepsView.inc"

#ifdef NATIVE_SIM
const char* EquationsApp::debugEngineName() const {
    return "giac";
}
const char* EquationsApp::debugStatusName() const {
    if (_giacResult.status == numos::MathEngineStatus::Ok) {
        if (_giacResult.setKind == numos::SolutionSetKind::NoSolution)
            return "no_solution";
        if (_giacResult.setKind == numos::SolutionSetKind::AllValues)
            return "all_values";
        if (_giacResult.setKind == numos::SolutionSetKind::Unsupported)
            return "unsupported";
        return "ok";
    }
    switch (_giacResult.status) {
        case numos::MathEngineStatus::ParseError: return "parse_error";
        case numos::MathEngineStatus::Undefined: return "undefined";
        case numos::MathEngineStatus::EvaluationError:
            return "evaluation_error";
        case numos::MathEngineStatus::OutOfMemory: return "out_of_memory";
        default: return "unsupported";
    }
}
int EquationsApp::debugSolutionCount() const {
    return static_cast<int>(_giacResult.groups.size());
}
bool EquationsApp::debugSolutionNear(const std::string& variable, int index,
                                     double expected,
                                     double epsilon) const {
    if (index < 0 || index >= static_cast<int>(_giacResult.groups.size()))
        return false;
    const auto& group = _giacResult.groups[static_cast<std::size_t>(index)];
    for (const auto& value : group.values) {
        if (value.variable != variable) continue;
        double actual = 0.0;
        if (value.hasApproximateReal) {
            actual = value.approximateReal;
        } else {
            vpam::ExactVal exact;
            if (!numos::CalculationEngine::resultTreeToExactVal(
                    value.exactValue, exact)) {
                return false;
            }
            actual = exact.toDouble();
        }
        return std::isfinite(actual) &&
               std::fabs(actual - expected) <= epsilon;
    }
    return false;
}
bool EquationsApp::debugSolutionExact(const std::string& variable, int index,
                                      const std::string& expected) const {
    if (index < 0 || index >= static_cast<int>(_giacResult.groups.size()))
        return false;
    const auto& group = _giacResult.groups[static_cast<std::size_t>(index)];
    for (const auto& value : group.values) {
        if (value.variable == variable) return value.exactText == expected;
    }
    return false;
}
std::string EquationsApp::debugSolutionExactText(
    const std::string& variable, int index) const {
    if (index < 0 || index >= static_cast<int>(_giacResult.groups.size()))
        return {};
    for (const auto& value :
         _giacResult.groups[static_cast<std::size_t>(index)].values) {
        if (value.variable == variable) return value.exactText;
    }
    return {};
}
const char* EquationsApp::debugResultKindName() const {
    switch (_resultKind) {
        case ResultKind::Structured: return "structured";
        case ResultKind::TextFallback: return "text_fallback";
        default: return "none";
    }
}
const char* EquationsApp::debugTutorStatusName() const {
    switch (_tutorStatus) {
        case TutorStatus::Complete: return "complete";
        case TutorStatus::Unavailable: return "unavailable";
        default: return "disabled";
    }
}
#endif
vpam::NodePtr EquationsApp::buildTemplateAST(int templateIdx) {
    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());

    switch (templateIdx) {
        case 0:  // Empty
            break;

        case 1:  // Polynomial: ax² + bx + c = 0
            r->appendChild(makeVariable('A'));
            r->appendChild(makePower(makeVariable('x'), makeNumber("2")));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('B'));
            r->appendChild(makeVariable('x'));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('C'));
            r->appendChild(makeRelation(OpKind::Eq));
            r->appendChild(makeNumber("0"));
            break;

        case 2: { // Exponential: a·e^x + b = 0
            r->appendChild(makeVariable('A'));
            r->appendChild(makePower(
                makeConstant(ConstKind::E), makeVariable('x')));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('B'));
            r->appendChild(makeRelation(OpKind::Eq));
            r->appendChild(makeNumber("0"));
            break;
        }

        case 3: { // Logarithmic: ln(x) + a = 0
            r->appendChild(makeFunction(FuncKind::Ln, makeVariable('x')));
            r->appendChild(makeOperator(OpKind::Add));
            r->appendChild(makeVariable('A'));
            r->appendChild(makeRelation(OpKind::Eq));
            r->appendChild(makeNumber("0"));
            break;
        }
    }

    return row;
}
bool EquationsApp::splitAtEquals(NodeRow* row,
                                  NodePtr& outLHS,
                                  NodePtr& outRHS) {
    outLHS.reset();
    outRHS.reset();
    if (!row) return false;

    int eqIdx = -1;
    int count = row->childCount();

    for (int i = 0; i < count; ++i) {
        const MathNode* ch = row->child(i);
        const bool isEquals = isEquality(ch);

        // WHY: authored equation rows must contain exactly one complete
        // equality. Treating a missing side as zero fabricated input and made
        // template relations take a different route from the '=' key.
        if (isEquals) {
            if (eqIdx >= 0) return false;
            eqIdx = i;
        }
    }

    if (eqIdx <= 0 || eqIdx >= count - 1) return false;

    outLHS = makeRow();
    auto* lhsRow = static_cast<NodeRow*>(outLHS.get());
    for (int i = 0; i < eqIdx; ++i) {
        lhsRow->appendChild(cloneNode(row->child(i)));
    }

    outRHS = makeRow();
    auto* rhsRow = static_cast<NodeRow*>(outRHS.get());
    for (int i = eqIdx + 1; i < count; ++i) {
        rhsRow->appendChild(cloneNode(row->child(i)));
    }

    return !lhsRow->isEmpty() && !rhsRow->isEmpty();
}
#ifdef NATIVE_SIM
bool EquationsApp::debugAssert(const std::string& expected) {
    std::istringstream in(expected); std::string kind,value; in>>kind>>value;
    if(kind=="closed") return !_screen && !_editRow && !_numEquations && _giacResult.groups.empty() && !_canvas[0].obj();
    if(!_screen) return false;
    if(kind=="view") {
        using namespace numos::tutor;
        if(_state!=State::STEPS||!_stepProse)return false;
        lv_obj_update_layout(_body);
        const int scroll=lv_obj_get_scroll_y(_body);
        const int maxScroll=std::max(0,scroll+int(lv_obj_get_scroll_bottom(_body)));
        if(value=="bounded")return scroll>=0&&scroll<=maxScroll;
        int expectedNumber=0;
        if(value=="page"){in>>expectedNumber;return expectedNumber==int(_teachingPage);}
        if(value=="count"){in>>expectedNumber;return expectedNumber==int(teachingPageCount(_derivation,_stepDetail));}
        if(value=="dump") {
            auto quote=[](const std::string& text) {
                std::string result="\"";
                for(char c:text) {
                    if(c=='\n')result+="\\n";
                    else if(c=='\r')result+="\\r";
                    else if(c=='\t')result+="\\t";
                    else {if(c=='"'||c=='\\')result+='\\';result+=c;}
                }
                return result+'"';
            };
            auto ast=[&](const MathNode* node,auto&& self,unsigned depth)->std::string {
                if(!node||depth>40)return "null";
                std::string text;
                if(node->type()==NodeType::Number)text=static_cast<const NodeNumber*>(node)->value();
                else if(node->type()==NodeType::Symbol)text=static_cast<const NodeSymbol*>(node)->name();
                else if(node->type()==NodeType::Variable)text=static_cast<const NodeVariable*>(node)->label();
                else if(node->type()==NodeType::Operator)text=static_cast<const NodeOperator*>(node)->symbol();
                std::string json="{\"type\":"+std::to_string(unsigned(node->type()))+",\"text\":"+quote(text)+",\"children\":[";
                for(int i=0;i<node->childCount();++i){if(i)json+=',';json+=self(node->child(i),self,depth+1);}
                return json+"]}";
            };
            const auto page=teachingPageAt(_derivation,_stepDetail,_teachingPage);
            const char* kinds[]={"start","transition","chain","coefficients","discriminant","quadratic_formula","roots","final"};
            const char* formulas[]={"equation","authored","conditions","standard_quadratic","coefficients","discriminant_definition","discriminant_values","general_formula","substituted_formula","operand","row_operation","coefficient_equation"};
            std::ostringstream json;
            json<<"{\"page\":"<<_teachingPage<<",\"count\":"<<teachingPageCount(_derivation,_stepDetail)
                <<",\"guided\":"<<(_stepDetail?"true":"false")<<",\"step\":"<<_stepIndex<<",\"lastStep\":"<<page.last
                <<",\"kind\":"<<quote(kinds[unsigned(page.kind)])<<",\"title\":"<<quote(lv_label_get_text(_title))
                <<",\"prose\":"<<quote(lv_label_get_text(_stepProse))<<",\"heading\":"<<quote(lv_label_get_text(_stepConditions))
                <<",\"scrollY\":"<<scroll<<",\"maxScroll\":"<<maxScroll
                <<",\"micros\":"<<_viewBuildMicros<<",\"conversions\":"<<_viewConversions<<",\"nodes\":"<<_viewNodesCount
                <<",\"formulas\":[";
            for(unsigned i=0;i<_stepFormulaCount;++i) {
                const auto& f=_stepFormulaRefs[i];if(i)json<<',';
                json<<"{\"kind\":"<<quote(formulas[unsigned(f.kind)])<<",\"state\":"<<f.state<<",\"step\":"<<f.step
                    <<",\"branch\":"<<unsigned(f.branch)<<",\"row\":"<<unsigned(f.row)
                    <<",\"caption\":"<<quote(lv_label_get_text(_stepLabels[i]))<<",\"ast\":"<<ast(_viewNodes[i].get(),ast,0)<<'}';
            }
            json<<"]}";std::printf("[TUTOR_VIEW] %s\n",json.str().c_str());return true;
        }
        return false;
    }
    if(kind=="trace") {
        using namespace numos::tutor;
        if(value=="complete")return _derivation.status==Status::Complete&&_derivation.validity==Verdict::Verified&&_derivation.completeness==Verdict::Verified;
        if(value=="check")return numos::GiacEngine::instance().verifyDerivation(_derivation,_derivation.input)==Verdict::Verified&&numos::GiacEngine::instance().tutorSnapshotCurrent(_derivation.input,_equationEpoch,setting_complex_enabled);
        if(value=="dump"){std::printf("[TUTOR_TRACE] %s\n",replayJson(_derivation,_stepLocale).c_str());return true;}
        int n=0;in>>n;
        if(value=="count")return n==int(_derivation.steps.size());
        if(value=="index")return n==_stepIndex;
        if(value=="builds")return n==int(_tutorBuilds);
        if(value=="formulas") {
            if(_state!=State::STEPS||!_stepFormulaCount)return false;
            for(int i=0;i<4;++i)if(_rows[i]&&!lv_obj_has_flag(_rows[i],LV_OBJ_FLAG_HIDDEN))
                if(!_viewNodes[i]||_viewNodes[i]->childCount()==0)return false;
            return true;
        }
        return false;
    }
    if(kind=="locale") {
        if(value=="en")_stepLocale=numos::tutor::Locale::English;
        else if(value=="es")_stepLocale=numos::tutor::Locale::Spanish;
        else if(value=="fr")_stepLocale=numos::tutor::Locale::French;
        else if(value=="pseudo")_stepLocale=numos::tutor::Locale::Pseudo;
        else return false;
        if(_state==State::STEPS)drawStep(true);return true;
    }
    if(kind=="state") {
        const char* names[]={"list","template","editing","solving","result","steps"};
        return value==names[int(_state)];
    }
    if(kind=="count") return value==std::to_string(_numEquations);
    if(kind=="focus") return value==std::to_string(_listFocus) && _rows[_listFocus] && !lv_obj_has_flag(_rows[_listFocus],LV_OBJ_FLAG_HIDDEN);
    if(kind=="epochs") {
        if(value=="invalid") return !_solveEpoch && !_stepsEpoch && _giacResult.groups.empty() && _resultKind==ResultKind::None;
        if(value=="current") return _solveEpoch && _solveEpoch==_equationEpoch;
        if(value=="steps") return _stepsEpoch && _stepsEpoch==_solveEpoch && _solveEpoch==_equationEpoch;
        return false;
    }
    if(kind=="cursor") {
        if(value=="root") return _editCursor.cursor().row==_editRow;
        if(value=="slot") return _editCursor.cursor().row && _editCursor.cursor().row!=_editRow;
        if(value=="visible") {
            lv_area_t c{},b{}; lv_obj_get_coords(_body,&b);
            const bool known=_canvas[0].cursorBounds(c);
            std::printf("EQ_CURSOR|y=%d..%d|viewport=%d..%d|scroll=%d\n",c.y1,c.y2,b.y1,b.y2,int(lv_obj_get_scroll_y(_body)));
            return known && c.y1>=b.y1 && c.y2<=b.y2;
        }
        return false;
    }
    if(kind=="scroll") return value=="positive"?lv_obj_get_scroll_y(_body)>0:lv_obj_get_scroll_y(_body)==0;
    if(kind=="page") return value==std::to_string(_page);
    if(kind=="variables") return value=="open"?_variableMenu!=nullptr:_variableMenu==nullptr;
    if(kind=="input") {
        std::string actual,error;
        return _editRow && numos::CalculationEngine::serializeForGiac(_editRow,actual,error) && value==actual;
    }
    if(kind=="equivalent") {
        int index=-1; std::string expectedValue; in>>index>>expectedValue;
        const auto actual=debugSolutionExactText(value,index);
        if(actual.empty() || expectedValue.empty()) return false;
        const auto expression="simplify(("+actual+")-("+expectedValue+"))";
        const auto residual=numos::GiacEngine::instance().evaluate(expression.c_str());
        return residual.ok() && residual.exactText=="0";
    }
    if(kind=="substitution") {
        const int index=std::atoi(value.c_str());
        if(index<0 || index>=int(_giacResult.groups.size())) return false;
        auto replaceIdentifiers=[&](const std::string& authored) {
            std::string out;
            for(size_t i=0;i<authored.size();) {
                const size_t start=i;
                while(i<authored.size() && ((authored[i]>='a' && authored[i]<='z') ||
                      (authored[i]>='A' && authored[i]<='Z') || authored[i]=='_')) ++i;
                if(i==start) { out+=authored[i++]; continue; }
                const auto identifier=authored.substr(start,i-start); bool replaced=false;
                for(const auto& v:_giacResult.groups[index].values) if(v.variable==identifier) {
                    out+='('+v.exactText+')'; replaced=true; break;
                }
                if(!replaced) out+=identifier;
            }
            return out;
        };
        auto& engine=numos::GiacEngine::instance();
        for(int i=0;i<_numEquations;++i) {
            NodePtr lhs,rhs; std::string a,b,error;
            if(!splitAtEquals(_eqRowData[i],lhs,rhs) ||
               !numos::CalculationEngine::serializeForGiac(lhs.get(),a,error) ||
               !numos::CalculationEngine::serializeForGiac(rhs.get(),b,error)) return false;
            // Test-only literal substitution BEFORE parsing/simplification:
            // authored denominator exclusions cannot disappear first.
            a=replaceIdentifiers(a); b=replaceIdentifiers(b);
            if(!engine.evaluate(a.c_str()).ok() || !engine.evaluate(b.c_str()).ok()) return false;
            const auto residual=engine.simplify(("("+a+")-("+b+")").c_str());
            if(!residual.ok() || residual.exactText!="0") return false;
        }
        return true;
    }
    if(kind=="draft" || kind=="equation") {
        NodeRow* row=_editRow;
        if(kind=="equation") { int index=std::atoi(value.c_str()); if(index<0 || index>=_numEquations) return false; row=_eqRowData[index]; in>>value; }
        if(value=="empty") return row && row->isEmpty();
        NodePtr lhs,rhs; std::string l,r,error;
        return splitAtEquals(row,lhs,rhs) && numos::CalculationEngine::serializeForGiac(lhs.get(),l,error) &&
            numos::CalculationEngine::serializeForGiac(rhs.get(),r,error) && value==l+"="+r;
    }
    if(kind=="template") {
        int i=std::atoi(value.c_str()); if(i<0 || i>=NUM_TEMPLATES) return false;
        auto reference=buildTemplateAST(i);
        const auto* actual=_state==State::TEMPLATE?_viewNodes[i].get():_state==State::EDITING?_editNode.get():nullptr;
        auto same=[&](auto&& self,const MathNode* a,const MathNode* b,int depth)->bool {
            if(!a || !b || a==b || depth>28 || a->type()!=b->type() || a->childCount()!=b->childCount()) return false;
            if(a->type()==NodeType::Variable && static_cast<const NodeVariable*>(a)->name()!=static_cast<const NodeVariable*>(b)->name()) return false;
            if(a->type()==NodeType::Number && static_cast<const NodeNumber*>(a)->value()!=static_cast<const NodeNumber*>(b)->value()) return false;
            if(a->type()==NodeType::Operator && static_cast<const NodeOperator*>(a)->op()!=static_cast<const NodeOperator*>(b)->op()) return false;
            if(a->type()==NodeType::Function && static_cast<const NodeFunction*>(a)->funcKind()!=static_cast<const NodeFunction*>(b)->funcKind()) return false;
            if(a->type()==NodeType::Constant && static_cast<const NodeConstant*>(a)->constKind()!=static_cast<const NodeConstant*>(b)->constKind()) return false;
            for(int k=0;k<a->childCount();++k) if(!self(self,a->child(k),b->child(k),depth+1)) return false;
            return true;
        };
        return same(same,actual,reference.get(),0);
    }
    if(kind=="layout") {
        lv_obj_update_layout(_screen);
        auto inside=[](lv_obj_t* child,lv_obj_t* parent) {
            lv_area_t a{},b{}; lv_obj_get_coords(child,&a); lv_obj_get_coords(parent,&b);
            return a.x1>=b.x1 && a.y1>=b.y1 && a.x2<=b.x2 && a.y2<=b.y2;
        };
        if(!inside(_title,_screen) || !inside(_hint,_screen) || !inside(_body,_screen)) return false;
        if(lv_obj_get_y(_title)+lv_obj_get_height(_title)>CONTENT_Y || lv_obj_get_y(_hint)<CONTENT_Y+CONTENT_H) return false;
        if(_state==State::EQ_LIST || _state==State::TEMPLATE) {
            int f=_state==State::EQ_LIST?_listFocus:_templateFocus;
            if(!_rows[f]) return false;
            if(lv_obj_get_height(_rows[f])<=CONTENT_H && !inside(_rows[f],_body)) return false;
            for(size_t i=0;i<_canvas.size();++i) if(_canvas[i].obj() && !inside(_canvas[i].obj(),lv_obj_get_parent(_canvas[i].obj()))) return false;
        }
        return true;
    }
    return false; // malformed and unknown assertions must fail closed
}
#endif
