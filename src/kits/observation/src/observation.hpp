#pragma once

// The Observation Kit (Tool slot 6): a list of every creature in the world
// with alerts for pregnancy, birth and low life force.
//
// Rebuilt from /C1 Kits/observation.exe ("Observation Kit - version 1.2.0");
// ../ORIGINAL.md describes the original's behaviour and numbers its bugs.
// This build keeps the original's protocol, registry settings and look, and
// fixes those bugs; each fix is marked "Fix (bug N)" where it is made.

#include "c1kitshell/kit_shell.hpp"
#include "c1kit/conversation.hpp"
#include "creature_monitor.hpp"

#include <memory>
#include <string>
#include <vector>

namespace observation {

class ObservationSheet;

// ---------------------------------------------------------------------------
// Overview page (dialog 130, "Details")
// ---------------------------------------------------------------------------

// Shows the sheet's latest poll.  Fix: the original polled from this page,
// so nothing was watched, and no alert raised, until the Details tab had
// been opened; polling now belongs to the sheet.
class OverviewPage : public CPropertyPage {
public:
    explicit OverviewPage(ObservationSheet& sheet);

    // UpdateOverviewList @ 0x00405b00 (display half).
    void show(const std::vector<c1kit::OverviewRecord>& records,
              const AlertSettings& settings);

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnSize(UINT type, int cx, int cy);
    DECLARE_MESSAGE_MAP()

private:
    int find_row(const CString& moniker) const;
    void set_row(int item, const c1kit::OverviewRecord& record, int icon);
    void fit_last_column();

    ObservationSheet& sheet_;
    CListCtrl list_;
    CImageList icons_;
    c1kitshell::ControlAnchors anchors_;
};

// ---------------------------------------------------------------------------
// Options page (dialog 143)
// ---------------------------------------------------------------------------

class WarnLevelEdit : public CEdit {
protected:
    afx_msg void OnChar(UINT character, UINT repeat, UINT flags);
    DECLARE_MESSAGE_MAP()
};

class OptionsPage : public CPropertyPage {
public:
    explicit OptionsPage(ObservationSheet& sheet);

protected:
    void DoDataExchange(CDataExchange* exchange) override;
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    BOOL OnKillActive() override;
    afx_msg void OnAlwaysOnTop();
    afx_msg void OnAbout();
    afx_msg void OnCloseKit();
    afx_msg void OnSettingChanged();
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()

private:
    void show_settings();   // sheet -> controls
    void store_settings();  // controls -> sheet

    ObservationSheet& sheet_;
    CSpinButtonCtrl spin_;
    WarnLevelEdit warn_edit_;
    c1kitshell::ControlAnchors anchors_;
    bool initialized_ = false;
    bool showing_ = false;
};

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

// Fix (bug 6): alerts are modeless windows, so the list keeps updating and
// several alerts can be open at once.  Each deletes itself when closed.
class AlertWindow : public CDialog {
public:
    static void open(CWnd& owner, const CString& text, int alert_type);

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    void OnCancel() override;
    void PostNcDestroy() override;
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()

private:
    AlertWindow(const CString& text, int alert_type);

    CString text_;
    int alert_type_ = 0;
    c1kitshell::PaletteBitmap face_;
    static std::vector<AlertWindow*> open_;  // for cascading and repeats
};

class AboutDialog : public CDialog {
public:
    AboutDialog();

protected:
    BOOL OnInitDialog() override;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

class ObservationSheet : public c1kitshell::KitSheet {
public:
    explicit ObservationSheet(CFont& default_font);
    ~ObservationSheet() override;

    bool create_window();

    const AlertSettings& settings() const { return settings_; }
    void set_settings(const AlertSettings& settings) { settings_ = settings; }
    bool always_on_top() const { return always_on_top_ != 0; }
    void set_always_on_top(bool on);

    const std::vector<c1kit::OverviewRecord>& records() const {
        return records_;
    }

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
    void initialize_pages();
    void load_preferences();
    void save_preferences();
    void update_pause();
    bool paused() const { return game_paused_ || minimised_; }
    void poll();
    bool load_overview();
    void raise_alerts(const std::vector<Alert>& alerts);

    CFont& default_font_;
    c1kitshell::CoverPage cover_;
    OverviewPage overview_;
    OptionsPage options_;
    c1kit::KitSettings* registry_ = nullptr;
    AlertSettings settings_;
    std::uint32_t always_on_top_ = 1;
    int saved_page_ = 0;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::vector<c1kit::OverviewRecord> records_;
    CreatureMonitor monitor_;
    bool connected_ = false;

    // Fix (bugs 5, 16): polling pauses exactly while the game is paused or
    // the window is minimised, worked out from these two facts.  The
    // original toggled the pages on each event and could fall out of step.
    bool game_paused_ = false;  // toggled by control state 9
    bool minimised_ = false;
};

} // namespace observation
