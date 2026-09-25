#pragma once

// The Funeral Kit (Tool slot 9): a memorial for each creature whose death
// the game reports, with its photographs from the Owner's Kit album, and a
// graveyard of the headstones made for them.
//
// Rebuilt from /C1 Kits/Funeral Kit.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  This build keeps the original's
// protocol and art, drops its cover page and sound, keeps its graves in a
// file of its own, and fixes those bugs; each fix is marked "Fix (bug N)".

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kit/funeral_files.hpp"
#include "c1kit/owner_files.hpp"

#include <memory>
#include <string>
#include <vector>

namespace funeral {

class FuneralSheet;

// Draws text centred in a rectangle, wrapping, in the kits' serif face.
class Lettering {
public:
    void create(int height, int weight);
    void draw(CDC& dc, const CRect& rect, const CString& text, COLORREF colour,
              COLORREF shadow) const;

private:
    CFont font_;
};

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

// A dead creature from the Register (CFuneralSheet, dialog 137): its
// photographs in GRAVE.bmp's frame, how long it lived, an epitaph, and
// Make Headstone.
class MemorialPage : public CPropertyPage {
public:
    MemorialPage(FuneralSheet& sheet, const std::string& moniker,
                 const CString& tab);
    const std::string& moniker() const { return moniker_; }
    void show();
    // Stores an edited epitaph; called before anything reads or saves it.
    void commit_epitaph();

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    BOOL OnKillActive() override;
    afx_msg void OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw);
    afx_msg HBRUSH OnCtlColor(CDC* dc, CWnd* window, UINT type);
    afx_msg void OnPreviousPhoto();
    afx_msg void OnNextPhoto();
    afx_msg void OnEpitaphKillFocus();
    afx_msg void OnMakeHeadstone();
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

private:
    void load_photos();
    void draw_picture();

    FuneralSheet& sheet_;
    std::string moniker_;
    CString tab_;
    std::vector<c1kit::Photo> photos_;
    int selected_photo_ = 0;
    CBitmapButton previous_;
    CBitmapButton next_;
    c1kitshell::Canvas picture_;
    Lettering lettering_;
    CRect life_span_box_;
    CBrush epitaph_brush_;
    bool initialized_ = false;
};

// A dead creature that was never registered (CUnMarkedPage, dialog 183).
class UnmarkedPage : public CPropertyPage {
public:
    UnmarkedPage(FuneralSheet& sheet, const std::string& moniker,
                 const std::string& death_time);
    const std::string& moniker() const { return moniker_; }

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw);
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

private:
    FuneralSheet& sheet_;
    std::string moniker_;
    std::string death_time_;
    CString tab_;
    c1kitshell::Canvas picture_;
    Lettering lettering_;
};

// The graveyard (CGravePage, dialog 182): funeral.bmp's headstone with a
// dead creature's name, life span and epitaph; the arrows walk the graves.
class GraveyardPage : public CPropertyPage {
public:
    explicit GraveyardPage(FuneralSheet& sheet);
    void show();
    void show_grave(const std::string& moniker);

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    afx_msg void OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw);
    afx_msg void OnPreviousGrave();
    afx_msg void OnNextGrave();
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

private:
    std::vector<const c1kit::Grave*> headstones() const;

    FuneralSheet& sheet_;
    std::string selected_;  // moniker of the grave shown
    CBitmapButton previous_;
    CBitmapButton next_;
    c1kitshell::Canvas picture_;
    Lettering name_lettering_;
    Lettering lettering_;
    CRect name_box_;
    CRect span_box_;
    CRect epitaph_box_;
    bool initialized_ = false;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

class FuneralSheet : public c1kitshell::KitSheet {
public:
    explicit FuneralSheet(CFont& default_font);
    ~FuneralSheet() override;

    bool create_window();

    std::vector<c1kit::Grave>& graves() { return graves_; }
    c1kit::Grave* grave(const std::string& moniker);
    const c1kitshell::GamePalette& palette() const { return palette_; }
    // A creature's name from the graves or the Register, or its moniker.
    CString name_for_moniker(const std::string& moniker) const;
    std::string world_file(const std::string& name) const;

    // Writes the graves file now.
    bool save_graves();
    // Make Headstone: mark the grave and show it in the graveyard.
    void make_headstone(const std::string& moniker);

protected:
    BOOL OnInitDialog() override;
    void on_integer_message(std::int32_t payload) override;
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
    void connect();
    void load_register();
    void load_graves();
    void creature_died(const std::string& moniker);
    // Adds a page for the grave (or the unmarked creature) unless there is
    // one; returns its index.
    int add_memorial_page(const std::string& moniker);
    int add_unmarked_page(const std::string& moniker, const std::string& death_time);
    int page_index_for(const std::string& moniker);
    void commit_epitaphs();
    void set_always_on_top(bool on);

    CFont& default_font_;
    GraveyardPage graveyard_page_;
    std::vector<std::unique_ptr<MemorialPage>> memorial_pages_;
    std::vector<std::unique_ptr<UnmarkedPage>> unmarked_pages_;
    c1kit::KitSettings* registry_ = nullptr;
    c1kitshell::GamePalette palette_;
    std::vector<c1kit::OwnerRecord> register_;
    std::vector<c1kit::Grave> graves_;
    std::vector<std::int32_t> early_deaths_;  // reported before the window was ready
    std::uint32_t always_on_top_ = 0;
    bool ready_ = false;
};

} // namespace funeral
