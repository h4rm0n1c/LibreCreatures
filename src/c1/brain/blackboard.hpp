#pragma once

#include "../objects/compound_object.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace creatures1::brain {

// The editor's character policy is part of the game runtime, not a Windows
// text-control detail.  Keep the recovered bit values at this boundary so a
// platform front end can implement the same policy without importing the
// native global input buffer.
enum class TextInputCharacter : std::uint32_t {
    letters = 0x01,
    space = 0x02,
    question_mark = 0x04,
    exclamation_mark = 0x08,
    // SFCDoc::UpdateWorld @ 0x00432850: 0x10 admits "?!.,:/\\£$%&*" and 0x20
    // admits "0123456789" -- the normal pointer policy (0x1f) has no digits.
    punctuation = 0x10,
    digits = 0x20,
};

constexpr std::uint32_t kNormalTextInputFlags =
    static_cast<std::uint32_t>(TextInputCharacter::letters) |
    static_cast<std::uint32_t>(TextInputCharacter::space) |
    static_cast<std::uint32_t>(TextInputCharacter::question_mark) |
    static_cast<std::uint32_t>(TextInputCharacter::exclamation_mark) |
    static_cast<std::uint32_t>(TextInputCharacter::punctuation);

// The application owns the shared editor state and the pointer-tool object.
// These operations are the semantic equivalent of the native global input
// buffer writes; no MFC globals or vtable slots leak into Blackboard.
class BlackboardTextInputHost {
public:
    virtual ~BlackboardTextInputHost() = default;
    virtual objects::Object* pointer_tool() const = 0;
    virtual void clear_text_input() = 0;
    virtual void configure_text_input(objects::Object* target,
                                      std::uint32_t maximum_length,
                                      std::uint32_t allowed_characters) = 0;
    virtual void update_text_input(objects::Object& target,
                                   std::string_view text) = 0;
};

// Drawing and dirty-rectangle ownership stays with the display/world
// subsystems. Blackboard supplies only its label geometry and text state.
class BlackboardDisplayHost {
public:
    virtual ~BlackboardDisplayHost() = default;
    virtual objects::EntityRasterHost& raster_host() = 0;
    virtual objects::CompoundObjectMoveRedrawHost& redraw_host() = 0;
};

class BlackboardRuntimeHost : public objects::ObjectSoundViewportHost,
                              public BlackboardTextInputHost {
public:
    ~BlackboardRuntimeHost() override = default;
};

struct BlackboardWord {
    // The native word record is an eleven-byte, NUL-terminated slot: ten
    // source characters plus the terminator.
    std::array<char, 11> text{};
};

struct BlackboardDisplayConfig {
    std::uint8_t fill_palette_index = 0;
    std::uint8_t text_render_config_1 = 0;
    std::uint8_t text_render_config_2 = 0;
    std::uint8_t tile_x = 0;
    std::uint8_t tile_y = 0;
};

class Blackboard final : public objects::CompoundObject {
public:
    static constexpr std::size_t kWordCount = 16;
    static constexpr std::size_t kWordCapacity = 11;
    static constexpr std::size_t kDisplayTextCapacity = 14;

    explicit Blackboard(
        objects::CompoundObjectLifetimeHost* lifetime_host = nullptr);

    // `new: bbrd`, from Macro::ExecuteNewCommand @ 0x0041d130.  The compound
    // base is built cache-protected, then initialize_state runs and the five
    // display bytes are written straight from the command operands.
    Blackboard(std::uint32_t object_type, int header_record_index,
               std::uint32_t image_count, std::uint8_t fill_palette_index,
               std::uint8_t text_render_config_1,
               std::uint8_t text_render_config_2, std::uint8_t tile_x,
               std::uint8_t tile_y,
               objects::CompoundObjectConstructionHost& construction);

    static std::unique_ptr<Blackboard> create(
        objects::CompoundObjectLifetimeHost* lifetime_host = nullptr);

    void serialize(objects::ObjectArchive& archive);
    void initialize_state();
    void redraw_display(bool draw_word_text, BlackboardDisplayHost& display);
    void set_edit_mode(bool enable_edit_mode, BlackboardRuntimeHost& runtime,
                       BlackboardDisplayHost& display);
    void finalize_text_input(std::string_view text,
                             BlackboardRuntimeHost& runtime,
                             BlackboardDisplayHost& display);
    void set_current_word_text(std::string_view text,
                               BlackboardDisplayHost& display);
    // Blackboard::Tick @ 0x0042cba0 -- vtable slot 42 (the world update's
    // drive-threshold phase), not the per-object slot-41 tick, which the
    // board inherits unchanged from CompoundObject.
    void tick(BlackboardRuntimeHost& runtime, BlackboardDisplayHost& display);

    const BlackboardWord& word(std::size_t index) const {
        return word_text_bank_[index];
    }
    BlackboardWord& word(std::size_t index) { return word_text_bank_[index]; }
    std::uint32_t word_value(std::size_t index) const {
        return word_value_bank_[index];
    }
    void set_word_value(std::size_t index, std::uint32_t value) {
        word_value_bank_[index] = value;
    }
    const BlackboardDisplayConfig& display_config() const {
        return display_config_;
    }
    BlackboardDisplayConfig& display_config() { return display_config_; }
    bool edit_mode_enabled() const { return edit_mode_enabled_; }

private:
    static std::size_t selected_word_index(const Blackboard& blackboard);
    void clear_current_word();
    void restore_normal_text_input(BlackboardRuntimeHost& runtime);

    std::array<BlackboardWord, kWordCount> word_text_bank_{};
    std::array<std::uint32_t, kWordCount> word_value_bank_{};
    BlackboardDisplayConfig display_config_{};
    bool edit_mode_enabled_ = false;
};

} // namespace creatures1::brain
