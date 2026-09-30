#pragma once

// The Score Kit, titled "Performance Kit" (Tool slot 8): the game's five
// score counters, an icon picture of eggs and norns, the Breeders Score and
// the elapsed world time.
//
// Rebuilt from /C1 Kits/Score Kit.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  This build keeps the
// original's protocol, registry settings and art, drops its cover page and
// sound, and fixes those bugs; each fix is marked "Fix (bug N)".  With the
// original beside it as "Score Kit.old" it wears the classic look: the 1996
// fixed window, the cover page and the looping sound (which can be muted),
// with every fix kept.

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kit/conversation.hpp"

#include <memory>

namespace score {

class ScoreSheet;

// Everything the page shows.
struct ScoreState {
    c1kit::ScoreValues values;
    int hours = 0;
    int minutes = 0;
};

// ---------------------------------------------------------------------------
// Score page (dialog 138)
// ---------------------------------------------------------------------------

class ScorePage : public CPropertyPage {
public:
    explicit ScorePage(ScoreSheet& sheet);

    void show(const ScoreState& state);
    void set_colon_visible(bool visible);

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw);
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar* bar);
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()

private:
    void load_art();
    CSize control_size(unsigned control_id) const;
    void draw_number(c1kitshell::Canvas& canvas, unsigned control_id,
                     const char* backdrop, int value, int digits);
    void draw_time();
    void draw_panel();
    void update_scroll_range();
    void invalidate_control(unsigned control_id);

    ScoreSheet& sheet_;
    c1kitshell::GamePalette palette_;
    c1kitshell::KitSprite digits_;
    c1kitshell::KitSprite icons_;
    c1kitshell::KitSprite colon_;
    c1kitshell::Canvas counters_[4];
    c1kitshell::Canvas breeders_score_;
    c1kitshell::Canvas elapsed_time_;
    c1kitshell::Canvas panel_;
    c1kitshell::ControlAnchors anchors_;
    ScoreState state_;
    bool colon_visible_ = true;
    int scroll_x_ = 0;
    bool initialized_ = false;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

class ScoreSheet : public c1kitshell::KitSheet {
public:
    explicit ScoreSheet(CFont& default_font);
    ~ScoreSheet() override;

    bool create_window();
    const ScoreState& state() const { return state_; }
    bool classic() const { return classic_ != nullptr; }

protected:
    BOOL OnInitDialog() override;
    void on_integer_message(std::int32_t payload) override;
    void on_control_state(std::uint8_t state) override;
    void before_game_quit() override;
    void before_skin_change() override;
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()

private:
    void load_preferences();
    void save_preferences();
    void connect();
    void refresh();
    void update_pause();
    bool paused() const { return game_paused_ || minimised_; }

    CFont& default_font_;
    std::unique_ptr<c1kitshell::ClassicArt> classic_;
    std::unique_ptr<c1kitshell::CoverPage> cover_;  // classic only
    ScorePage page_;
    int saved_page_ = 1;  // classic: the page to open on, the cover being 0
    CString page_title_;  // classic: "Performance page"
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    ScoreState state_;
    int ticks_until_refresh_ = 0;
    bool colon_visible_ = true;
    bool connected_ = false;
    bool refresh_pending_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace score
