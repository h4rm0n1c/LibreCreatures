#pragma once

// The Object Injector (Tool slot 7): the COBs in a folder, to inject into
// the world or remove from it, and what each one does.
//
// Rebuilt from /C1 Kits/Injector.exe, the "Injector Kit 2.0" rebuild of the
// 1996 Object Injector; ../ORIGINAL.md describes its behaviour and numbers
// its bugs.  This build keeps its pages, protocol and settings, lays its
// pages out in code so they grow with the window, and fixes those bugs;
// each fix is marked "Fix (bug N)".  No cover page and no sound.

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/cob.hpp"
#include "c1kit/conversation.hpp"

#include <memory>
#include <string>
#include <vector>

namespace injector {

class InjectorSheet;

// One COB in the folder.
struct CobEntry {
    std::string path;
    c1kit::Cob cob;
};

// A page with the find box and the list of COBs, which both pages have.
class CobListPage : public c1kitshell::LayoutPage {
public:
    CobListPage(InjectorSheet& sheet, UINT title_string);
    // The list changed (reloaded, or a count changed).
    virtual void cobs_changed();

protected:
    void create_list();
    void place_list(int x, int y, int width, int height);
    void fill_list();
    int selected_index() const;  // into the sheet's entries, or -1
    virtual void selection_changed() {}
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnFindChanged();
    DECLARE_MESSAGE_MAP()

    InjectorSheet& sheet_;
    CStatic find_label_;
    CEdit find_;
    CListCtrl list_;
};

// COBs: the picture, description and count of the one picked; Inject and
// Remove.
class CobsPage : public CobListPage {
public:
    explicit CobsPage(InjectorSheet& sheet);
    void cobs_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    void selection_changed() override;
    afx_msg void OnInject();
    afx_msg void OnRemove();
    afx_msg void OnRefresh();
    afx_msg void OnBrowse();
    afx_msg void OnIgnoreAmount();
    afx_msg void OnAllowWithout();
    DECLARE_MESSAGE_MAP()

private:
    void show_selected();
    void draw_picture(CDC& dc, const CRect& rect);

    c1kitshell::PaintedView picture_;
    c1kitshell::Canvas canvas_;
    CEdit description_;
    CButton inject_;
    CButton remove_;
    CButton refresh_;
    CButton browse_;
    CStatic quantity_;
    CStatic folder_;
    CButton ignore_amount_;
    CButton allow_without_;
};

// Analysis: everything the COB's scripts do, as a tree.
class AnalysisPage : public CobListPage {
public:
    explicit AnalysisPage(InjectorSheet& sheet);

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    void selection_changed() override;

private:
    void fill_tree();

    CTreeCtrl tree_;
};

class InjectorSheet : public c1kitshell::KitSheet {
public:
    explicit InjectorSheet(CFont& default_font);
    ~InjectorSheet() override;

    bool create_window();

    std::vector<CobEntry>& entries() { return entries_; }
    const std::string& folder() const { return folder_; }
    void set_folder(const std::string& folder);
    void reload();
    const c1kitshell::GamePalette& palette() const { return palette_; }
    const std::vector<std::string>& chemical_names() const { return chemical_names_; }
    const std::vector<c1kit::ClassifierName>& classifier_names() const { return classifier_names_; }
    bool ignore_amount() const { return ignore_amount_ != 0; }
    bool allow_without_subject() const { return allow_without_ != 0; }
    void set_ignore_amount(bool on);
    void set_allow_without_subject(bool on);
    bool expired(const c1kit::Cob& cob) const;

    // The actions, with messages for the user in `why` when they fail.
    bool inject(CobEntry& entry, CString& why);
    bool remove(CobEntry& entry, CString& why);

protected:
    BOOL OnInitDialog() override;
    void on_control_state(std::uint8_t state) override;
    void before_game_quit() override;
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSysCommand(UINT id, LPARAM lparam);
    DECLARE_MESSAGE_MAP()

private:
    void load_preferences();
    void save_preferences();
    void update_title();
    bool run(const std::string& script);
    bool creature_selected();
    void set_always_on_top(bool on);
    std::string game_file(const std::string& name) const;

    CFont& default_font_;
    CobsPage cobs_page_;
    AnalysisPage analysis_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::vector<CobEntry> entries_;
    std::string folder_;
    c1kitshell::GamePalette palette_;
    std::vector<std::string> chemical_names_;
    std::vector<c1kit::ClassifierName> classifier_names_;
    std::string subject_name_;
    std::uint32_t ignore_amount_ = 0;
    std::uint32_t allow_without_ = 0;
    std::uint32_t always_on_top_ = 0;
    int saved_page_ = 0;
    bool connected_ = false;
};

} // namespace injector
