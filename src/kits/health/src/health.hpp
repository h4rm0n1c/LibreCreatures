#pragma once

// The Health Kit (Tool slot 3): how the selected creature is doing (its
// vital signs, drives and brain activity) and a shop of herbs and food for
// it.
//
// Rebuilt from /C1 Kits/Health Kit.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  This build keeps its protocol
// and shop file, drops its cover page and sound, and replaces its pictures
// (a thermometer, a heart monitor, gauges, a cartoon brain) with labelled
// readings: each deviation is noted where it is made, and bug fixes are
// marked "Fix (bug N)".  The classic look (the original beside it as
// "Health Kit.old") brings the 1996 pages back -- their pictures drawn from
// the game's files as the original drew them -- with the cover and the
// looping sound, keeping every fix.

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_shop.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/brain_map.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/health_files.hpp"
#include "c1kit/science_files.hpp"

#include <memory>
#include <string>
#include <vector>

namespace health {

class HealthSheet;

// A page laid out in code that the sheet polls while it is showing.
class HealthPage : public c1kitshell::LayoutPage {
public:
    HealthPage(HealthSheet& sheet, UINT title_string);
    virtual void poll() {}
    virtual void subject_changed() {}

protected:
    HealthSheet& sheet_;
};

// Fitness: life force, health, temperature, breathing and energy stores.
class FitnessPage : public HealthPage {
public:
    explicit FitnessPage(HealthSheet& sheet);
    void poll() override;
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;

private:
    void draw(CDC& dc, const CRect& rect);

    c1kitshell::PaintedView vitals_;
    bool have_readings_ = false;
    int life_force_ = 0;  // percent
    std::string health_;
    std::string age_;
    std::string pregnancy_;
    std::vector<int> chemicals_;  // as kFitnessChemicals
};

// Drives and needs: every drive, strongest marked.
class DrivesPage : public HealthPage {
public:
    explicit DrivesPage(HealthSheet& sheet);
    void poll() override;
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;

private:
    void draw(CDC& dc, const CRect& rect);

    c1kitshell::PaintedView drives_;
    std::vector<int> levels_;
};

// Brain activity: how busy each lobe is, and what it does.
class BrainPage : public HealthPage {
public:
    explicit BrainPage(HealthSheet& sheet);
    void poll() override;
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;

private:
    void draw(CDC& dc, const CRect& rect);

    c1kitshell::PaintedView lobes_;
    c1kit::BrainActivity activity_;
    bool have_report_ = false;
};

// ---------------------------------------------------------------------------
// The classic look's pages: the 1996 dialogs (health_classic.cpp)
// ---------------------------------------------------------------------------

// A 1996 page: its template, a drawn view in place of each picture control,
// its Close, and a poll timer that runs while the page is showing.
class ClassicHealthPage : public CPropertyPage {
public:
    ClassicHealthPage(HealthSheet& sheet, UINT dialog, UINT title_string, UINT poll_ms);

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    BOOL OnKillActive() override;
    virtual void init() = 0;
    virtual void poll() = 0;
    virtual void tick() {}  // the animation timer, if the page has one
    void replace_picture(UINT control, c1kitshell::PaintedView& view,
                         c1kitshell::PaintedView::Painter painter);
    void start_animation(UINT ms);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

    HealthSheet& sheet_;
    CString title_;
    UINT poll_ms_;
    UINT animation_ms_ = 0;
};

// Fitness page (dialog 138): Skeleton.bmp with the thermometer, the heart
// monitor, the beating heart and the blinking eye.
class ClassicFitnessPage : public ClassicHealthPage {
public:
    explicit ClassicFitnessPage(HealthSheet& sheet);

protected:
    void init() override;
    void poll() override;
    void tick() override;

private:
    void draw(CDC& dc, const CRect& rect);
    void fade(int scan_x);
    void flat();
    void line_to(int target_y, bool up);
    void thermometer();

    c1kitshell::PaintedView picture_;
    c1kitshell::Canvas canvas_;
    int width_ = 0, height_ = 0;
    std::vector<std::uint8_t> skeleton_, frame_;
    int therm_width_ = 0, therm_height_ = 0;
    std::vector<std::uint8_t> therm_;
    c1kitshell::KitSprite hearts_, blink_;
    bool loaded_ = false, have_data_ = false;
    int x_ = 0, y_ = 0, state_ = 0, pending_ = 0, gap_ = 0;
    int heart_tick_ = 0, heart_frame_ = 0, blink_counter_ = 0, eye_frame_ = 0;
    int mercury_x_ = 0;
};

// Drives and needs (dialog 139): five Gauge.spr gauges and their names.
class ClassicDrivesPage : public ClassicHealthPage {
public:
    explicit ClassicDrivesPage(HealthSheet& sheet);

protected:
    void init() override;
    void poll() override;

private:
    void draw_gauge(int index, CDC& dc, const CRect& rect);

    c1kitshell::PaintedView gauges_[5];
    c1kitshell::Canvas canvas_;
    c1kitshell::KitSprite gauge_;
    CFont label_font_;
    int levels_[5] = {};
};

// Brain activity (dialog 134): Lobes.bmp, each lobe lit as it is busy.
class ClassicBrainPage : public ClassicHealthPage {
public:
    explicit ClassicBrainPage(HealthSheet& sheet);

protected:
    void init() override;
    void poll() override;

private:
    void draw(CDC& dc, const CRect& rect);

    c1kitshell::PaintedView picture_;
    c1kitshell::Canvas canvas_;
    std::vector<c1kitshell::KitSprite> lobe_sprites_;
    int frames_[6] = {};
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

struct Subject {
    bool present = false;
    std::string moniker;
    std::string name;
};

class HealthSheet : public c1kitshell::KitSheet, private c1kitshell::ShopHost {
public:
    explicit HealthSheet(CFont& default_font);
    ~HealthSheet() override;

    bool create_window();
    bool classic() const { return classic_ != nullptr; }
    const c1kitshell::GamePalette& palette() const { return palette_; }

    const Subject& subject() const { return subject_; }
    bool query(const std::string& script, std::string& reply);
    bool brain_report(int mode, std::string& reply);
    const std::vector<std::string>& chemical_names() const { return chemical_names_; }
    const std::vector<c1kit::LobeLayout>& lobes() const { return lobes_; }
    // c1kitshell::ShopHost: the Doctor's page's stock, "Health".
    std::vector<c1kit::ShopItem>& shop_items() override { return shop_; }
    bool run_shop_command(const std::string& script) override;
    bool save_shop() override;
    const c1kitshell::GamePalette& shop_palette() const override { return palette_; }
    const char* shop_file_name() const override { return c1kit::kHealthShopFileName; }
    c1kit::KitSettings* settings() { return registry_; }

protected:
    BOOL OnInitDialog() override;
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
    void load_data_files();
    void connect();
    void take_subject();
    void update_title();
    bool paused() const { return game_paused_ || minimised_; }
    std::string game_file(const std::string& name) const;

    CFont& default_font_;
    std::unique_ptr<c1kitshell::ClassicArt> classic_;
    std::unique_ptr<c1kitshell::CoverPage> cover_;
    std::unique_ptr<ClassicFitnessPage> classic_fitness_;
    std::unique_ptr<ClassicDrivesPage> classic_drives_;
    std::unique_ptr<ClassicBrainPage> classic_brain_;
    std::unique_ptr<c1kitshell::ClassicShopPage> classic_doctor_;
    FitnessPage fitness_page_;
    DrivesPage drives_page_;
    BrainPage brain_page_;
    c1kitshell::ShopPage doctor_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::unique_ptr<c1kit::MacroConversation> report_conversation_;
    Subject subject_;
    std::vector<std::string> chemical_names_;
    std::vector<c1kit::LobeLayout> lobes_;
    std::vector<c1kit::ShopItem> shop_;
    c1kitshell::GamePalette palette_;
    int saved_page_ = 0;
    bool connected_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace health
