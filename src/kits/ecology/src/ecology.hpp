#pragma once

// The Ecology Kit (Tool slot 10, new in LibreCreatures): a small map of the
// whole world, its rooms coloured by a chosen layer (temperature, crowding,
// food and drink, toys, disease), every creature as a dot and every egg as a
// small egg.  A click on a creature selects it and moves the camera to it.
//
// The rooms, their temperatures and the creatures come from the
// LibreCreatures `dde: ecol` query (c1kit/ecology.hpp); the food and toys
// from plain CAOS.  Against a game without `dde: ecol` the kit shows only
// the world's picture.

#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/ecology.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ecology {

class EcologySheet;

// ---------------------------------------------------------------------------
// The map page
// ---------------------------------------------------------------------------

class MapPage : public c1kitshell::LayoutPage {
public:
    explicit MapPage(EcologySheet& sheet);
    ~MapPage() override;

    // After each poll.
    void refresh();
    c1kit::EcologyLayer layer() const { return layer_; }
    void set_layer(c1kit::EcologyLayer layer);

protected:
    void create_controls() override;
    void layout(int width, int height) override;
    afx_msg void OnLayerChanged();
    DECLARE_MESSAGE_MAP()

private:
    void load_world_picture();
    void draw(CDC& dc, const CRect& rect);
    // Where the world maps into `rect` (kept in proportion), and back.
    CRect world_rect(const CRect& rect) const;
    CPoint to_view(int world_x, int world_y) const;
    void on_click(CPoint point);
    void describe();

    EcologySheet& sheet_;
    c1kitshell::PaintedView map_;
    CStatic layer_label_;
    CComboBox layer_box_;
    CStatic legend_;
    // The world's picture (back.spr / .s32), made small once.
    std::vector<std::uint32_t> picture_;
    int picture_width_ = 0;
    int picture_height_ = 0;
    CRect view_world_;  // the last drawn world rectangle, in view pixels
    c1kit::EcologyLayer layer_ = c1kit::EcologyLayer::temperature;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

class EcologySheet : public c1kitshell::KitSheet {
public:
    explicit EcologySheet(CFont& default_font);
    ~EcologySheet() override;

    bool create_window();
    // The latest poll.
    const c1kit::EcologySnapshot& world() const { return world_; }
    const std::vector<int>& food_per_room() const { return food_; }
    const std::vector<int>& toys_per_room() const { return toys_; }
    // Each egg's bottom centre, in the world.
    const std::vector<std::pair<int, int>>& eggs() const { return eggs_; }
    // False against a game without `dde: ecol`.
    bool game_reports_ecology() const { return reports_ecology_; }
    bool connected() const { return connected_; }
    // Selects the creature and pans the view to it.
    void select_creature(std::uint32_t handle);
    // The game's Images folders, world first, for the world picture.
    std::vector<std::string> image_directories() const;
    std::string game_file(const std::string& name) const;
    void save_layer();

protected:
    BOOL OnInitDialog() override;
    void on_control_state(std::uint8_t state) override;
    void before_game_quit() override;
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
    void poll();
    bool run(const std::string& script, std::string& reply);
    void update_pause();
    bool paused() const { return game_paused_ || minimised_; }

    CFont& default_font_;
    MapPage page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    c1kit::EcologySnapshot world_;
    std::vector<int> food_;
    std::vector<int> toys_;
    std::vector<std::pair<int, int>> eggs_;
    bool reports_ecology_ = false;
    bool connected_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace ecology
