#include "bubble.hpp"

#include <algorithm>

namespace creatures1::objects {

Bubble::Bubble(Object* anchor_object, std::uint8_t lifetime_ticks,
               std::string_view text, BubblePlacementMode placement_mode,
               bool place_on_right, BubbleConstructionHost& construction)
    : SimpleObject(
          0x74737973,
          9 + (placement_mode == BubblePlacementMode::viewport_centered
                   ? static_cast<int>(place_on_right)
                   : static_cast<int>(place_on_right) +
                         static_cast<int>(placement_mode) * 2),
          1, true, -99, -99, 9000, 0x10, 0, 2, 1, 2, 0xff, 0, 0, 0,
          construction),
      lifetime_ticks_remaining(lifetime_ticks),
      anchor_object(anchor_object),
      place_on_right(place_on_right),
      placement_mode(placement_mode) {
    update_sound(construction);

    int target_x = 0;
    int target_y = 0;
    bool have_target = true;
    if (placement_mode == BubblePlacementMode::viewport_centered) {
        const world::ViewportBounds viewport = construction.sound_viewport();
        target_x = (viewport.left + viewport.right) / 2;
        target_y = (viewport.top + viewport.bottom) / 2;
    } else if (anchor_object == nullptr) {
        have_target = false;
    } else {
        target_x = anchor_object->sound_source_x();
        target_x += place_on_right ? anchor_object->current_visual_width()
                                   : -0xa0;
        target_y = anchor_object->sound_source_y() - 0x19;
    }

    if (have_target &&
        (sound_source_x() != target_x || sound_source_y() != target_y)) {
        move_to_and_redraw(target_x, target_y, construction);
    }

    if (lifetime_ticks_remaining != 0 &&
        --lifetime_ticks_remaining == 0) {
        destroy_and_redraw(construction);
    }

    set_text(text, construction);
}

void Bubble::destroy_and_redraw(BubbleRedrawHost& host) {
    world::WorldRect bubble_bounds{};
    get_bounds(&bubble_bounds);

    // The native routine invokes the object's deleting hook before asking
    // the world renderer to expose the now-vacated image rectangle.
    host.invoke_bubble_deleting(*this, 1);
    host.redraw_after_bubble_deleting(bubble_bounds);
}

void Bubble::set_text(std::string_view text, BubbleTextHost& host) {
    // The native buffer is 25 bytes: at most 24 source bytes followed by a
    // terminator. Stop at an embedded NUL just as the original char* API did.
    const std::size_t copy_count = [&] {
        std::size_t count = 0;
        while (count < 24 && count < text.size() && text[count] != '\0') {
            ++count;
        }
        return count;
    }();
    std::copy_n(text.begin(), copy_count, text_buffer.begin());
    text_buffer[copy_count] = '\0';
    std::fill(text_buffer.begin() + copy_count + 1, text_buffer.end(), '\0');

    const std::string_view stored_text(text_buffer.data(), copy_count);
    const std::uint16_t text_width =
        host.measure_bubble_text_width(stored_text);
    host.clear_bubble_text_band(*this);
    host.draw_bubble_text(*this, (0x90 - text_width) / 2 + 6, 3,
                          stored_text, 0xf2, 0xb, 0xc2);

    world::WorldRect bubble_bounds{};
    get_bounds(&bubble_bounds);
    host.redraw_after_bubble_text(bubble_bounds);
}

void Bubble::serialize(ObjectArchive& archive) {
    // SimpleObject contributes the inherited Object and Entity archive record.
    // Bubble's
    // native extension is deliberately kept in this order: lifetime, the
    // polymorphic anchor reference, then all 25 bytes of the text field.
    SimpleObject::serialize(archive);

    if (archive.is_loading()) {
        lifetime_ticks_remaining = archive.read_byte();
        anchor_object = static_cast<Object*>(
            archive.read_object_reference("Object"));
        for (std::size_t index = 0; index < text_buffer.size(); ++index) {
            text_buffer[index] = static_cast<char>(archive.read_byte());
        }
        return;
    }

    archive.write_byte(lifetime_ticks_remaining);
    archive.write_object_reference(anchor_object, "Object");
    for (const char value : text_buffer) {
        archive.write_byte(static_cast<std::uint8_t>(value));
    }
}

bool Bubble::references_object(Object* candidate) const {
    return anchor_object == candidate;
}

void Bubble::clear_references_to(Object* candidate) {
    if (anchor_object == candidate) {
        anchor_object = nullptr;
    }
}

void Bubble::tick(BubbleTickHost& host) {
    update_sound(host);

    int target_x = 0;
    int target_y = 0;
    bool have_target = true;
    if (placement_mode == BubblePlacementMode::viewport_centered) {
        const world::ViewportBounds viewport = host.sound_viewport();
        target_x = (viewport.left + viewport.right) / 2;
        target_y = (viewport.top + viewport.bottom) / 2;
    } else {
        if (anchor_object == nullptr) {
            have_target = false;
        } else {
            target_x = anchor_object->sound_source_x();
            if (place_on_right) {
                target_x += anchor_object->current_visual_width();
            } else {
                target_x -= 0xa0;
            }
            target_y = anchor_object->sound_source_y() - 0x19;
        }
    }

    if (have_target &&
        (sound_source_x() != target_x || sound_source_y() != target_y)) {
        host.move_bubble_and_redraw(*this, target_x, target_y);
    }

    if (lifetime_ticks_remaining != 0 &&
        --lifetime_ticks_remaining == 0) {
        host.expire_bubble(*this);
    }
}

} // namespace creatures1::objects
