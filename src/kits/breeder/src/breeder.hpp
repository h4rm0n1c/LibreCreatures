#pragma once

// The Breeder's Kit (Tool slot 5): the selected creature's sex hormones,
// fertility and pregnancy, and a shop of aphrodisiacs.
//
// Rebuilt from /C1 Kits/Breeder's Kit.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  This build keeps its protocol
// and shop file, drops its cover page and sound, and replaces its pictures
// (gauges, a pregnancy silhouette, an unlabelled hormone trace) with labelled
// readings and a graph: each deviation is noted where it is made, and bug
// fixes are marked "Fix (bug N)".

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_shop.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/health_files.hpp"
#include "c1kit/science_files.hpp"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace breeder {

class BreederSheet;

// Fertility: pregnancy, the sex hormones now, and a graph of them.
class FertilityPage : public c1kitshell::LayoutPage {
public:
    explicit FertilityPage(BreederSheet& sheet);
    void poll();
    void subject_changed();

protected:
    void create_controls() override;
    void layout(int width, int height) override;

private:
    void draw(CDC& dc, const CRect& rect);
    void draw_graph(CDC& dc, const CRect& rect);

    BreederSheet& sheet_;
    c1kitshell::PaintedView view_;
    std::vector<int> chemicals_;              // followed, for this sex
    std::vector<std::deque<int>> history_;    // per chemical, newest last
    bool have_overview_ = false;
    std::string age_;
    std::string pregnancy_;
    int life_force_ = 0;
};

struct Subject {
    bool present = false;
    std::string moniker;
    std::string name;
    int sex = 0;  // 1 male, 2 female
};

class BreederSheet : public c1kitshell::KitSheet, private c1kitshell::ShopHost {
public:
    explicit BreederSheet(CFont& default_font);
    ~BreederSheet() override;

    bool create_window();

    const Subject& subject() const { return subject_; }
    bool query(const std::string& script, std::string& reply);
    const std::vector<std::string>& chemical_names() const { return chemical_names_; }

    // c1kitshell::ShopHost: the Aphrodisiac page's stock, "Aphro".
    std::vector<c1kit::ShopItem>& shop_items() override { return shop_; }
    bool run_shop_command(const std::string& script) override;
    bool save_shop() override;
    const c1kitshell::GamePalette& shop_palette() const override { return palette_; }
    const char* shop_file_name() const override { return kShopFileName; }

    static constexpr char kShopFileName[] = "Aphro";

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
    bool paused() const { return game_paused_ || minimised_; }
    void set_always_on_top(bool on);
    std::string game_file(const std::string& name) const;

    CFont& default_font_;
    FertilityPage fertility_page_;
    c1kitshell::ShopPage shop_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    Subject subject_;
    std::vector<std::string> chemical_names_;
    std::vector<c1kit::ShopItem> shop_;
    c1kitshell::GamePalette palette_;
    std::uint32_t always_on_top_ = 0;
    int saved_page_ = 0;
    bool connected_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace breeder
