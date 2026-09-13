#pragma once

#include "simple_object.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace creatures1::objects {

// Bubble @ 0x00429da0 folds the mode into the sprite base as
// `9 + place_on_right + mode * 2`, except for the centred mode which drops
// the mode term.  Native callers use all three values: Creature::Speak and
// PointerTool's transient bubble pass 0, PointerTool's persistent bubble
// passes 1, and the Macro `bbd:` presentation path passes 2.
enum class BubblePlacementMode : std::uint8_t {
    speech = 0,
    persistent = 1,
    viewport_centered = 2,
};

class Bubble;

// Bubble positioning uses Object's sound source and visual dimensions.  The
// renderer/audio owner supplies presentation and expiration side effects.
class BubbleTickHost : public virtual ObjectSoundPlaybackHost {
public:
    virtual ~BubbleTickHost() = default;
    virtual void move_bubble_and_redraw(Bubble& bubble, int world_x,
                                        int world_y) = 0;
    virtual void expire_bubble(Bubble& bubble) = 0;
};

// Destruction redraw captures the bubble bounds before invoking the native
// deleting hook.  Window/iconic state, wrapped viewport intersection, and the
// deferred dirty-rectangle queue remain renderer-owned.
class BubbleRedrawHost {
public:
    virtual ~BubbleRedrawHost() = default;
    virtual void invoke_bubble_deleting(Bubble& bubble,
                                        std::uint32_t deletion_flags) = 0;
    virtual void redraw_after_bubble_deleting(
        const world::WorldRect& bubble_bounds) = 0;
};

// SetText owns the bounded text buffer and presentation order. Font metrics,
// pixel writes, and the window/dirty-rectangle redraw remain host-owned.
class BubbleTextHost {
public:
    virtual ~BubbleTextHost() = default;
    virtual std::uint16_t measure_bubble_text_width(
        std::string_view text) const = 0;
    virtual void clear_bubble_text_band(Bubble& bubble) = 0;
    virtual void draw_bubble_text(Bubble& bubble, int x, int y,
                                  std::string_view text,
                                  std::uint8_t background,
                                  std::uint8_t foreground,
                                  std::uint8_t shadow) = 0;
    virtual void redraw_after_bubble_text(
        const world::WorldRect& bubble_bounds) = 0;
};

// Bubble construction performs two presentation operations that belong to
// the world/UI owner: the native expiration redraw and the image-backed text
// update.  The recovered constructor keeps their ordering explicit while the
// owner retains window, image, and dirty-rectangle storage.
class BubbleConstructionHost : public virtual SimpleObjectConstructionHost,
                               public virtual ObjectSoundPlaybackHost,
                               public virtual SimpleObjectMoveRedrawHost,
                               public virtual BubbleRedrawHost,
                               public virtual BubbleTextHost {
public:
    ~BubbleConstructionHost() override = default;
};

class Bubble : public SimpleObject {
public:
    // The no-argument form is the framework/archive factory path, matching
    // Creature's split: MFC dynamic creation needs a default constructor, and
    // the game-creation constructor below carries the construction host.
    Bubble() = default;
    Bubble(Object* anchor_object, std::uint8_t lifetime_ticks,
           std::string_view text, BubblePlacementMode placement_mode,
           bool place_on_right, BubbleConstructionHost& construction);
    void destroy_and_redraw(BubbleRedrawHost& host);
    void set_text(std::string_view text, BubbleTextHost& host);
    void serialize(ObjectArchive& archive);
    bool references_object(Object* candidate) const override;
    void clear_references_to(Object* candidate) override;
    void tick(BubbleTickHost& host);

    std::uint8_t lifetime_ticks_remaining = 0;
    std::array<char, 25> text_buffer{};
    Object* anchor_object = nullptr;
    bool place_on_right = false;
    BubblePlacementMode placement_mode = BubblePlacementMode::speech;
};

} // namespace creatures1::objects
