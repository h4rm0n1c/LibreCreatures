#pragma once

// The Hatchery (Tool slot 0): six eggs in a nest; hatch one and it goes into
// the game's incubator.
//
// Rebuilt from /C1 Kits/Hatchery.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  The 1996 kit was a window
// showing the incubator machine with the eggs in its nest, animated fans
// and a scanner; a double-click on an egg hatched it and closed the kit.
// This build shows the nest with each egg's sex and parents, hatches on a
// double-click or a button, and stays open.  No sound.

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/hatchery.hpp"

namespace hatchery {

class HatcherySheet;

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
    void draw(CDC& dc, const CRect& rect);
    CRect egg_rect(const CRect& view, int slot) const;
    int egg_at(CPoint point) const;
    void load_art();

    HatcherySheet& sheet_;
    c1kitshell::PaintedView nest_;
    CButton hatch_;
    CButton refill_;
    CStatic status_;
    HBITMAP eggs_[c1kit::kEggCount] = {};
    HBITMAP female_ = nullptr;
    HBITMAP male_ = nullptr;
    CRect view_rect_;
    int selected_ = -1;
    int hover_ = -1;
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

protected:
    BOOL OnInitDialog() override;
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
    void save_nest();
    void set_always_on_top(bool on);

    CFont& default_font_;
    NestPage nest_page_;
    c1kit::KitSettings* registry_ = nullptr;
    c1kit::Nest nest_;
    std::uint32_t always_on_top_ = 0;
    bool connected_ = false;
};

} // namespace hatchery
