#include "blackboard.hpp"

#include <algorithm>
#include <memory>

namespace creatures1::brain {

Blackboard::Blackboard(std::uint32_t object_type, int header_record_index,
                       std::uint32_t image_count,
                       std::uint8_t fill_palette_index,
                       std::uint8_t text_render_config_1,
                       std::uint8_t text_render_config_2, std::uint8_t tile_x,
                       std::uint8_t tile_y,
                       objects::CompoundObjectConstructionHost& construction)
    : objects::CompoundObject(object_type, header_record_index, image_count,
                              /*cache_protected=*/true, construction) {
    initialize_state();
    display_config_.fill_palette_index = fill_palette_index;
    display_config_.text_render_config_1 = text_render_config_1;
    display_config_.text_render_config_2 = text_render_config_2;
    display_config_.tile_x = tile_x;
    display_config_.tile_y = tile_y;
}


namespace {

constexpr std::uint32_t kEditTextMaximumLength = 10;
constexpr std::uint32_t kNormalTextMaximumLength = 0x18;

void copy_word_text(std::array<char, Blackboard::kDisplayTextCapacity>& out,
                    const BlackboardWord& word, bool append_cursor) {
    std::size_t length = 0;
    while (length + 1 < word.text.size() && word.text[length] != '\0' &&
           length + 1 < out.size()) {
        out[length] = word.text[length];
        ++length;
    }
    if (append_cursor && length + 1 < out.size()) {
        out[length] = '_';
        ++length;
    }
    out[length] = '\0';
}

void copy_input_to_word(BlackboardWord& word, std::string_view text) {
    const std::size_t length =
        std::min<std::size_t>(text.size(), word.text.size() - 1);
    for (std::size_t index = 0; index < length; ++index) {
        word.text[index] = text[index];
    }
    word.text[length] = '\0';
}

} // namespace

Blackboard::Blackboard(objects::CompoundObjectLifetimeHost* lifetime_host)
    : objects::CompoundObject(lifetime_host) {
    initialize_state();
}

std::unique_ptr<Blackboard> Blackboard::create(
    objects::CompoundObjectLifetimeHost* lifetime_host) {
    return std::make_unique<Blackboard>(lifetime_host);
}

void Blackboard::serialize(objects::ObjectArchive& archive) {
    CompoundObject::serialize(archive);

    if (archive.is_loading()) {
        display_config_.fill_palette_index = archive.read_byte();
        display_config_.text_render_config_1 = archive.read_byte();
        display_config_.text_render_config_2 = archive.read_byte();
        display_config_.tile_x = archive.read_byte();
        display_config_.tile_y = archive.read_byte();
        for (std::size_t index = 0; index < kWordCount; ++index) {
            word_value_bank_[index] = archive.read_uint32();
            archive.read_bytes(word_text_bank_[index].text.data(),
                               word_text_bank_[index].text.size());
        }
        return;
    }

    archive.write_byte(display_config_.fill_palette_index);
    archive.write_byte(display_config_.text_render_config_1);
    archive.write_byte(display_config_.text_render_config_2);
    archive.write_byte(display_config_.tile_x);
    archive.write_byte(display_config_.tile_y);
    for (std::size_t index = 0; index < kWordCount; ++index) {
        archive.write_uint32(word_value_bank_[index]);
        archive.write_bytes(word_text_bank_[index].text.data(),
                            word_text_bank_[index].text.size());
    }
}

void Blackboard::initialize_state() {
    for (BlackboardWord& word : word_text_bank_) {
        word.text[0] = '\0';
    }
    for (std::uint32_t& value : word_value_bank_) {
        value = 0;
    }
    display_config_ = {};
    edit_mode_enabled_ = false;
}

std::size_t Blackboard::selected_word_index(const Blackboard& blackboard) {
    // The native code indexes directly with CAOS variable zero.  A malformed
    // runtime value is not a valid word selection; clamping keeps the clean
    // object boundary safe while preserving every valid native selection.
    return std::min<std::size_t>(blackboard.object_variable_0(),
                                 kWordCount - 1);
}

void Blackboard::clear_current_word() {
    word_text_bank_[selected_word_index(*this)].text[0] = '\0';
}

void Blackboard::restore_normal_text_input(BlackboardRuntimeHost& runtime) {
    runtime.clear_text_input();
    runtime.configure_text_input(runtime.pointer_tool(),
                                 kNormalTextMaximumLength,
                                 kNormalTextInputFlags);
}

void Blackboard::redraw_display(bool draw_word_text,
                                 BlackboardDisplayHost& display) {
    objects::CompoundPart& primary_part = part(0);
    if (primary_part.entity == nullptr) {
        return;
    }

    objects::Entity& entity = *primary_part.entity;
    const int tile_x = display_config_.tile_x;
    const int tile_y = display_config_.tile_y;
    entity.fill_current_image_rect(display_config_.fill_palette_index, tile_x,
                                   tile_y, tile_x + 0x3c, tile_y + 0x0c,
                                   display.raster_host());
    if (draw_word_text) {
        std::array<char, kDisplayTextCapacity> text{};
        copy_word_text(text, word_text_bank_[selected_word_index(*this)],
                       edit_mode_enabled_);
        entity.draw_text_to_current_image(
            tile_x, tile_y, text.data(), display_config_.fill_palette_index,
            display_config_.text_render_config_1,
            display_config_.text_render_config_2, display.raster_host());
    }
    queue_primary_part_dirty_rect(display.redraw_host());
}

void Blackboard::set_edit_mode(bool enable_edit_mode,
                                BlackboardRuntimeHost& runtime,
                                BlackboardDisplayHost& display) {
    runtime.clear_text_input();
    if (!enable_edit_mode) {
        runtime.configure_text_input(runtime.pointer_tool(),
                                     kNormalTextMaximumLength,
                                     kNormalTextInputFlags);
        edit_mode_enabled_ = false;
    } else {
        edit_mode_enabled_ = true;
        runtime.configure_text_input(this, kEditTextMaximumLength,
                                     static_cast<std::uint32_t>(
                                         TextInputCharacter::letters));
        clear_current_word();
    }
    redraw_display(true, display);
}

void Blackboard::finalize_text_input(std::string_view text,
                                      BlackboardRuntimeHost& runtime,
                                      BlackboardDisplayHost& display) {
    runtime.update_text_input(*this, text);
    restore_normal_text_input(runtime);
    edit_mode_enabled_ = false;
    redraw_display(true, display);
    // The duplicate refresh is present in the native body and is retained as
    // an observable side effect for renderers that coalesce dirty rectangles.
    redraw_display(true, display);
}

void Blackboard::set_current_word_text(std::string_view text,
                                       BlackboardDisplayHost& display) {
    copy_input_to_word(word_text_bank_[selected_word_index(*this)], text);
    redraw_display(true, display);
}

void Blackboard::tick(BlackboardRuntimeHost& runtime,
                      BlackboardDisplayHost& display) {
    if (!edit_mode_enabled_ ||
        sound_audibility_state(runtime) !=
            objects::SoundAudibilityState::out_of_range) {
        return;
    }
    restore_normal_text_input(runtime);
    edit_mode_enabled_ = false;
    redraw_display(true, display);
}

} // namespace creatures1::brain
