#pragma once

#include "../objects/bubble.hpp"
#include "../world/geometry.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace creatures1::creatures {
class Creature;
class CreatureEventFanoutHost;
}

namespace creatures1::ui {

class PointerTool;

// Pointer-tool archive loading has two game-owned effects after the record is
// restored: an unbounded placement pass and activation of the shared text
// editor.  The renderer/input owner supplies those effects; the archive and
// object fields remain in this source-level owner.
class PointerToolArchiveHost : public objects::SimpleObjectPlacementHost {
public:
    ~PointerToolArchiveHost() override = default;

    virtual void activate_pointer_tool_text_input(
        PointerTool& tool, std::uint32_t maximum_length,
        std::uint32_t allowed_characters) = 0;
};

// PointerTool owns the recovered input, click-script, queued-stimulus, and
// bubble state machines.  World registries, the selector control, bubble
// lifetime/ownership, and pending SFCView input remain runtime boundaries.
// In particular, these names replace the native globals and MFC/USER32 calls
// without pretending those platform objects are part of the C1 source model.
class PointerToolRuntimeHost : public objects::SimpleObjectInteractionHost {
public:
    ~PointerToolRuntimeHost() override = default;

    virtual creatures1::creatures::Creature* creature_for_object(
        objects::Object& object) const = 0;
    virtual std::size_t creature_count() const = 0;
    virtual creatures1::creatures::Creature* creature_at(
        std::size_t index) const = 0;
    virtual void report_invalid_creature_index() const = 0;
    virtual void select_creature(creatures1::creatures::Creature& creature,
                                 bool return_to_creature) = 0;
    virtual bool is_creature_object(const objects::Object& object) const = 0;
    virtual void update_creature_attention(
        creatures1::creatures::Creature& creature) = 0;
    virtual void queue_pointer_tool_stimulus(
        creatures1::creatures::Creature& source, PointerTool& tool) = 0;

    virtual void set_edit_object(objects::Object* object) = 0;
    virtual std::size_t scenery_object_count() const = 0;
    virtual objects::Object* scenery_object_at(std::size_t index) const = 0;
    virtual void report_invalid_scenery_index() const = 0;
    virtual bool point_in_world_rect(const world::WorldRect& bounds,
                                     int world_x, int world_y) const = 0;

    virtual std::uint32_t pending_input_flags() const = 0;
    virtual void finish_pending_input() = 0;

    virtual void dismiss_pointer_bubble(objects::Bubble& bubble) = 0;
    virtual objects::Bubble* create_persistent_pointer_bubble(
        PointerTool& tool, std::string_view text) = 0;
    virtual objects::Bubble* create_transient_pointer_bubble(
        PointerTool& tool, std::string_view text) = 0;
    virtual void set_pointer_bubble_text(objects::Bubble& bubble,
                                          std::string_view text) = 0;
    virtual void queue_speech_range_event(
        objects::Object& source, objects::ObjectEventId event_id) = 0;
    virtual void synchronize_creature_selector(std::string_view text) = 0;
};

// The pointer tool is a SimpleObject-derived UI object.  Its archive tail is
// deliberately kept separate from the inherited SimpleObject record: two
// signed cursor offsets, one polymorphic Bubble reference, and 25 raw text
// bytes, in that order.
class PointerTool final : public objects::SimpleObject {
public:
    static constexpr std::size_t kTextCapacity = 25;
    static constexpr std::uint32_t kTextInputMaximumLength = 0x18;
    static constexpr std::uint32_t kTextInputAllowedCharacters = 0x1fu;

    // The no-argument form is the framework/archive factory path, matching
    // Creature and Bubble: MFC dynamic creation from World.sfc needs a
    // default constructor, and the game-creation constructor below carries
    // the construction host.
    PointerTool() = default;
    PointerTool(std::uint32_t sprite_file_id,
                int header_record_index,
                std::uint32_t image_count,
                bool cache_protected,
                int initial_world_x,
                int initial_world_y,
                int render_plane,
                std::uint8_t bounds_flags,
                std::uint32_t packed_classifier,
                std::uint8_t click_event_selector,
                std::uint32_t reserved_word_0,
                std::uint32_t reserved_word_1,
                std::uint8_t interaction_event_flags,
                objects::SimpleObjectConstructionHost& construction);

    void serialize(objects::ObjectArchive& archive,
                   PointerToolArchiveHost& runtime);

    void handle_queued_event_4(
        const objects::QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout,
        PointerToolRuntimeHost& runtime);
    void handle_queued_event_5(const objects::QueuedObjectEvent& event,
                               PointerToolRuntimeHost& runtime);
    void set_text_with_toolbar_update(std::string_view text,
                                      PointerToolRuntimeHost& runtime);
    void set_persistent_bubble_text(std::string_view text,
                                    PointerToolRuntimeHost& runtime);
    void set_transient_bubble_text(std::string_view text,
                                   PointerToolRuntimeHost& runtime);
    void execute_click_script_fallback(
        objects::Object* target, objects::ObjectEventId event_id,
        PointerToolRuntimeHost& runtime);
    void process_pending_input(objects::SimpleObject& receiver,
                               PointerToolRuntimeHost& runtime);

    std::int32_t cursor_hotspot_offset_x = 2;
    std::int32_t cursor_hotspot_offset_y = 2;
    objects::Bubble* persistent_bubble = nullptr;
    std::array<char, kTextCapacity> text_buffer{};
};

} // namespace creatures1::ui
