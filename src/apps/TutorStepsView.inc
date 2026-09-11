// SPDX-License-Identifier: GPL-3.0-or-later
// Private Steps view implementation; included once by EquationsApp.cpp.
void EquationsApp::showSteps() {
    using namespace numos::tutor;
    clearView(); _state=State::STEPS;
    const auto hint=explain(Message::ViewGuidedHint,{},_stepLocale);
    header("Steps",hint.c_str());
    if(!_solveEpoch || _solveEpoch!=_equationEpoch ||
       !numos::GiacEngine::instance().tutorSnapshotCurrent(_derivation.input,_equationEpoch,setting_complex_enabled)) {
        const auto note=explain(Message::ViewSolveFirst,{},_stepLocale);
        text(_body,note.c_str(),8,8,280);return;
    }
    _stepsEpoch=_solveEpoch;
    if((_derivation.status!=Status::Complete && _derivation.status!=Status::Partial) || _derivation.steps.empty()) {
        const auto key=_derivation.status==Status::CheckFailed?Message::CheckFailed:
            _derivation.status==Status::ReconciliationFailed?Message::ReconciliationFailed:
            _derivation.status==Status::Partial?Message::Partial:Message::Unsupported;
        const auto note=explain(key,{},_stepLocale);text(_body,note.c_str(),8,8,280);return;
    }
    _stepProse=text(_body,"",8,0,280);
    _stepConditions=text(_body,"",8,0,280); // short operation heading
    lv_obj_set_style_text_color(_stepConditions,lv_color_hex(ACCENT),0);
    lv_obj_set_width(_title,196);
    _stepVerified=text(_screen,"",212,32,100);
    lv_obj_set_style_text_font(_stepVerified,&lv_font_montserrat_10,0);
    lv_obj_set_style_text_align(_stepVerified,LV_TEXT_ALIGN_RIGHT,0);
    // WHY: four reused formula widgets cover a summary's start plus three
    // intermediate states, or a row operation with both input equations.
    for(int i=0;i<4;++i) {
        _stepLabels[i]=text(_body,"",8,0,280);
        _viewNodes[i]=makeRow();formula(i,static_cast<NodeRow*>(_viewNodes[i].get()),0,nullptr);
    }
    drawStep();
}

void EquationsApp::scrollTeaching(int delta) {
    if(!_body)return;
    lv_obj_update_layout(_body);
    const int current=lv_obj_get_scroll_y(_body);
    const int bottom=std::max(0,current+int(lv_obj_get_scroll_bottom(_body)));
    lv_obj_scroll_to_y(_body,std::clamp(current+delta,0,bottom),LV_ANIM_OFF);
}

void EquationsApp::drawStep(bool preserveScroll) {
    using namespace numos::tutor;
    using tutorview::FormulaKind;
    using tutorview::FormulaRef;
    if(!_stepProse || _derivation.steps.empty())return;
    const auto started=std::chrono::steady_clock::now();
    const int oldScroll=lv_obj_get_scroll_y(_body);
    const unsigned total=teachingPageCount(_derivation,_stepDetail);
    if(!total)return;
    _teachingPage=std::min(_teachingPage,total-1);
    const auto page=teachingPageAt(_derivation,_stepDetail,_teachingPage);
    _stepIndex=page.first;
    const unsigned operationIndex=page.kind==TeachingKind::Final?page.last:page.first;
    const auto& step=_derivation.steps[operationIndex];
    const auto& before=_derivation.states[step.before];
    const auto& after=_derivation.states[step.after];
    std::array<FormulaRef,4> refs{};
    std::array<NodePtr,4> prepared{};
    std::array<std::string,4> captions{};
    unsigned count=0,calls=0,nodes=0,proseAfter=0;
    auto words=[&](Message key){return explain(key,{},_stepLocale);};
    auto numbered=[&](Message key,unsigned number){return explain(key,{{ParameterKind::Integer,std::to_string(number)}},_stepLocale);};
    auto titleKey=Message::ViewCurrent;
    auto ref=[&](FormulaKind kind,unsigned state=0,unsigned branch=0,unsigned row=0,unsigned source=65535){
        return FormulaRef{kind,uint16_t(state),uint16_t(source==65535?operationIndex:source),uint8_t(branch),uint8_t(row)};
    };
    try {
        std::string prose=explain(step.explanation,step.parameters,_stepLocale);
        auto add=[&](FormulaRef formula,std::string caption=std::string{}) {
            if(count>=refs.size())throw std::bad_alloc(); // bounded projection, never silently omit a formula
            refs[count]=formula;captions[count]=std::move(caption);++count;
        };
        auto caseCaption=[&](unsigned branch, bool active, bool final=false) {
            if(final)return numbered(Message::ViewSolutionNumber,branch+1);
            if(after.branches[branch].status==BranchStatus::Rejected)return numbered(Message::RejectedAlternative,branch+1);
            return numbered(active?Message::ActiveAlternative:Message::ViewUnchangedCase,branch+1);
        };
        auto addState=[&](unsigned state,bool final) {
            const auto& value=_derivation.states[state];
            unsigned solution=0;
            for(unsigned b=0;b<value.branches.size();++b) {
                if(final&&value.branches[b].status==BranchStatus::Rejected)continue;
                ++solution;
                for(unsigned r=0;r<value.branches[b].equations.size();++r) {
                    std::string caption;
                    if(value.branches[b].equations.size()>1)caption=numbered(Message::EquationRow,r+1);
                    else if(value.branches.size()>1)caption=final?numbered(Message::ViewSolutionNumber,solution):numbered(Message::Alternative,b+1);
                    add(ref(FormulaKind::Equation,state,b,r),std::move(caption));
                }
            }
        };
        switch(page.kind) {
        case TeachingKind::Start:
            titleKey=Message::ViewStart;prose=words(Message::ViewStartingSystem);
            for(unsigned r=0;r<_derivation.input.authored.size();++r)
                add(ref(FormulaKind::Authored,0,0,r),numbered(Message::EquationRow,r+1));
            break;
        case TeachingKind::Coefficients:
            titleKey=Message::ViewCoefficients;prose=words(Message::ViewMatchCoefficients);
            add(ref(FormulaKind::CoefficientEquation),words(Message::ViewZeroRight));
            add(ref(FormulaKind::StandardQuadratic),words(Message::ViewStandardForm));
            add(ref(FormulaKind::Coefficients),words(Message::ViewCoefficientValues));
            break;
        case TeachingKind::Discriminant:
            titleKey=Message::ViewDiscriminant;prose=words(Message::ViewComputeDiscriminant);
            add(ref(FormulaKind::DiscriminantDefinition),words(Message::ViewDefinition));
            add(ref(FormulaKind::DiscriminantValues),words(Message::ViewEvaluated));
            break;
        case TeachingKind::QuadraticFormula:
            titleKey=Message::ViewFormula;prose=words(Message::ViewUseFormula);
            add(ref(FormulaKind::GeneralFormula),words(Message::ViewFormula));
            add(ref(FormulaKind::SubstitutedFormula),words(Message::ViewSubstitution));
            break;
        case TeachingKind::Roots:
            titleKey=Message::ViewSolutions;prose=words(Message::ViewReadRoots);
            addState(step.after,false);break;
        case TeachingKind::Final:
            if(page.first!=page.last) {
                // Both complete message templates retain their checked sources:
                // the quadratic's sign evaluation and the terminal verification.
                prose=words(Message::ViewReadRoots)+"\n"+prose;
            }
            titleKey=after.conclusion==Conclusion::Empty?Message::ViewNoSolution:
                after.conclusion==Conclusion::Family?Message::ViewFamily:
                step.rule==Rule::AlreadySolved?Message::ViewAlready:
                after.conclusion==Conclusion::Identity?Message::ViewIdentity:Message::ViewSolutions;
            if(after.conclusion==Conclusion::Empty) {
                // A classification may legitimately leave its equation unchanged.
                // Its reason, rather than a fabricated transform, is instructional.
                if(step.rule==Rule::QuadraticFormula)add(ref(FormulaKind::DiscriminantValues));
                else addState(step.after,true);
            } else addState(step.after,true);
            break;
        case TeachingKind::Chain:
            titleKey=Message::ViewSummary;prose.clear();
            add(ref(FormulaKind::Equation,step.before),words(Message::ViewBefore));
            for(unsigned i=page.first;i<=page.last;++i) {
                const auto& item=_derivation.steps[i];
                add(ref(FormulaKind::Equation,item.after,0,0,i),explain(item.explanation,item.parameters,_stepLocale));
            }
            break;
        case TeachingKind::Transition: {
            switch(step.rule) {
            case Rule::Domain:titleKey=Message::ViewDomain;break;
            case Rule::SubstituteValues:titleKey=Message::ViewSavedValues;break;
            case Rule::Expand:titleKey=Message::ViewExpand;break;
            case Rule::Collect:titleKey=Message::ViewCollect;break;
            case Rule::SwapSides:titleKey=Message::ViewBalance;break;
            case Rule::AddBoth:titleKey=Message::ViewBalance;break;
            case Rule::DivideBoth:titleKey=Message::ViewDivide;break;
            case Rule::ClearDenominators:titleKey=Message::ViewClear;break;
            case Rule::Factor:titleKey=Message::ViewFactor;break;
            case Rule::SquareRoots:titleKey=Message::ViewSquareRoots;break;
            case Rule::ZeroProduct:titleKey=Message::ViewCases;break;
            case Rule::RejectCandidate:titleKey=Message::ViewCheckCandidate;break;
            case Rule::RowSwap:titleKey=Message::ViewRowSwap;break;
            case Rule::RowScale:titleKey=Message::ViewRowScale;break;
            case Rule::RowAdd:titleKey=Message::ViewRowAdd;break;
            default:break;
            }
            const bool system=before.branches[step.branch].equations.size()>1;
            const bool rowOperation=step.rule==Rule::RowAdd||step.rule==Rule::RowScale||step.rule==Rule::RowSwap;
            const auto label=system?numbered(Message::EquationRow,step.row+1):
                before.branches.size()>1?numbered(Message::ActiveAlternative,step.branch+1):std::string{};
            add(ref(FormulaKind::Equation,step.before,step.branch,step.row),label);
            if(step.rule==Rule::RowAdd||step.rule==Rule::RowSwap)
                add(ref(FormulaKind::Equation,step.before,step.branch,step.otherRow),numbered(Message::EquationRow,step.otherRow+1));
            proseAfter=count;
            if(rowOperation)add(ref(FormulaKind::RowOperation));
            else if(step.rule==Rule::ClearDenominators ||
                    teachingNeedsOperand(step))
                add(ref(FormulaKind::Operand),words(Message::ViewOperand));
            if(step.rule==Rule::Domain)break; // new condition is the actual changed mathematical content
            if(step.relation==Relation::ExhaustiveSplit)addState(step.after,false);
            else if(step.rule==Rule::RowSwap) {
                // Four widgets: the two before rows and the two after rows.
                // The exact swap notation is redundant with the checked prose.
                --count;
                add(ref(FormulaKind::Equation,step.after,step.branch,step.row),numbered(Message::EquationRow,step.row+1));
                add(ref(FormulaKind::Equation,step.after,step.branch,step.otherRow),numbered(Message::EquationRow,step.otherRow+1));
            } else {
                add(ref(FormulaKind::Equation,step.after,step.branch,step.row),
                    before.branches.size()>1?caseCaption(step.branch,true):system?numbered(Message::EquationRow,step.row+1):std::string{});
                if(after.branches.size()>1 && after.conditions.empty() && count<4)
                    for(unsigned b=0;b<after.branches.size();++b)if(b!=step.branch)
                        add(ref(FormulaKind::Equation,step.after,b),caseCaption(b,false));
            }
            break;
        }
        }
        if(page.kind!=TeachingKind::Start && !after.conditions.empty())
            add(ref(FormulaKind::Conditions,step.after),numbered(Message::ViewRestrictions,unsigned(after.conditions.size())));

        // Construct transactionally before changing the visible page. Authored
        // equations are cloned before any Giac evaluation can cancel x/x, etc.
        for(unsigned i=0;i<count;++i) {
            const Equation* authored=nullptr;
            if(refs[i].kind==FormulaKind::Authored)authored=&_derivation.input.authored[refs[i].row];
            if(refs[i].kind==FormulaKind::Equation)
                authored=&_derivation.states[refs[i].state].branches[refs[i].branch].equations[refs[i].row];
            if(authored)for(unsigned j=0;j<_derivation.input.authored.size();++j)
                if(authored->lhs==_derivation.input.authored[j].lhs&&authored->rhs==_derivation.input.authored[j].rhs) {
                    prepared[i]=cloneNode(_eqRowData[j]);
                    tutorview::authoredSigns(prepared[i].get());break;
                }
            if(!prepared[i])prepared[i]=tutorview::build(_derivation,refs[i],calls);
            if(!prepared[i])throw std::bad_alloc();
            nodes+=tutorview::nodeCount(prepared[i].get());
            if(nodes>640)throw std::bad_alloc();
        }
        const auto title=page.kind==TeachingKind::Final||page.kind==TeachingKind::Start?words(titleKey):
            explain(_stepDetail?Message::ViewStep:Message::ViewSummary,
                    {{ParameterKind::Integer,std::to_string(_teachingPage+1)},
                     {ParameterKind::Integer,std::to_string(total)}},_stepLocale);
        const auto hint=words(_stepDetail?Message::ViewGuidedHint:Message::ViewSummaryHint);
        header(title.c_str(),hint.c_str());
        lv_label_set_text(_stepVerified,words(_derivation.status==Status::Complete?Message::ViewVerified:Message::ViewPartial).c_str());
        const auto heading=page.kind==TeachingKind::Final||page.kind==TeachingKind::Start||page.kind==TeachingKind::Chain?std::string{}:words(titleKey);
        lv_label_set_text(_stepConditions,heading.c_str());lv_obj_set_pos(_stepConditions,8,2);lv_obj_update_layout(_stepConditions);
        int y=heading.empty()?2:lv_obj_get_height(_stepConditions)+8;
        lv_label_set_text(_stepProse,prose.c_str());lv_obj_update_layout(_stepProse);
        auto placeProse=[&] {
            lv_obj_set_pos(_stepProse,8,y);
            if(!prose.empty())y+=lv_obj_get_height(_stepProse)+8;
        };
        for(unsigned i=0;i<count;++i) {
            if(i==proseAfter)placeProse();
            lv_label_set_text(_stepLabels[i],captions[i].c_str());lv_obj_set_pos(_stepLabels[i],8,y);
            lv_obj_remove_flag(_stepLabels[i],LV_OBJ_FLAG_HIDDEN);lv_obj_update_layout(_stepLabels[i]);
            if(!captions[i].empty())y+=lv_obj_get_height(_stepLabels[i])+4;
            _canvas[i].setExpression(nullptr,nullptr);_viewNodes[i].reset();
            auto row=makeRow();auto* r=static_cast<NodeRow*>(row.get());r->appendChild(std::move(prepared[i]));
            _viewNodes[i]=std::move(row);auto& canvas=_canvas[i];canvas.setExpression(r,nullptr);
            r->calculateLayout(canvas.normalMetrics());
            const int height=std::max(26,int(mathObjectHeightPx(r->layout(),canvas.normalMetrics(),4)));
            lv_obj_remove_flag(_rows[i],LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(_rows[i],2,y);lv_obj_set_height(_rows[i],height+8);
            const bool affected=step.relation==Relation::ExhaustiveSplit||
                (refs[i].branch==step.branch&&(refs[i].row==step.row||step.rule==Rule::RowSwap));
            const bool changed=refs[i].kind==FormulaKind::Equation&&refs[i].state==step.after&&affected&&
                page.kind!=TeachingKind::Final&&page.kind!=TeachingKind::Roots;
            const bool rejected=refs[i].kind==FormulaKind::Equation&&
                _derivation.states[refs[i].state].branches[refs[i].branch].status==BranchStatus::Rejected;
            lv_obj_set_style_bg_color(_rows[i],lv_color_hex(rejected?0xFCE5E5:changed?0xFFF1C9:0xF5F5F5),0);
            lv_obj_set_size(canvas.obj(),292,height);canvas.scrollBounded(10000);canvas.invalidate();
            y+=height+14;
        }
        if(proseAfter==count)placeProse();
        for(unsigned i=count;i<4;++i) {
            lv_obj_add_flag(_stepLabels[i],LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(_rows[i],LV_OBJ_FLAG_HIDDEN);
            _canvas[i].setExpression(nullptr,nullptr);_viewNodes[i].reset();
        }
        _stepFormulaRefs=refs;_stepFormulaCount=uint8_t(count);
        lv_obj_update_layout(_body);lv_obj_scroll_to_y(_body,preserveScroll?oldScroll:0,LV_ANIM_OFF);scrollTeaching(0);
#ifdef NATIVE_SIM
        _viewConversions=calls;_viewNodesCount=nodes;
        _viewBuildMicros=uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
#endif
    } catch(...) {
        // Recover without allocating a diagnostic or altering the checked trace.
        // Existing formula trees remain owned and BACK continues to work.
        _stepFormulaCount=0;
        for(unsigned i=0;i<4;++i){lv_obj_add_flag(_rows[i],LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(_stepLabels[i],LV_OBJ_FLAG_HIDDEN);}
        lv_label_set_text_static(_stepProse,messageFallback(Message::ViewUnavailable));lv_obj_set_pos(_stepProse,8,8);
        lv_label_set_text_static(_stepConditions,"");
    }
}
