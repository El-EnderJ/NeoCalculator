#include "Toolbox.h"
#include "MathRenderer.h"
#include "TutorFonts.h"
#include "ToolboxFonts.h"
#include "StatusBar.h"
#include "math/ToolboxStore.h"
#include "math/ToolboxView.h"
#include "math/tutor/Locale.h"
#include "input/KeyboardManager.h"
#include "input/KeySemanticResolver.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#ifdef NATIVE_SIM
#include <chrono>
#endif

namespace ui::toolbox {
using namespace numos::toolbox;
namespace {
#ifdef NATIVE_SIM
struct Measurement {uint64_t samples=0,total=0,maximum=0;};
Measurement measurements[5];
struct Measure {
    unsigned index;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    explicit Measure(unsigned i):index(i){}
    ~Measure(){const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();auto& m=measurements[index];++m.samples;m.total+=ns;m.maximum=std::max(m.maximum,uint64_t(ns));}
};
#define TOOLBOX_MEASURE(i) Measure measurement(i)
#else
#define TOOLBOX_MEASURE(i)
#endif
constexpr unsigned kVisible=4, kDepth=6;
constexpr uint16_t Favorites=1, Recent=2, Search=3;
constexpr uint32_t Ink=0x26364B, Muted=0x66758A, Accent=0xD48B18, Selected=0xFFF0D3;
constexpr int PopupX=18, PopupWidth=284, RowTop=46, FooterHeight=20;
bool spanish() { return numos::i18n::isSpanish(numos::i18n::productLocale); }
const char* tr(const char* en,const char* es) {return spanish()?es:en;}
struct Level {uint16_t group=0;size_t selection=0,top=0;const char* en=nullptr;const char* es=nullptr;};
struct Session {
    Receiver receiver;
    View view;
    vpam::NodeRow* root=nullptr;
    vpam::Cursor cursor{};
    uint32_t epoch=0;
    vpam::ModifierPhase shift{},alpha{};
    lv_obj_t *shield=nullptr,*panel=nullptr,*title=nullptr,*path=nullptr,*counter=nullptr,*hint=nullptr,*message=nullptr;
    std::array<lv_obj_t*,4> tabs{};
    vpam::MathCanvas context;
    std::array<lv_obj_t*,kVisible> rows{},names{},marks{};
    std::array<vpam::MathCanvas,kVisible> canvases;
    std::array<vpam::NodePtr,kVisible> previews;
    std::array<Identity,kVisible> previewIds{};
    std::array<Level,kDepth> levels{};
    unsigned depth=0,visible=0;
    bool queryFocus=false,options=false,help=false,saveWarning=false;
    bool tabsFocus=false;
    unsigned tab=0,tabCursor=0;
    int height=180;
    unsigned option=0;
    char query[65]{};size_t queryLength=0,queryCursor=0;
    size_t matches=0;
    const Entry* optionEntry=nullptr;
    Level& level() {return levels[depth];}
    ~Session() {
        for(auto& canvas:canvases) canvas.destroy();
        context.destroy();
        if(shield) lv_obj_delete(shield);
    }
};
std::unique_ptr<Session> s;
KeyCode suppressed=KeyCode::NONE;
const Provider& provider=catalogProvider();
lv_obj_t* label(lv_obj_t* parent,int x,int y,int width,const lv_font_t* font) {
    auto* obj=lv_label_create(parent);lv_obj_set_pos(obj,x,y);lv_obj_set_width(obj,width);
    // WHY: DOT needs a bounded height; auto-height would wrap long names and
    // descriptions over the following row/footer at the actual 320x240 size.
    lv_obj_set_height(obj,font->line_height);
    lv_obj_set_style_text_font(obj,font,0);lv_obj_set_style_text_color(obj,lv_color_hex(Ink),0);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_DOT);return obj;
}
bool visibleEntry(const Entry& e) {
    return s->view.visible(e);
}
bool visibleRow(const Row& row,unsigned depth=0) {
    return s->view.visible(row,depth);
}
size_t providerCount(uint16_t group) {
    return s->view.count(group);
}
Row providerAt(uint16_t group,size_t index) {
    return s->view.at(group,index);
}
size_t storedCount(bool recent) {
    auto& store=Store::instance();size_t total=0;
    for(size_t i=0;i<(recent?store.recentCount():store.favoriteCount());++i) {
        const auto* e=find(recent?store.recent(i):store.favorite(i));if(e && visibleEntry(*e))++total;
    }return total;
}
size_t count() {
    auto& store=Store::instance();const auto group=s->level().group;
    if(!group) return providerCount(0);
    if(group==Favorites) return storedCount(false);
    if(group==Recent) return storedCount(true);
    if(group==Search) return s->matches;
    return providerCount(group);
}
Row at(size_t index) {
    auto& store=Store::instance();const auto group=s->level().group;
    if(!group) return providerAt(0,index);
    if(group==Favorites || group==Recent) {
        for(size_t i=0;i<(group==Favorites?store.favoriteCount():store.recentCount());++i) {
            const auto* e=find(group==Favorites?store.favorite(i):store.recent(i));
            if(e && visibleEntry(*e) && index--==0)return {e,e->options,e->en,e->es};
        }return {};
    }
    if(group==Search) {
        return s->view.searchAt(index);
    }
    return providerAt(group,index);
}
void refresh();
void changedQuery();
void selectTab(unsigned tab) {
    s->tab=s->tabCursor=tab;s->tabsFocus=false;s->help=s->options=false;
    s->depth=tab?1:0;s->levels[0]={s->receiver.initialGroup};
    if(tab)s->level()={uint16_t(tab),0,0,nullptr,nullptr};
    s->queryFocus=tab==Search;
    if(s->queryFocus){changedQuery();return;}
    refresh();
}
void notice(const char* en,const char* es) {
    // WHY: search owns a separate line. Empty/error messages start below it.
    const int y=RowTop+(s->level().group==Search && !s->help && !s->options?26:4);
    lv_obj_set_pos(s->message,8,y);lv_obj_set_size(s->message,PopupWidth-18,s->height-FooterHeight-y-4);
    lv_label_set_text_static(s->message,tr(en,es));lv_obj_remove_flag(s->message,LV_OBJ_FLAG_HIDDEN);
    for(auto* row:s->rows)if(row)lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
    s->visible=0;
}
void close(bool restore=true) {
    TOOLBOX_MEASURE(4);
    if(!s)return;
    Store::instance().save();
    const auto shift=s->shift,alpha=s->alpha;
    s.reset();
    if(restore) vpam::KeyboardManager::instance().restoreLogical(shift,alpha);
}
bool requestClose() {
    if(!Store::instance().save() && !s->saveWarning) {
        s->saveWarning=true;
        notice("Not saved. Kept this session. BACK closes.","Sin guardar. En sesión. BACK cierra.");return false;
    }
    close();return true;
}
void enter(uint16_t group,const char* en,const char* es,Identity origin={}) {
    if(s->depth+1>=kDepth)return;
    ++s->depth;s->level()={group,provider.initialFrom?provider.initialFrom(provider.context,group,origin):provider.initial(provider.context,group),0,en,es};
    // Provider-requested selection is centred when the viewport allows it.
    s->level().top=s->level().selection>=kVisible/2 ? s->level().selection-kVisible/2:0;
    s->queryFocus=group==Search;
    if(group==Search) {s->query[0]=0;s->queryLength=s->queryCursor=0;changedQuery();return;}
    refresh();
}
void activate(bool right=false) {
    TOOLBOX_MEASURE(3);
    if(!s || !count())return;
    const Row row=at(s->level().selection);
    if(!row.entry || right) {if(row.children)enter(row.children,row.entry?displayName(*row.entry):row.en,row.entry?displayName(*row.entry):row.es,row.entry?row.entry->identity:Identity{});return;}
    const auto& e=*row.entry;
    if(!available(e,s->receiver.capabilities)) {notice("Unavailable in this editor","No disponible en este editor");return;}
    if(!Store::instance().save() && !s->saveWarning) {
        s->saveWarning=true;
        notice("Favorites not saved. EXE inserts; BACK returns.","Favoritos sin guardar. EXE inserta; BACK vuelve.");return;
    }
    auto& cc=*s->receiver.cursor;
    if(cc.epoch()!=s->epoch || cc.rootRow()!=s->root || cc.cursor().row!=s->cursor.row || cc.cursor().index!=s->cursor.index) {
        notice("Editor changed. Close and reopen.","El editor cambió. Cierra y vuelve a abrir.");return;
    }
    if(s->receiver.selected) {
        const auto receiver=s->receiver;
        if(!visibleEntry(e) || !receiver.selected(receiver.owner,e.identity)) {
            notice("Cannot select. Previous result retained.","No se puede elegir. Resultado conservado.");return;
        }
        close();return; // EXE is consumed; selecting a target never inserts.
    }
#if defined(__cpp_exceptions)
    try {
#endif
        auto prepared=prepare(e);
        if(!cc.insertPrepared(std::move(prepared.node),prepared.slot,prepared.index)) {
            notice("Cannot insert. Your expression is intact.","No se puede insertar. Expresión intacta.");return;
        }
#if defined(__cpp_exceptions)
    } catch(const std::bad_alloc&) {notice("Not enough memory. BACK cancels.","Memoria insuficiente. BACK cancela.");return;}
#endif
    const auto receiver=s->receiver;
    Store::instance().inserted(e.identity);
    // The transaction is now published. Reporting an app refresh failure as a
    // failed insertion would falsely promise rollback. No allocation follows
    // in the Toolbox publication/close path (save is noexcept).
    close();
    if(receiver.committed)receiver.committed(receiver.owner);
}
void optionsAction() {
    if(!s->optionEntry)return;
    auto& store=Store::instance();auto id=s->optionEntry->identity;
    if(s->option==0) {
        const auto result=store.toggle(id);
        s->options=false;refresh();
        if(result==FavoriteResult::Full)notice("24 favorites: remove one first.","24 favoritos: elimina uno primero.");
    } else if(s->option==1 || s->option==2) {
        store.move(id,s->option==1?-1:1);
        if(s->level().group==Favorites)for(size_t i=0;i<store.favoriteCount();++i)if(store.favorite(i)==id)s->level().selection=i;
        refresh();
    } else {s->help=true;s->options=false;refresh();}
}
void pointer(lv_event_t* ev) {
    if(!s)return;
    auto* target=static_cast<lv_obj_t*>(lv_event_get_target(ev));
    if(target==s->shield){requestClose();return;}
    if(s->help){back();return;}
    for(unsigned i=0;i<s->tabs.size();++i)if(target==s->tabs[i]){selectTab(i);return;}
    if(target==s->path && s->level().group==Search){s->queryFocus=true;refresh();return;}
    if(target==s->title){back();return;}
    if(target==s->hint) {
        if(s->help || s->options){back();return;}
        auto row=count()?at(s->level().selection):Row{};
        if(row.entry){s->optionEntry=row.entry;s->options=true;s->option=0;refresh();}return;
    }
    for(unsigned i=0;i<s->visible;++i) {
        if(target!=s->rows[i] && target!=s->marks[i])continue;
        if(s->options){s->option=i;optionsAction();return;}
        s->level().selection=s->level().top+i;s->queryFocus=s->tabsFocus=false;
        activate(target==s->marks[i]);return;
    }
}
void refresh() {
    TOOLBOX_MEASURE(1);
    if(!s)return;
    // WHY: even lv_label_set_text_static schedules an allocating LVGL layout
    // event. Help uses immutable text drawn over the existing panel instead.
    if(s->help){lv_obj_invalidate(s->panel);return;}
    // WHY: titles, tabs and reused preview canvases share a compact surface.
    // Repaint their common parent when changing level, so partial invalidation
    // cannot leave a neighbouring tab with only the tail of its old label.
    lv_obj_invalidate(s->panel);
    lv_obj_add_flag(s->message,LV_OBJ_FLAG_HIDDEN);
    s->visible=0;
    for(unsigned i=0;i<kVisible;++i) {
        lv_obj_add_flag(s->rows[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s->canvases[i].obj(),LV_OBJ_FLAG_HIDDEN);
    }
    const auto total=count();auto& level=s->level();
    if(level.selection>=total)level.selection=total?total-1:0;
    if(level.selection<level.top)level.top=level.selection;
    if(level.selection>=level.top+kVisible)level.top=level.selection-kVisible+1;
    const char* heading=s->help?tr("<  Help","<  Ayuda"):s->options?tr("<  Options","<  Opciones"):
        level.group==Search?tr("<  Search","<  Buscar"):level.group==Favorites?tr("<  Favorites","<  Favoritos"):
        level.group==Recent?tr("<  Recent","<  Recientes"):level.en&&level.es?tr(level.en,level.es):s->receiver.selected?tr("Output unit","Unidad de salida"):tr("Toolbox","Herramientas");
    lv_label_set_text(s->title,heading);
    char counter[24];std::snprintf(counter,sizeof(counter),"%u / %u",unsigned(s->options?s->option+1:total?level.selection+1:0),unsigned(s->options?4:total));
    lv_label_set_text(s->counter,s->help?"":counter);
    const char* en[]={s->receiver.selected?"Units":"Functions","Favorites","Recent","Search"};
    const char* es[]={s->receiver.selected?"Unidades":"Funciones","Favoritos","Recientes","Buscar"};
    for(unsigned i=0;i<s->tabs.size();++i) {
        auto* tab=s->tabs[i];lv_label_set_text_static(tab,tr(en[i],es[i]));
        const bool focused=s->tabsFocus && s->tabCursor==i;
        lv_obj_set_style_bg_color(tab,lv_color_hex(focused?Selected:s->tab==i?0xFFFFFF:0xEEF1F5),0);
        lv_obj_set_style_text_color(tab,lv_color_hex(focused||s->tab==i?Ink:Muted),0);
        lv_obj_set_style_border_width(tab,focused||s->tab==i?2:0,0);
        if(s->options||s->help)lv_obj_add_flag(tab,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(tab,LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s->path,LV_OBJ_FLAG_HIDDEN);
    int y=RowTop;
    if(level.group==Search && !s->options && !s->help) {
        char query[80];std::snprintf(query,sizeof(query),"%.*s%s%s",int(s->queryCursor),s->query,s->queryFocus?"|":"",s->query+s->queryCursor);
        lv_label_set_text(s->path,query[0]?query:tr("Type a function...","Busca una función..."));
        lv_obj_set_pos(s->path,8,RowTop+2);lv_obj_set_height(s->path,18);
        lv_obj_set_style_text_color(s->path,lv_color_hex(s->queryFocus?Ink:Muted),0);
        lv_obj_remove_flag(s->path,LV_OBJ_FLAG_HIDDEN);y+=22;
    }
    if(s->help || s->options) {
        lv_label_set_text(s->path,displayName(*s->optionEntry));
        lv_obj_set_pos(s->path,10,27);lv_obj_set_height(s->path,16);
        lv_obj_remove_flag(s->path,LV_OBJ_FLAG_HIDDEN);
    }
    if(s->help) {
        notice(s->optionEntry->helpEn,s->optionEntry->helpEs);
        lv_label_set_text(s->hint,tr("BACK Return","BACK Volver"));return;
    }
    if(s->options) {
        const bool favorite=Store::instance().contains(s->optionEntry->identity);
        const char* optEn[]={favorite?"Remove favorite":"Add favorite","Move up","Move down","Help"};
        const char* optEs[]={favorite?"Quitar favorito":"Añadir favorito","Subir","Bajar","Ayuda"};
        const int height=(s->height-RowTop-FooterHeight-3)/4;
        for(unsigned i=0;i<kVisible;++i) {
            auto* row=s->rows[i];lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(row,1,RowTop+height*i);lv_obj_set_height(row,height);
            lv_obj_set_style_bg_color(row,lv_color_hex(s->option==i?Selected:0xFFFFFF),0);
            lv_obj_set_style_border_width(row,s->option==i?1:0,0);
            lv_obj_set_pos(s->names[i],12,(height-ui::tutorFont12()->line_height)/2);
            lv_obj_set_style_text_font(s->names[i],ui::tutorFont12(),0);
            lv_obj_set_height(s->names[i],ui::tutorFont12()->line_height);lv_label_set_long_mode(s->names[i],LV_LABEL_LONG_DOT);
            lv_obj_set_width(s->names[i],246);lv_obj_set_style_text_color(s->names[i],lv_color_hex(Ink),0);
            lv_label_set_text(s->names[i],tr(optEn[i],optEs[i]));lv_label_set_text(s->marks[i],"");lv_obj_set_user_data(s->marks[i],nullptr);
        }
        s->visible=4;lv_label_set_text(s->hint,tr("EXE Choose    BACK Return","EXE Elegir    BACK Volver"));return;
    }
    if(!total)notice(level.group==Search?"No matches. UP edits your search.":level.group==Favorites?"Save a function with FORMAT. It will appear here.":"Insert a function to find it here next time.",
        level.group==Search?"Sin resultados. UP edita la búsqueda.":level.group==Favorites?"Guarda una función con FORMAT. Aparecerá aquí.":"Inserta una función para encontrarla aquí después.");
    for(unsigned i=0;i<kVisible && level.top+i<total;++i) {
        Row item=at(level.top+i);int height=27,previewWidth=86;
        const bool focused=level.selection==level.top+i && !s->queryFocus && !s->tabsFocus;
        if(item.entry) {
#if defined(__cpp_exceptions)
            try {
#endif
                if(s->previewIds[i]!=item.entry->identity || !s->previews[i]) {
                    auto node=preview(*item.entry);auto root=vpam::makeRow();
                    static_cast<vpam::NodeRow*>(root.get())->appendChild(std::move(node));
                    s->canvases[i].setExpression(nullptr,nullptr);s->previews[i]=std::move(root);s->previewIds[i]=item.entry->identity;
                }
                auto* root=static_cast<vpam::NodeRow*>(s->previews[i].get());root->calculateLayout(s->canvases[i].normalMetrics());
                height=std::max(27,int(root->layout().height())+4);
                // MathCanvas includes 8 px on both sides. Reserve its complete
                // viewport, not just the glyph box, so rules cannot be clipped.
                previewWidth=std::min(134,std::max(86,int(root->layout().width)+18));
                s->canvases[i].setExpression(root,nullptr);
                lv_obj_remove_flag(s->canvases[i].obj(),LV_OBJ_FLAG_HIDDEN);
#if defined(__cpp_exceptions)
            }catch(const std::bad_alloc&){notice("Preview unavailable; BACK cancels.","Vista previa no disponible; BACK cancela.");}
#endif
        }
        if(y+height>s->height-FooterHeight-2) {
            if(level.top+i<=level.selection && level.top<level.selection){++level.top;refresh();return;}
            break;
        }
        auto* row=s->rows[i];lv_obj_remove_flag(row,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(row,1,y);lv_obj_set_height(row,height);
        lv_obj_set_style_bg_color(row,lv_color_hex(focused?Selected:item.entry?0xFFFFFF:0xF0F3F7),0);
        lv_obj_set_style_border_width(row,focused?1:0,0);
        lv_obj_set_pos(s->names[i],item.entry?previewWidth+3:12,(height-ui::tutorFont12()->line_height)/2);
        lv_obj_set_width(s->names[i],item.entry?PopupWidth-previewWidth-33:220);
        const bool unitSearch=level.group==Search && item.entry && (item.entry->recipe==Recipe::Unit || item.entry->recipe==Recipe::QuantityReference);
        const auto* nameFont=unitSearch?ui::tutorFont10():ui::tutorFont12();
        lv_obj_set_style_text_font(s->names[i],nameFont,0);
        lv_obj_set_height(s->names[i],nameFont->line_height*(unitSearch?2:1));
        lv_label_set_long_mode(s->names[i],unitSearch?LV_LABEL_LONG_WRAP:LV_LABEL_LONG_DOT);
        if(unitSearch) {
            const auto* category=numos::units::category(item.entry->category);char name[192];
            std::snprintf(name,sizeof(name),"%s\n%s",displayName(*item.entry),category?tr(category->en,category->es):"");
            lv_obj_set_y(s->names[i],(height-2*nameFont->line_height)/2);lv_label_set_text(s->names[i],name);
        } else lv_label_set_text(s->names[i],item.entry?displayName(*item.entry):tr(item.en?item.en:"",item.es?item.es:""));
        lv_obj_set_style_text_color(s->names[i],lv_color_hex(item.entry&&!available(*item.entry,s->receiver.capabilities)?Muted:Ink),0);
        lv_obj_set_pos(s->canvases[i].obj(),1,0);lv_obj_set_size(s->canvases[i].obj(),previewWidth,height);
        char mark[16]{};
        if(item.entry)std::snprintf(mark,sizeof(mark),"%s",Store::instance().contains(item.entry->identity)?"*":"");
        else std::snprintf(mark,sizeof(mark),"%u",unsigned(s->receiver.filter?providerCount(item.children):provider.count(provider.context,item.children)));
        lv_obj_set_user_data(s->marks[i],reinterpret_cast<void*>(uintptr_t(item.children?1:0)));
        lv_label_set_text(s->marks[i],mark);lv_obj_set_pos(s->marks[i],PopupWidth-44,(height-18)/2);
        y+=height+1;++s->visible;
    }
    const auto selected=total?at(level.selection):Row{};
    if(!lv_obj_has_flag(s->message,LV_OBJ_FLAG_HIDDEN)) {
        for(auto* row:s->rows)lv_obj_add_flag(row,LV_OBJ_FLAG_HIDDEN);
        s->visible=0;
    }
    if(s->tabsFocus)lv_label_set_text(s->hint,tr("LEFT/RIGHT Tabs   EXE Open","IZQ/DER Pestañas   EXE Abrir"));
    else if(s->queryFocus)lv_label_set_text(s->hint,tr("ALPHA Type   DOWN Results","ALPHA Texto   ABAJO Lista"));
    else if(selected.entry)lv_label_set_text(s->hint,s->receiver.selected?
        (selected.children?tr("EXE Apply   RIGHT More   FORMAT","EXE Aplicar   DER Más   FORMAT"):tr("EXE Apply   FORMAT Options","EXE Aplicar   FORMAT Opciones")):
        (selected.children?tr("EXE Insert   RIGHT More   FORMAT","EXE Insertar   DER Más   FORMAT"):tr("EXE Insert   FORMAT Options","EXE Insertar   FORMAT Opciones")));
    else lv_label_set_text(s->hint,tr("EXE Open   BACK Return","EXE Abrir   BACK Volver"));
}


void drawHelp(lv_event_t* event) {
    if(!s || !s->help || !s->optionEntry)return;
    auto* layer=lv_event_get_layer(event);
    lv_area_t panel;lv_obj_get_coords(s->panel,&panel);
    lv_draw_rect_dsc_t background;lv_draw_rect_dsc_init(&background);
    background.bg_color=lv_color_white();background.bg_opa=LV_OPA_COVER;
    background.border_color=lv_color_hex(0x9FA9B7);background.border_width=1;background.radius=4;
    lv_draw_rect(layer,&background,&panel);
    lv_area_t heading{panel.x1+1,panel.y1+1,panel.x2-1,panel.y1+22};
    background.bg_color=lv_color_hex(Ink);background.border_width=0;background.radius=0;
    lv_draw_rect(layer,&background,&heading);
    auto text=[&](const char* value,int x,int y,int width,int height,uint32_t color,const lv_font_t* font=nullptr) {
        lv_draw_label_dsc_t label;lv_draw_label_dsc_init(&label);
        label.font=font?font:ui::tutorFont12();label.color=lv_color_hex(color);
        // WHY: registry and i18n literals outlive every draw task. No copied
        // label buffer, UI object, string or label-layout callback is created here.
        label.text=value;
        lv_area_t bounds{panel.x1+x,panel.y1+y,panel.x1+x+width-1,panel.y1+y+height-1};
        lv_draw_label(layer,&label,&bounds);
    };
    text(tr("<  Help","<  Ayuda"),10,4,PopupWidth-20,18,0xFFFFFF);
    text(displayName(*s->optionEntry),10,27,PopupWidth-20,18,Ink,queryFont());
    text(tr(s->optionEntry->helpEn,s->optionEntry->helpEs),12,RowTop+4,
         PopupWidth-24,s->height-RowTop-FooterHeight-8,Ink);
    text(tr("BACK Return","BACK Volver"),10,s->height-FooterHeight+2,PopupWidth-20,16,Muted);
}

void drawChevron(lv_event_t* ev) {
    auto* obj=static_cast<lv_obj_t*>(lv_event_get_target(ev));
    if(!lv_obj_get_user_data(obj))return;
    lv_area_t box;lv_obj_get_coords(obj,&box);
    const int x=box.x2-4,y=(box.y1+box.y2)/2;
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
    line.color=lv_color_hex(Ink);line.width=2;line.round_start=line.round_end=1;
    line.p1={x-5,y-4};line.p2={x,y};lv_draw_line(lv_event_get_layer(ev),&line);
    line.p1={x,y};line.p2={x-5,y+4};lv_draw_line(lv_event_get_layer(ev),&line);
}

void changedQuery() {
    s->view.search(s->query);s->matches=s->view.matches();
    s->level().selection=s->level().top=0;refresh();
}
size_t previous(size_t pos) {if(!pos)return 0;--pos;while(pos && (uint8_t(s->query[pos])&0xc0)==0x80)--pos;return pos;}
size_t next(size_t pos) {if(pos>=s->queryLength)return s->queryLength;++pos;while(pos<s->queryLength && (uint8_t(s->query[pos])&0xc0)==0x80)++pos;return pos;}
}
bool active(){return bool(s);}
bool searching(){return s && s->queryFocus;}
bool open(lv_obj_t* parent,Receiver receiver) {
    TOOLBOX_MEASURE(0);
    if(s || !parent || !receiver.cursor || !receiver.cursor->rootRow())return false;
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    lv_mem_monitor_t memory{};lv_mem_monitor(&memory);
    if(memory.free_size<15500 || memory.free_biggest_size<5000) {
        StatusBar::showActiveNotice(tr("Low memory","Sin memoria"));return false;
    }
#endif
#if defined(__cpp_exceptions)
    try {
#endif
        s=std::make_unique<Session>();s->receiver=receiver;s->root=receiver.cursor->rootRow();s->cursor=receiver.cursor->cursor();s->epoch=receiver.cursor->epoch();
        s->levels[0].group=receiver.initialGroup;
        auto& km=vpam::KeyboardManager::instance();s->shift=km.shiftPhase();s->alpha=km.alphaPhase();km.reset();
        s->view.prepare(receiver.capabilities,receiver.owner,receiver.filter,receiver.filterUnitFamilies);
        Store::instance().load();
        // The subdued backdrop consumes outside clicks. The live expression
        // is shown above the popup, with no duplicate fragments at its edges.
        s->shield=lv_obj_create(parent);lv_obj_remove_style_all(s->shield);
        lv_obj_set_pos(s->shield,0,24);lv_obj_set_size(s->shield,320,216);
        lv_obj_set_style_bg_color(s->shield,lv_color_hex(0xF3F6FA),0);lv_obj_set_style_bg_opa(s->shield,LV_OPA_COVER,0);
        lv_obj_remove_flag(s->shield,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(s->shield,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s->shield,pointer,LV_EVENT_CLICKED,nullptr);
        // WHY: borrow the live expression with its original math style. This
        // keeps the insertion context visible without cloning a long AST or
        // moving/resizing the actual editor and losing its viewport on cancel.
        s->context.create(s->shield);s->context.setAutoHeightEnabled(false);
        lv_obj_set_style_bg_opa(s->context.obj(),LV_OPA_TRANSP,0);
        s->context.setMathStyle(receiver.style);s->context.setEmptyRootPlaceholderVisible(false);
        s->context.setExpression(s->root,receiver.cursor);s->context.stopCursorBlink();
        const int contextHeight=std::max(24,std::min(48,int(s->root->layout().height())+6));
        lv_obj_set_pos(s->context.obj(),PopupX,2);lv_obj_set_size(s->context.obj(),PopupWidth,contextHeight);
        lv_obj_remove_flag(s->context.obj(),LV_OBJ_FLAG_CLICKABLE);
        const int popupY=contextHeight+6;s->height=210-popupY;
        s->panel=lv_obj_create(s->shield);lv_obj_remove_style_all(s->panel);
        lv_obj_set_pos(s->panel,PopupX,popupY);lv_obj_set_size(s->panel,PopupWidth,s->height);
        lv_obj_remove_flag(s->panel,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(s->panel,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(s->panel,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_bg_opa(s->panel,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(s->panel,1,0);lv_obj_set_style_border_color(s->panel,lv_color_hex(0x9FA9B7),0);
        lv_obj_set_style_border_post(s->panel,true,0);
        lv_obj_set_style_radius(s->panel,4,0);
        lv_obj_add_event_cb(s->panel,drawHelp,LV_EVENT_DRAW_POST,nullptr);
        s->title=label(s->panel,0,0,PopupWidth,ui::tutorFont12());lv_obj_set_height(s->title,23);
        lv_obj_set_style_pad_left(s->title,10,0);lv_obj_set_style_pad_top(s->title,4,0);
        lv_obj_set_style_bg_color(s->title,lv_color_hex(Ink),0);lv_obj_set_style_bg_opa(s->title,LV_OPA_COVER,0);
        lv_obj_set_style_text_color(s->title,lv_color_white(),0);
        s->counter=label(s->panel,232,5,46,ui::tutorFont10());lv_obj_set_style_text_color(s->counter,lv_color_hex(0xD6DEE8),0);
        s->path=label(s->panel,8,RowTop,PopupWidth-18,queryFont());
        s->hint=label(s->panel,7,s->height-FooterHeight+2,PopupWidth-12,ui::tutorFont12());
        lv_obj_set_style_text_color(s->hint,lv_color_hex(Muted),0);
        for(unsigned i=0;i<s->tabs.size();++i) {
            auto* tab=label(s->panel,1+70*i,23,70,ui::tutorFont12());s->tabs[i]=tab;
            lv_obj_set_height(tab,22);lv_obj_set_style_pad_top(tab,4,0);
            lv_obj_set_style_text_align(tab,LV_TEXT_ALIGN_CENTER,0);
            lv_obj_set_style_bg_opa(tab,LV_OPA_COVER,0);
            lv_obj_set_style_border_side(tab,LV_BORDER_SIDE_BOTTOM,0);
            lv_obj_set_style_border_color(tab,lv_color_hex(Accent),0);
            lv_obj_add_flag(tab,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(tab,pointer,LV_EVENT_CLICKED,nullptr);
        }
        s->message=label(s->panel,10,RowTop+7,PopupWidth-20,ui::tutorFont12());
        lv_label_set_long_mode(s->message,LV_LABEL_LONG_WRAP);lv_obj_set_height(s->message,s->height-RowTop-FooterHeight-13);
        lv_obj_set_style_pad_all(s->message,7,0);lv_obj_set_style_bg_color(s->message,lv_color_white(),0);
        lv_obj_set_style_bg_opa(s->message,LV_OPA_COVER,0);lv_obj_set_style_border_width(s->message,1,0);
        lv_obj_set_style_border_color(s->message,lv_color_hex(Accent),0);lv_obj_set_style_radius(s->message,3,0);
        lv_obj_add_flag(s->message,LV_OBJ_FLAG_CLICKABLE);
        for(auto* obj:{s->title,s->path,s->hint}){lv_obj_add_flag(obj,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(obj,pointer,LV_EVENT_CLICKED,nullptr);}
        for(unsigned i=0;i<kVisible;++i) {
            s->rows[i]=lv_obj_create(s->panel);
            if(!s->rows[i]) {close();StatusBar::showActiveNotice(tr("Low memory","Sin memoria"));return false;}
            lv_obj_remove_style_all(s->rows[i]);lv_obj_set_width(s->rows[i],PopupWidth-2);
            lv_obj_set_style_border_color(s->rows[i],lv_color_hex(Accent),0);lv_obj_set_style_bg_opa(s->rows[i],LV_OPA_COVER,0);
            lv_obj_remove_flag(s->rows[i],LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(s->rows[i],LV_OBJ_FLAG_CLICKABLE);
            s->names[i]=label(s->rows[i],90,8,155,ui::tutorFont12());s->marks[i]=label(s->rows[i],PopupWidth-44,8,38,ui::tutorFont12());
            lv_obj_set_height(s->marks[i],18);lv_obj_set_style_pad_right(s->marks[i],14,0);
            lv_obj_add_event_cb(s->marks[i],drawChevron,LV_EVENT_DRAW_MAIN,nullptr);
            lv_obj_set_style_text_align(s->marks[i],LV_TEXT_ALIGN_RIGHT,0);
            s->canvases[i].create(s->rows[i]);s->canvases[i].setAutoHeightEnabled(false);s->canvases[i].stopCursorBlink();
            lv_obj_remove_flag(s->canvases[i].obj(),LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_bg_opa(s->canvases[i].obj(),LV_OPA_TRANSP,0);
            lv_obj_add_event_cb(s->rows[i],pointer,LV_EVENT_CLICKED,nullptr);lv_obj_add_flag(s->marks[i],LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(s->marks[i],pointer,LV_EVENT_CLICKED,nullptr);
        }
        lv_obj_move_foreground(s->message);refresh();return true;
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){close();StatusBar::showActiveNotice(tr("Low memory","Sin memoria"));return false;}
#endif
}

bool text(const char* utf8) {
    TOOLBOX_MEASURE(2);
    if(!s)return false;
    if(s->options || s->help)return true;
    if(!searching())selectTab(Search);
    // Insert complete, validated UTF-8 codepoints; reject overlong sequences.
    for(auto* p=reinterpret_cast<const uint8_t*>(utf8);*p;) {
        size_t n=*p<0x80?1:(*p>=0xc2&&*p<=0xdf)?2:(*p>=0xe0&&*p<=0xef)?3:(*p>=0xf0&&*p<=0xf4)?4:0;
        if(!n)break;
        bool valid=true;for(size_t i=1;i<n;++i)if(!p[i] || (p[i]&0xc0)!=0x80){valid=false;break;}
        if(n>=3 && ((*p==0xe0 && p[1]<0xa0) || (*p==0xed && p[1]>=0xa0) ||
           (*p==0xf0 && p[1]<0x90) || (*p==0xf4 && p[1]>=0x90)))valid=false;
        if(!valid)break;
        if(s->queryLength+n>=sizeof(s->query)) {
            // WHY: a single text event can accept several complete characters
            // before reaching the limit. Publish that prefix before the notice.
            changedQuery();notice("Search limit: 64 bytes.","Límite de búsqueda: 64 bytes.");return true;
        }
        if(*p>=32) {
            std::memmove(s->query+s->queryCursor+n,s->query+s->queryCursor,s->queryLength-s->queryCursor+1);
            std::memcpy(s->query+s->queryCursor,p,n);s->queryCursor+=n;s->queryLength+=n;
        }
        p+=n;
    }
    changedQuery();return true;
}
bool back() {
    if(!s)return false;
    if(s->help || s->options){s->help=s->options=false;refresh();return true;}
    if(s->tabsFocus){s->tabsFocus=false;refresh();return true;}
    if(s->depth){--s->depth;if(!s->depth)s->tab=s->tabCursor=0;s->queryFocus=false;refresh();return true;}
    requestClose();return true;
}
void closeOwner(void* owner) {if(s && s->receiver.owner==owner)close(false);}
bool handle(const KeyEvent& ev) {
    if(suppressed!=KeyCode::NONE) {
        if(ev.code==suppressed && ev.action==KeyAction::RELEASE){suppressed=KeyCode::NONE;return true;}
        if(ev.code==suppressed && ev.action==KeyAction::REPEAT)return true;
        if(ev.action==KeyAction::PRESS)suppressed=KeyCode::NONE;
    }
    if(!s)return false;
    if(ev.code==KeyCode::HOME || ev.code==KeyCode::MODE){close(false);return false;}
    if(ev.action!=KeyAction::PRESS && ev.action!=KeyAction::REPEAT)return true;
    const bool navigation=ev.code==KeyCode::UP||ev.code==KeyCode::DOWN||ev.code==KeyCode::LEFT||ev.code==KeyCode::RIGHT;
    if(ev.action==KeyAction::REPEAT && !navigation && ev.code!=KeyCode::DEL)return true;
    if(ev.code==KeyCode::SHIFT || ev.code==KeyCode::ALPHA){
        if(ev.code==KeyCode::ALPHA && !searching() && !s->help && !s->options)selectTab(Search);
        auto& km=vpam::KeyboardManager::instance();if(ev.code==KeyCode::SHIFT)km.pressShift();else km.pressAlpha();return true;
    }
    if(ev.code==KeyCode::BACK || ev.code==KeyCode::AC || ev.code==KeyCode::TOOLBOX){suppressed=ev.code;back();return true;}
    if(s->help){if(ev.code==KeyCode::EXE||ev.code==KeyCode::ENTER)back();return true;}
    if(s->options) {
        if(ev.code==KeyCode::UP && s->option)--s->option;
        if(ev.code==KeyCode::DOWN && s->option<3)++s->option;
        if(ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER){optionsAction();return true;}
        refresh();return true;
    }
    if(s->tabsFocus) {
        if(ev.code==KeyCode::LEFT)s->tabCursor=(s->tabCursor+3)%4;
        if(ev.code==KeyCode::RIGHT)s->tabCursor=(s->tabCursor+1)%4;
        if(ev.code==KeyCode::DOWN || ev.code==KeyCode::ENTER || ev.code==KeyCode::EXE){selectTab(s->tabCursor);return true;}
        refresh();return true;
    }
    if(s->queryFocus) {
        if(ev.code==KeyCode::UP){s->queryFocus=false;s->tabsFocus=true;s->tabCursor=s->tab;}
        else if(ev.code==KeyCode::LEFT)s->queryCursor=previous(s->queryCursor);
        else if(ev.code==KeyCode::RIGHT)s->queryCursor=next(s->queryCursor);
        else if(ev.code==KeyCode::DOWN || ev.code==KeyCode::EXE || ev.code==KeyCode::ENTER){s->queryFocus=false;vpam::KeyboardManager::instance().reset();}
        else if(ev.code==KeyCode::DEL && s->queryCursor) {
            const size_t before=previous(s->queryCursor);std::memmove(s->query+before,s->query+s->queryCursor,s->queryLength-s->queryCursor+1);s->queryLength-=s->queryCursor-before;s->queryCursor=before;changedQuery();return true;
        } else {
            if(ev.text && *ev.text){text(ev.text);return true;}
            const auto semantic=static_cast<numos::input::SemanticId>(ev.semanticId);
            if(semantic>=numos::input::SemanticId::alpha_A && semantic<=numos::input::SemanticId::alpha_Z) {
                char c[2]={char('a'+int(semantic)-int(numos::input::SemanticId::alpha_A)),0};text(c);return true;
            }
            const int digit=keyCodeDigitValue(ev.code);
            if(digit>=0){char c[2]={char('0'+digit),0};text(c);return true;}
        }
        refresh();return true;
    }
    if(ev.code==KeyCode::UP){if(s->level().selection)--s->level().selection;else if(s->level().group==Search)s->queryFocus=true;else{s->tabsFocus=true;s->tabCursor=s->tab;}}
    else if(ev.code==KeyCode::DOWN){if(s->level().selection+1<count())++s->level().selection;}
    else if(ev.code==KeyCode::LEFT){back();return true;}
    else if(ev.code==KeyCode::RIGHT){activate(true);return true;}
    else if(ev.code==KeyCode::ENTER || ev.code==KeyCode::EXE){suppressed=ev.code;activate();return true;}
    else if(ev.code==KeyCode::FORMAT || ev.code==KeyCode::FREE_EQ || ev.code==KeyCode::FORMAT_MENU) {
        const Row row=count()?at(s->level().selection):Row{};
        if(row.entry){s->optionEntry=row.entry;s->options=true;s->option=0;}
    }
    refresh();return true;
}
#ifdef NATIVE_SIM
Snapshot snapshot() {
    Snapshot state{};state.favorites=Store::instance().favoriteCount();state.recent=Store::instance().recentCount();
    if(!s)return state;
    state.open=true;state.queryFocus=s->queryFocus;state.group=s->level().group;state.selection=unsigned(s->level().selection);state.top=unsigned(s->level().top);state.count=unsigned(count());state.queryBytes=unsigned(s->queryLength);
    auto row=count()?at(s->level().selection):Row{};if(row.entry){state.id=row.entry->identity.id;state.variant=row.entry->identity.variant;}
    return state;
}
bool debug(const char* expected) {
    if(!std::strcmp(expected,"closed"))return !s;
    if(!s)return false;
    if(!std::strcmp(expected,"open"))return true;
    if(!std::strcmp(expected,"query")) {
        if(s->level().group!=Search)return false;
        size_t matches=0;
        for(size_t i=0;i<entryCount();++i)if(visibleEntry(*entryAt(i)) && discoverable(*entryAt(i)) && searchRank(*entryAt(i),s->query)<4)++matches;
        char visible[80];std::snprintf(visible,sizeof(visible),"%.*s%s%s",int(s->queryCursor),s->query,s->queryFocus?"|":"",s->query+s->queryCursor);
        return matches==s->matches && !std::strcmp(lv_label_get_text(s->path),visible);
    }
    if(!std::strcmp(expected,"focus")) {
        for(unsigned i=0;i<s->visible;++i) {
            const bool selected=s->options?s->option==i:!s->queryFocus && !s->tabsFocus && s->level().selection==s->level().top+i;
            if(lv_obj_get_style_border_width(s->rows[i],LV_PART_MAIN)!=(selected?1:0))return false;
        }
        return true;
    }
    if(!std::strcmp(expected,"geometry")) {
        lv_obj_update_layout(s->shield);
        lv_area_t popup;lv_obj_get_coords(s->panel,&popup);
        if(popup.x1<10 || popup.x2>309 || popup.y1<50 || popup.y2>237)return false;
        for(auto* tab:s->tabs)if(lv_obj_get_style_outline_width(tab,LV_PART_MAIN))return false;
        if(s->level().group==Search && !s->options && !s->help && !lv_obj_has_flag(s->message,LV_OBJ_FLAG_HIDDEN)) {
            lv_area_t query,message;lv_obj_get_coords(s->path,&query);lv_obj_get_coords(s->message,&message);
            if(query.y2>=message.y1 || message.y2>=popup.y2-FooterHeight)return false;
        }
        for(unsigned i=0;i<s->visible;++i) {
            lv_area_t row;lv_obj_get_coords(s->rows[i],&row);
            if(row.y2>=popup.y2-FooterHeight)return false;
            if(lv_obj_has_flag(s->canvases[i].obj(),LV_OBJ_FLAG_HIDDEN))continue;
            const auto& box=s->previews[i]->layout();
            if(box.width+16>lv_obj_get_width(s->canvases[i].obj()) || box.height()+2>lv_obj_get_height(s->canvases[i].obj()))return false;
        }
        return true;
    }
    if(!std::strcmp(expected,"dump")) {
        const Row row=count()?at(s->level().selection):Row{};
        std::printf("[TOOLBOX] group=%u selection=%u top=%u count=%u query=%s focus=%u id=%u variant=%u favorites=%u recent=%u widgets=28 previews=4 context=1 epoch=%u sessionBytes=%u\n",s->level().group,unsigned(s->level().selection),unsigned(s->level().top),unsigned(count()),s->query,s->queryFocus,row.entry?row.entry->identity.id:0,row.entry?row.entry->identity.variant:0,unsigned(Store::instance().favoriteCount()),unsigned(Store::instance().recentCount()),s->epoch,unsigned(sizeof(Session)));
        for(unsigned i=0;i<5;++i)std::printf("[TOOLBOX-TIME] scope=%u samples=%llu totalNs=%llu maxNs=%llu\n",i,(unsigned long long)measurements[i].samples,(unsigned long long)measurements[i].total,(unsigned long long)measurements[i].maximum);
        lv_mem_monitor_t memory{};lv_mem_monitor(&memory);std::printf("[TOOLBOX-MEM] free=%u largest=%u\n",unsigned(memory.free_size),unsigned(memory.free_biggest_size));return true;
    }
    unsigned id=0,variant=0;if(std::sscanf(expected,"selected %u %u",&id,&variant)==2){auto row=count()?at(s->level().selection):Row{};return row.entry&&row.entry->identity==Identity{uint16_t(id),uint16_t(variant)};}
    return false;
}
#endif
}
