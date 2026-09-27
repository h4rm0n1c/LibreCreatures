#pragma once

// The Science Kit (Tool slot 4): the selected creature's biochemistry over
// time, its genome, a live view of its brain, its decisions, and medicine
// injections.
//
// Rebuilt from /C1 Kits/Science Kit.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  This build keeps its protocol
// and data files, drops its cover page and sound, and redesigns its pages:
// each deviation is noted where it is made, and bug fixes are marked
// "Fix (bug N)".

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_graph.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_syringe.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/brain_map.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/genome.hpp"
#include "c1kit/owner_files.hpp"
#include "c1kit/science_files.hpp"

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace science {

class ScienceSheet;

using c1kitshell::PaintedView;
using c1kitshell::blend;
using c1kitshell::lobe_colour;
using c1kitshell::series_colour;

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

// A page laid out in code (c1kitshell::LayoutPage) that the sheet polls.
class SciencePage : public c1kitshell::LayoutPage {
public:
    SciencePage(ScienceSheet& sheet, UINT title_string);

    // Called by the sheet's poll timer while this page is showing.
    virtual void poll() {}
    // The creature being looked at changed (or went).
    virtual void subject_changed() {}

protected:
    ScienceSheet& sheet_;
};

// Biochemistry: any number of chemicals (up to kMaxTrackedChemicals) plotted
// over time, picked from a checklist of every named chemical, and themes.
class BiochemistryPage : public SciencePage {
public:
    explicit BiochemistryPage(ScienceSheet& sheet);
    void sample();  // called on every other poll, showing or not
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnThemeChanged();
    afx_msg void OnSaveTheme();
    afx_msg void OnDeleteTheme();
    afx_msg void OnClearGraph();
    DECLARE_MESSAGE_MAP()

private:
    void fill_chemical_list();
    void fill_themes();
    void set_tracked(const std::vector<int>& chemicals);
    void tracked_from_list();
    void refresh_levels_column();
    void draw_graph(CDC& dc, const CRect& rect);
    int series_index(int chemical) const;
    void save_selection();

    CListCtrl chemicals_;
    CComboBox themes_;
    CButton save_theme_;
    CButton delete_theme_;
    CButton clear_;
    CStatic theme_label_;
    PaintedView graph_;
    CImageList swatches_;
    c1kitshell::ChemicalGraph plot_{2000, 500};
    std::vector<int> list_chemicals_;  // chemical per list row
    bool updating_list_ = false;
};

// Genetics: everything known about the creature's genome, listed as
// property and value, as a file's Details are.
class GeneticsPage : public SciencePage {
public:
    explicit GeneticsPage(ScienceSheet& sheet);
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;

private:
    void fill();
    void add_heading(const CString& text);
    void add_row(const CString& property, const CString& value);

    CListCtrl details_;
    CFont heading_font_;
    std::vector<bool> heading_rows_;
};

// Brain: the 64 x 64 grid with every lobe outlined and named and each
// neuron shaded by the brain report; pointing at a neuron says what it is,
// clicking it follows it.
class BrainPage : public SciencePage {
public:
    explicit BrainPage(ScienceSheet& sheet);
    void poll() override;
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnModeChanged();
    afx_msg void OnWiringChanged();
    afx_msg void OnLayoutToggled();
    DECLARE_MESSAGE_MAP()

private:
    void draw_grid(CDC& dc, const CRect& rect);
    // Where the lobes are drawn: arranged by how they feed each other
    // (c1kit::arrange_lobes), or where the genome puts them on the grid.
    // Values are always read at the real grid positions.
    void arrange();
    const std::vector<c1kit::LobeLayout>& shown() const;
    bool grid_positions() const;
    // The genome's wiring for the subject (brain_wiring), once per subject.
    void load_wiring();
    bool wiring_shown() const;
    // The Lines choice: 0 none, 1 between lobes, 2 dendrites.
    int lines_mode() const;
    // Each neuron's dendrites from the game (LibreCreatures' `dde: dend`),
    // when it has them and Lines asks for them.
    bool dendrites_shown() const;
    void refresh_dendrites();
    const std::vector<c1kit::Dendrite>* dendrites_of(int lobe, int neuron, int rule) const;
    void draw_wiring(CDC& dc, const std::vector<CRect>& outlines);
    // "Fed by ... / Feeds ..." for a lobe.
    CString wiring_text(int lobe) const;
    // Where one neuron's dendrites land, rule by rule.
    CString reach_text(int lobe, int neuron) const;
    // A lobe's firing: the share of its neurons firing and their mean.
    void lobe_firing(int lobe, int& share_percent, int& mean) const;
    void fit_view(const CRect& rect);
    bool cell_at(CPoint point, int& x, int& y) const;
    void on_mouse(CPoint point, bool clicked);
    void show_neuron_info();
    void fill_lobe_list();
    void update_lobe_list();
    void select_lobe(int lobe);
    void refresh_exact();
    // Grid cell (x, y) of lobe `lobe`: its exact value if the lobe is
    // selected and it has been read, else the report's estimate; `exact`
    // says which.  -1 if the report does not list it (it is at zero).
    int cell_value(int lobe, int x, int y, bool& exact) const;
    int report_mode() const;
    int report_rule() const;
    // Whether the brain report gives the chosen measure (always so for
    // firing strength; for the rest, only where the game runs the report's
    // script).
    bool report_covers_measure() const;

    PaintedView grid_;
    CComboBox mode_;
    CComboBox rule_;
    CStatic mode_label_;
    CListCtrl lobes_;
    CEdit info_;
    c1kit::BrainActivity activity_;
    // The cells drawn (arranged, the lobes' extent; on the grid, the whole
    // grid from (0, 0)), and where: grid cell (view_x_, view_y_) at
    // view_origin_ (scrolled, when the grid is larger than the page),
    // scale_ pixels a cell.
    int view_x_ = 0;
    int view_y_ = 0;
    int view_width_ = c1kit::kStandardBrainGrid;
    int view_height_ = c1kit::kStandardBrainGrid;
    CPoint view_origin_;
    CRect painted_;  // the whole painting, in view coordinates
    double scale_ = 0;
    int hover_lobe_ = -1;
    int hover_neuron_ = -1;
    int followed_lobe_ = -1;
    int followed_neuron_ = -1;
    c1kit::NeuronValues followed_values_[2];
    bool followed_valid_ = false;
    // Exact values (`dde: cell`, a few dozen a query, round and round) by
    // lobe and neuron, -1 where not read yet: the selected lobe's for firing
    // strength, every lobe's for the other measures.
    int selected_lobe_ = -1;
    std::vector<std::vector<int>> exact_;
    int exact_mode_ = -1;
    int exact_rule_ = -1;
    int exact_lobe_ = 0;
    int exact_next_ = 0;
    bool updating_list_ = false;
    // Whether the game honours a report's measure: -1 not known yet.
    int report_honours_measure_ = -1;
    CComboBox lines_;
    CStatic lines_label_;
    int capabilities_ = -1;  // the game's (c1kit::parse_capabilities), -1 not asked
    // By lobe, neuron and rule; refreshed a few queries a poll.
    std::vector<std::vector<std::array<std::vector<c1kit::Dendrite>, 2>>> dendrites_;
    std::size_t dendrite_pair_ = 0;
    int dendrite_next_ = 0;
    CButton layout_check_;
    std::vector<c1kit::LobeLayout> shown_;
    std::vector<c1kit::LobeWiring> wiring_;
    bool wiring_loaded_ = false;
    bool wiring_valid_ = false;
    // Firing strength, for the connections, whatever the map shows.
    c1kit::BrainActivity firing_;
    int pulse_frame_ = 0;
};

// Decisions: the decision lobe, one bar per action, the strongest marked,
// with the reward and punishment chemicals.
class DecisionsPage : public SciencePage {
public:
    explicit DecisionsPage(ScienceSheet& sheet);
    void poll() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnValueChanged();
    DECLARE_MESSAGE_MAP()

private:
    void draw_bars(CDC& dc, const CRect& rect);

    PaintedView bars_;
    CComboBox value_;
    CStatic value_label_;
    CBitmap reward_;
    CBitmap punishment_;
    std::vector<std::array<int, 7>> neurons_;
    int reward_level_ = 0;
    int punishment_level_ = 0;
};

// Injections: a medicine, a dose, Inject, and the medicine's level now.
class InjectionsPage : public SciencePage {
public:
    explicit InjectionsPage(ScienceSheet& sheet);
    void poll() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnInject();
    afx_msg void OnMedicineChanged();
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar* bar);
    DECLARE_MESSAGE_MAP()

private:
    void update_dose_label();
    int selected_chemical() const;

    CListBox medicines_;
    CSliderCtrl dose_;
    CStatic dose_label_;
    CStatic level_;
    CStatic medicine_label_;
    CButton inject_;
};

// ---------------------------------------------------------------------------
// The classic skin's pages (science_classic.cpp): the 1996 pages themselves,
// on their own templates, sharing only the sheet's data and queries with
// the pages above.
// ---------------------------------------------------------------------------

// A 1996 page: its template, its Close, and a poll timer while it shows.
class ClassicSciencePage : public CPropertyPage {
public:
    ClassicSciencePage(ScienceSheet& sheet, UINT dialog, UINT title_string, UINT poll_ms);

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    BOOL OnKillActive() override;
    virtual void init() = 0;
    virtual void poll() {}
    virtual void tick() {}  // the animation timer, for a page that has one
    void start_animation(UINT ms) { animation_ms_ = ms; }
    // A drawn view where the template's picture control was.
    void replace_picture(UINT control, PaintedView& view, PaintedView::Painter painter);
    // The template's image statics (id -1), in order, given the original's bitmaps.
    void set_template_bitmaps(std::initializer_list<UINT> bitmaps);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

    ScienceSheet& sheet_;
    CString title_;
    UINT poll_ms_;
    UINT animation_ms_ = 0;
};

// Biochemistry (CMonitorPage, dialog 133): four chemicals on the 1996 graph,
// themes of four.
class ClassicMonitorPage : public ClassicSciencePage {
public:
    explicit ClassicMonitorPage(ScienceSheet& sheet);

protected:
    void init() override;
    void poll() override;
    afx_msg void OnChemicalChanged();
    afx_msg void OnThemeChanged();
    afx_msg void OnAddTheme();
    afx_msg void OnRemoveTheme();
    DECLARE_MESSAGE_MAP()

private:
    void fill_themes();
    int chemical_in(int slot) const;
    void draw_graph(CDC& dc, const CRect& rect);

    PaintedView graph_;
    std::vector<std::array<int, 4>> samples_;  // oldest first
};

// Genetics (CChromosonePage, dialog 143): the creature's genetic summary,
// the spinning DNA, and the breakdown a gene type at a time.
class ClassicGeneticsPage : public ClassicSciencePage {
public:
    explicit ClassicGeneticsPage(ScienceSheet& sheet);

protected:
    BOOL OnSetActive() override;
    void init() override;
    void tick() override;
    afx_msg void OnNext();
    DECLARE_MESSAGE_MAP()

private:
    void fill();
    void show_group();
    void draw_dna(CDC& dc, const CRect& rect);

    PaintedView dna_;
    c1kitshell::KitSprite gene_;
    c1kitshell::Canvas canvas_;
    c1kitshell::GamePalette palette_;
    CFont title_font_;
    std::vector<int> counts_;
    int group_ = 1;
    int frame_ = 1;
};

// Brain scanner (CScannerPage, dialog 134): the brain report's neurons as
// dots on Scanner.bmp.
class ClassicScannerPage : public ClassicSciencePage {
public:
    explicit ClassicScannerPage(ScienceSheet& sheet);

protected:
    void init() override;
    void poll() override;

private:
    void draw(CDC& dc, const CRect& rect);

    PaintedView picture_;
    c1kitshell::Canvas canvas_;
    c1kitshell::GamePalette palette_;
    std::unique_ptr<c1kit::BrainActivity> activity_;
    bool have_report_ = false;
};

// Decisions (CDecisionPage, dialog 135): a short bar per action, and the
// reward and punishment chemicals.
class ClassicDecisionsPage : public ClassicSciencePage {
public:
    explicit ClassicDecisionsPage(ScienceSheet& sheet);

protected:
    void init() override;
    void poll() override;
    afx_msg void OnDrawItem(int id, LPDRAWITEMSTRUCT draw);
    DECLARE_MESSAGE_MAP()

private:
    std::vector<int> outputs_;
    int reward_ = 0;
    int punishment_ = 0;
    CBitmap reward_bitmap_;
    CBitmap punishment_bitmap_;
};

// Injections (CInjectPage, dialog 144): the syringe, its slider, the
// medicines and Go.
class ClassicInjectionsPage : public ClassicSciencePage {
public:
    explicit ClassicInjectionsPage(ScienceSheet& sheet);

protected:
    void init() override;
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar* bar);
    afx_msg void OnMedicineChanged();
    afx_msg void OnGo();
    DECLARE_MESSAGE_MAP()

private:
    void tick() override;
    int dose() const;
    void set_dose(int dose);
    void change_liquid();

    PaintedView syringe_view_;
    c1kitshell::Syringe syringe_;
    c1kitshell::GamePalette palette_;
    int liquid_ = -1;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

struct Subject {
    bool present = false;
    std::string moniker;  // Register spelling ("4b5a4633")
    std::string name;
    int sex = 0;          // 1 male, 2 female
    std::string age;
};

class ScienceSheet : public c1kitshell::KitSheet {
public:
    explicit ScienceSheet(CFont& default_font);
    ~ScienceSheet() override;

    bool create_window();
    // The classic look: the original beside this one as "Science Kit.old".
    bool classic() const { return classic_ != nullptr; }
    const c1kitshell::ClassicArt* classic_art() const { return classic_.get(); }

    CFont& font() { return default_font_; }
    const Subject& subject() const { return subject_; }
    bool paused() const { return game_paused_ || minimised_; }
    // A query on the kit's one holder (a mode-1 holder, kept).
    bool query(const std::string& script, std::string& reply);
    // `dde: lobe`, whose reply is binary.
    bool query_binary(const std::string& script, std::string& reply);
    // A brain report (mode-2 holder).
    bool brain_report(int mode, int rule, std::string& reply);
    // The subject's genome file (in the world's Genetics directory), parsed;
    // `file_bytes` its length.  False if there is none.
    bool read_genome(std::vector<c1kit::Gene>& genes, std::size_t& file_bytes) const;
    // Runs a script without waiting for output (a scheduler holder).
    bool run(const std::string& script);

    const std::vector<std::string>& chemical_names() const { return chemical_names_; }
    const c1kit::NeuronNames& neuron_names() const { return neuron_names_; }
    const std::vector<std::string>& medicines() const { return medicines_; }
    std::vector<c1kit::ChemicalTheme>& themes() { return themes_; }
    bool save_themes();
    const std::vector<c1kit::LobeLayout>& lobes() const { return lobes_; }
    CString name_for_moniker(const std::string& moniker) const;
    std::string world_file(const std::string& name) const;
    std::string game_file(const std::string& name) const;

    c1kit::KitSettings* settings() { return registry_; }

protected:
    BOOL OnInitDialog() override;
    void on_control_state(std::uint8_t state) override;
    void before_game_quit() override;
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSysCommand(UINT id, LPARAM lparam);
    DECLARE_MESSAGE_MAP()

private:
    void load_preferences();
    void save_preferences();
    void load_data_files();
    void connect();
    void take_subject();
    void update_title();
    void set_always_on_top(bool on);

    CFont& default_font_;
    std::unique_ptr<c1kitshell::ClassicArt> classic_;
    std::unique_ptr<c1kitshell::CoverPage> cover_;
    std::unique_ptr<ClassicMonitorPage> classic_monitor_;
    std::unique_ptr<ClassicGeneticsPage> classic_genetics_;
    std::unique_ptr<ClassicScannerPage> classic_scanner_;
    std::unique_ptr<ClassicDecisionsPage> classic_decisions_;
    std::unique_ptr<ClassicInjectionsPage> classic_injections_;
    BiochemistryPage biochemistry_page_;
    GeneticsPage genetics_page_;
    BrainPage brain_page_;
    DecisionsPage decisions_page_;
    InjectionsPage injections_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::unique_ptr<c1kit::MacroConversation> report_conversation_;
    Subject subject_;
    std::vector<std::string> chemical_names_;
    c1kit::NeuronNames neuron_names_;
    std::vector<std::string> medicines_;
    std::vector<c1kit::ChemicalTheme> themes_;
    std::vector<c1kit::OwnerRecord> register_;
    std::vector<c1kit::LobeLayout> lobes_;
    std::uint32_t always_on_top_ = 0;
    int saved_page_ = 0;
    unsigned poll_count_ = 0;
    bool connected_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace science
