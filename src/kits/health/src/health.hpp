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
// marked "Fix (bug N)".

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
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

// Doctor's page: the shop ("Health"), one item at a time.
class DoctorPage : public HealthPage {
public:
    explicit DoctorPage(HealthSheet& sheet);
    void subject_changed() override;

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnSelectionChanged();
    afx_msg void OnAddToWorld();
    DECLARE_MESSAGE_MAP()

private:
    void fill_list();
    void show_selected();
    void draw_picture(CDC& dc, const CRect& rect);

    CListBox items_;
    c1kitshell::PaintedView picture_;
    CButton add_;
    CStatic status_;
    c1kitshell::Canvas canvas_;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

struct Subject {
    bool present = false;
    std::string moniker;
    std::string name;
};

class HealthSheet : public c1kitshell::KitSheet {
public:
    explicit HealthSheet(CFont& default_font);
    ~HealthSheet() override;

    bool create_window();

    const Subject& subject() const { return subject_; }
    bool query(const std::string& script, std::string& reply);
    bool brain_report(int mode, std::string& reply);
    const std::vector<std::string>& chemical_names() const { return chemical_names_; }
    const std::vector<c1kit::LobeLayout>& lobes() const { return lobes_; }
    const c1kitshell::GamePalette& palette() const { return palette_; }
    std::vector<c1kit::ShopItem>& shop() { return shop_; }
    bool save_shop();
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
    bool paused() const { return game_paused_ || minimised_; }
    void set_always_on_top(bool on);
    std::string game_file(const std::string& name) const;

    CFont& default_font_;
    FitnessPage fitness_page_;
    DrivesPage drives_page_;
    BrainPage brain_page_;
    DoctorPage doctor_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::unique_ptr<c1kit::MacroConversation> report_conversation_;
    Subject subject_;
    std::vector<std::string> chemical_names_;
    std::vector<c1kit::LobeLayout> lobes_;
    std::vector<c1kit::ShopItem> shop_;
    c1kitshell::GamePalette palette_;
    std::uint32_t always_on_top_ = 0;
    int saved_page_ = 0;
    bool connected_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace health
