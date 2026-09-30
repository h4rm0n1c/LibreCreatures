#pragma once

// The Biochemistry Kit (Tool slot 1): chemical levels over time, injections
// (once or repeated), and the chemical names.
//
// Rebuilt from /C1 Kits/BiochemKit.exe (v1.2); ../ORIGINAL.md describes its
// behaviour and numbers its bugs.  This build keeps its pages, protocol,
// files and settings, lays its pages out in code so they grow with the
// window, and fixes those bugs; each fix is marked "Fix (bug N)".  No cover
// page, no syringe animation and no sound -- except in the classic skin (the
// original beside it as "BiochemKit.old"): the v1.2 kit's own pages and
// window (biochem_classic.cpp), sharing only the sheet's data with these.

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_graph.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_syringe.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/biochem_files.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/science_files.hpp"

#include <memory>
#include <string>
#include <vector>

namespace biochem {

class BiochemSheet;

// Fills a combo with every chemical, "Name (n)", whose name holds `filter`.
void fill_chemical_combo(CComboBox& combo, const std::vector<std::string>& names,
                         const CString& filter, int keep_chemical);
int combo_chemical(const CComboBox& combo);

class MonitorPage : public c1kitshell::LayoutPage {
public:
    explicit MonitorPage(BiochemSheet& sheet);
    void sample();
    void names_changed();
    void subject_changed();

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnFilterChanged();
    afx_msg void OnAdd();
    afx_msg void OnRemove();
    afx_msg void OnClear();
    afx_msg void OnLoad();
    afx_msg void OnSave();
    afx_msg void OnDelete();
    DECLARE_MESSAGE_MAP()

private:
    void set_followed(const std::vector<int>& chemicals);
    void fill_followed();
    void fill_saved();
    std::string saved_path() const;

    BiochemSheet& sheet_;
    CStatic filter_label_;
    CEdit filter_;
    CStatic chemical_label_;
    CComboBox chemical_;
    CButton add_;
    CButton remove_;
    CButton clear_;
    CStatic saved_label_;
    CComboBox saved_;
    CButton load_;
    CButton save_;
    CButton delete_;
    CListCtrl followed_;
    c1kitshell::PaintedView graph_;
    c1kitshell::ChemicalGraph plot_{8000, 1000};
};

class InjectPage : public c1kitshell::LayoutPage {
public:
    explicit InjectPage(BiochemSheet& sheet);
    void names_changed();
    void repeat_tick();

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnFilterChanged();
    afx_msg void OnChemicalChanged();
    afx_msg void OnInject();
    afx_msg void OnRepeatChanged();
    afx_msg void OnStop();
    afx_msg void OnAmountChanged();
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar* bar);
    DECLARE_MESSAGE_MAP()

private:
    bool inject_once();
    void stop_repeat(const CString& why);
    void update_repeat_controls();

    BiochemSheet& sheet_;
    CStatic chemical_label_;
    CComboBox chemical_;
    CStatic filter_label_;
    CEdit filter_;
    CStatic dose_label_;
    CSliderCtrl dose_;
    CEdit amount_;
    CButton inject_;
    CButton repeat_;
    CStatic every_label_;
    CEdit every_;
    CStatic count_label_;
    CEdit count_;
    CButton stop_;
    CStatic status_;
    bool repeating_ = false;
    int repeat_chemical_ = -1;
    int repeat_done_ = 0;
    int repeat_total_ = 0;
    bool syncing_ = false;
};

class NamesPage : public c1kitshell::LayoutPage {
public:
    explicit NamesPage(BiochemSheet& sheet);

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnSearchChanged();
    afx_msg void OnRename();
    afx_msg void OnSaveNames();
    DECLARE_MESSAGE_MAP()

private:
    void fill();
    int selected_chemical() const;

    BiochemSheet& sheet_;
    CStatic search_label_;
    CEdit search_;
    CListCtrl names_;
    CStatic index_;
    CStatic name_label_;
    CEdit name_;
    CButton rename_;
    CButton save_;
};

// ---------------------------------------------------------------------------
// The classic skin's pages (biochem_classic.cpp): the v1.2 kit's own, on its
// templates (dialogs 150, 151 and 152).
// ---------------------------------------------------------------------------

// A v1.2 page on its template.
class ClassicBiochemPage : public CPropertyPage {
public:
    ClassicBiochemPage(BiochemSheet& sheet, UINT dialog, UINT title_string);
    virtual void names_changed() {}

protected:
    BiochemSheet& sheet_;
    CString title_;
};

// Biochemistry (CMonitorPage, dialog 150).
class ClassicMonitorPage : public ClassicBiochemPage {
public:
    explicit ClassicMonitorPage(BiochemSheet& sheet);
    void sample();
    void subject_changed();
    void set_tooltips(bool on);
    void names_changed() override;

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    BOOL OnKillActive() override;
    afx_msg void OnFilterChanged();
    afx_msg void OnAdd();
    afx_msg void OnRemove();
    afx_msg void OnClear();
    afx_msg void OnLoad();
    afx_msg void OnSave();
    afx_msg void OnDelete();
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    DECLARE_MESSAGE_MAP()

private:
    void set_followed(const std::vector<int>& chemicals);
    void fill_followed();
    void fill_saved();
    std::string saved_path() const;

    CListCtrl followed_;
    c1kitshell::PaintedView graph_;
    c1kitshell::ChemicalGraph plot_{8000, 1000};
    bool tooltips_ = true;
};

// Injections (CInjectPage, dialog 151).
class ClassicInjectPage : public ClassicBiochemPage {
public:
    explicit ClassicInjectPage(BiochemSheet& sheet);
    void repeat_tick();
    void names_changed() override;

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnFilterChanged();
    afx_msg void OnChemicalChanged();
    afx_msg void OnInject();
    afx_msg void OnRepeatChanged();
    afx_msg void OnStop();
    afx_msg void OnAmountChanged();
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar* bar);
    afx_msg void OnTimer(UINT_PTR timer_id);
    DECLARE_MESSAGE_MAP()

private:
    bool inject_once();
    void stop_repeat(const CString& why);
    void update_repeat_controls();
    void change_liquid();
    int dose() const;
    void set_dose(int dose);

    c1kitshell::PaintedView syringe_view_;
    c1kitshell::Syringe syringe_;
    c1kitshell::GamePalette palette_;
    int liquid_ = -1;
    bool repeating_ = false;
    int repeat_chemical_ = -1;
    int repeat_done_ = 0;
    int repeat_total_ = 0;
    bool syncing_ = false;
};

// Chemical Names (CChemicalsPage, dialog 152).
class ClassicNamesPage : public ClassicBiochemPage {
public:
    explicit ClassicNamesPage(BiochemSheet& sheet);
    void names_changed() override;

protected:
    BOOL OnInitDialog() override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnSearchChanged();
    afx_msg void OnRename();
    afx_msg void OnSaveNames();
    DECLARE_MESSAGE_MAP()

private:
    void fill();
    int selected_chemical() const;

    CListCtrl names_;
};

class BiochemSheet : public c1kitshell::KitSheet {
public:
    explicit BiochemSheet(CFont& default_font);
    ~BiochemSheet() override;

    bool create_window();
    bool classic() const { return classic_ != nullptr; }
    // The classic row's Show Tooltips, seen only with the Biochemistry page.
    bool query(const std::string& script, std::string& reply);
    bool subject_present() const { return subject_present_; }
    std::vector<std::string>& chemical_names() { return chemical_names_; }
    bool save_chemical_names();
    void names_changed();
    std::string kit_file(const std::string& name) const;  // beside the exe
    c1kit::KitSettings* settings() { return registry_; }

protected:
    BOOL OnInitDialog() override;
    void on_control_state(std::uint8_t state) override;
    void before_game_quit() override;
    void before_skin_change() override;
    void add_kit_options(CMenu& options) override;
    bool on_kit_option(UINT id) override;
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()

private:
    void set_up_classic_window();
    void load_preferences();
    void save_preferences();
    void take_subject();
    std::string game_file(const std::string& name) const;

    CFont& default_font_;
    std::unique_ptr<c1kitshell::ClassicArt> classic_;
    std::unique_ptr<c1kitshell::CoverPage> cover_;
    std::unique_ptr<ClassicMonitorPage> classic_monitor_;
    std::unique_ptr<ClassicInjectPage> classic_inject_;
    std::unique_ptr<ClassicNamesPage> classic_names_;
    std::uint32_t tooltips_ = 1;
    MonitorPage monitor_page_;
    InjectPage inject_page_;
    NamesPage names_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::vector<std::string> chemical_names_;
    std::string subject_name_;
    bool subject_present_ = false;
    int saved_page_ = 0;
    bool connected_ = false;
    bool game_paused_ = false;
};

} // namespace biochem
