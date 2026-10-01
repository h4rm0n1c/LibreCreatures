#pragma once

// The Hatchery (Tool slot 0): six eggs in a nest; hatch one and it goes into
// the game's incubator.
//
// Rebuilt from /C1 Kits/Hatchery.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  The 1996 kit was a window
// showing the incubator machine with the eggs in its nest, animated fans
// and a scanner; a double-click on an egg hatched it and closed the kit.
// This build shows the six eggs in a lamp-lit nest, drawn from the game's
// own egg sprites (Images\eggs.spr, or eggs.s32 / eggs@2x.s32 where the
// game has them) at a whole-number scale, each with its
// sex; it hatches on a double-click or a button, and stays open.  No sound.

#include "c1kitshell/game_art.hpp"
#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_sound.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/game_sprite.hpp"
#include "c1kit/hatchery.hpp"

#include <cstdint>
#include <vector>

namespace hatchery {

class HatcherySheet;

// Draws `bitmap` into `target` (nearest-neighbour when scaled), leaving out
// its `key`-coloured pixels; `fade_percent` washes it towards `fade_to`.
void draw_keyed(CDC& dc, HBITMAP bitmap, const CRect& target, int fade_percent = 0,
                COLORREF fade_to = RGB(0, 0, 0), COLORREF key = RGB(0, 255, 0));

// The classic look (hatchery_classic.cpp): the 1996 machine, 320 x 240 --
// the incubator from Hatchery\hatchery.bmp with its fans, flickering lamp
// and scanner, the eggs in its nest, the pointed-at egg wobbling under its
// spinning sex symbol, and the 1996 sounds -- all read from the game's own
// Hatchery and Sounds folders at run time.
class MachinePage : public CPropertyPage {
public:
    explicit MachinePage(HatcherySheet& sheet);
    ~MachinePage() override;
    void refresh();

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
    afx_msg void OnDestroy();
    afx_msg BOOL OnEggTip(UINT id, NMHDR* header, LRESULT* result);
    BOOL PreTranslateMessage(MSG* message) override;
    DECLARE_MESSAGE_MAP()

private:
    void hatch_at(CPoint point);
    void load_art();
    void compose(CDC& dc);
    int egg_at(CPoint point) const;
    void keep_ambience();

    HatcherySheet& sheet_;
    HBITMAP background_ = nullptr;
    HBITMAP front_ = nullptr;
    HBITMAP eggs_[c1kit::kEggCount] = {};
    HBITMAP genspin_ = nullptr;
    HBITMAP fans_[4] = {};
    HBITMAP scans_[6] = {};
    HBITMAP lamp_off_ = nullptr;
    int counter_ = 0;
    int wobble_[c1kit::kEggCount] = {};
    int pointed_ = -1;
    int last_click_egg_ = -1;
    DWORD last_click_time_ = 0;
    int fan_channel_ = -1;
    int scanner_channel_ = -1;
    int egg_channel_ = -1;
    CToolTipCtrl tips_;
    CString tip_text_;  // the shown tip's text, kept while it shows
};

class NestPage : public c1kitshell::LayoutPage {
public:
    explicit NestPage(HatcherySheet& sheet);
    ~NestPage() override;
    void refresh();

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnHatch();
    afx_msg void OnRefill();
    DECLARE_MESSAGE_MAP()

private:
    // The eggs' three looks: whole, incubating (selected), cracked (taken).
    enum Look { kWhole, kIncubating, kCracked, kLooks };

    void draw(CDC& dc, const CRect& rect);
    // Lays the nest out in `view`: the scale, the eggs' baseline, and each
    // slot's egg.
    void fit(const CRect& view);
    CRect egg_rect(int slot) const;
    int egg_at(CPoint point) const;
    void load_art();
    // The panel behind the eggs and their labels -- gradient, lamp glow,
    // ground, shadows, the selection and the sex symbols -- drawn pixel by
    // pixel so that it can be soft and anti-aliased.
    void render_panel(const CRect& view);
    void describe_selection();
    void play_selection_sound();
    void stop_tone();

    HatcherySheet& sheet_;
    c1kitshell::PaintedView nest_;
    CButton hatch_;
    CButton refill_;
    CStatic status_;
    HBITMAP egg_art_[c1kit::kEggCount][kLooks] = {};
    // From eggs@2x.s32, drawn instead when the eggs are at least doubled.
    HBITMAP egg_art_2x_[c1kit::kEggCount][kLooks] = {};
    CSize egg_size_;  // one frame, unscaled
    CRect view_rect_;
    int scale_ = 1;
    int baseline_ = 0;
    std::vector<std::uint32_t> panel_;  // BGRA, view_rect_'s size
    int selected_ = -1;
    int hover_ = -1;
    int tone_channel_ = -1;
};

class HatcherySheet : public c1kitshell::KitSheet {
public:
    explicit HatcherySheet(CFont& default_font);
    ~HatcherySheet() override;

    bool create_window();
    const c1kit::Nest& nest() const { return nest_; }
    // Hatches egg `slot`; false (with the reason) if it could not.
    bool hatch(int slot, CString& why);
    void refill();
    std::string game_file(const std::string& name) const;
    // With Scramble Eggs on: "mum4 x dad1" for egg `slot`, else empty.
    CString egg_parents_text(int slot) const;
    bool connected() const { return connected_; }
    // The classic look: the player's original Hatchery beside this one.
    bool classic() const { return classic_ != nullptr; }
    c1kitshell::KitSound& sound() { return sound_; }
    // Options > Mute sounds.
    bool muted() const { return c1kitshell::KitSound::muted(); }

protected:
    BOOL OnInitDialog() override;
    void before_game_quit() override;
    void before_skin_change() override;
    void add_kit_options(CMenu& options) override;
    bool on_kit_option(UINT id) override;
    void on_sounds_muted_changed() override;
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnInitMenuPopup(CMenu* menu, UINT index, BOOL system_menu);
    afx_msg void OnAbout();
    afx_msg void OnWindowPosChanging(WINDOWPOS* position);
    DECLARE_MESSAGE_MAP()

private:
    void set_up_classic_window();
    // Sizes the window so its inside is the machine, exactly.
    void fit_classic_window();
    CSize classic_window_size() const;
    void load_preferences();
    void save_preferences();
    void save_nest();
    void save_parents();
    // Every egg's own pair, or with Scramble Eggs a new pair for each.
    void pick_parents();
    // With Scramble Eggs on, no egg keeps a same-number pair, however the
    // mode was turned on (the menu, or the setting changed while closed).
    void ensure_scrambled();
    // Make My Creatures Colourful: crosses the egg's genome, adds colour
    // genes to its file, and answers the genome; false if any step fails
    // (the egg is then made the plain way).
    bool colourful_genome(c1kit::MacroTransport& game, int mum, int dad,
                          std::int32_t& genome);

    CFont& default_font_;
    NestPage nest_page_;
    MachinePage machine_page_;
    std::unique_ptr<c1kitshell::ClassicArt> classic_;
    CSize classic_correction_{0, 0};  // see fit_classic_window
    c1kitshell::KitSound sound_;
    CMenu classic_menu_;
    c1kit::KitSettings* registry_ = nullptr;
    c1kit::Nest nest_;
    // Options > Scramble Eggs (saved as "Scramble Eggs").
    std::uint32_t scramble_eggs_ = 0;
    // Options > Make My Creatures Colourful (saved as "Colourful Eggs").
    std::uint32_t colourful_eggs_ = 0;
    // Each egg's parents (saved as "Egg Parents"): its own pair, or with
    // Scramble Eggs the pair picked when the nest was filled.
    c1kit::NestParents parents_;
    bool connected_ = false;
};

} // namespace hatchery
