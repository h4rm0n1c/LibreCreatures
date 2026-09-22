#include "tools.hpp"

#include "../brain/brain.hpp"
#include "../creatures/attention.hpp"
#include "../creatures/creature.hpp"
#include "../creatures/events.hpp"
#include "../objects/events.hpp"
#include "views.hpp"

#include <algorithm>
#include <string_view>

namespace creatures1::ui {

namespace {

std::string_view bounded_pointer_text(const std::array<char,
                                                       PointerTool::kTextCapacity>&
                                          text_buffer) {
    std::size_t length = 0;
    while (length < text_buffer.size() && text_buffer[length] != '\0') {
        ++length;
    }
    return {text_buffer.data(), length};
}

void copy_pointer_text(std::array<char, PointerTool::kTextCapacity>& output,
                       std::string_view input) {
    const std::size_t count = std::min(output.size() - 1, input.size());
    const auto nul = std::find(input.begin(), input.begin() + count, '\0');
    const std::size_t actual_count =
        static_cast<std::size_t>(nul - input.begin());
    std::copy_n(input.begin(), actual_count, output.begin());
    output[actual_count] = '\0';
    std::fill(output.begin() + actual_count + 1, output.end(), '\0');
}

creatures1::creatures::Creature* find_creature_for_object(
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    objects::Object& object) {
    for (std::size_t index = 0; index < fanout.creature_count(); ++index) {
        creatures1::creatures::Creature* creature = fanout.creature_at(index);
        if (creature == nullptr) {
            fanout.report_invalid_creature_index();
            continue;
        }
        if (fanout.is_same_object(*creature, object)) {
            return creature;
        }
    }
    return nullptr;
}

std::uint32_t click_script_event(objects::ObjectEventId event_id) {
    switch (event_id) {
    case objects::ObjectEventId::event_0: return 0x32;
    case objects::ObjectEventId::event_1: return 0x33;
    case objects::ObjectEventId::event_2: return 0x34;
    case objects::ObjectEventId::event_4: return 0x36;
    case objects::ObjectEventId::event_5: return 0x35;
    default: return 0;
    }
}

creatures1::creatures::AttentionClassifier attention_classifier(
    const objects::Object& object) {
    const std::uint32_t classifier = object.classifier_base();
    return {
        static_cast<creatures1::creatures::AttentionObjectFamily>(
            (classifier >> 24) & 0xffu),
        static_cast<std::uint8_t>((classifier >> 16) & 0xffu)};
}

} // namespace

PointerTool::PointerTool(
    std::uint32_t sprite_file_id, int header_record_index,
    std::uint32_t image_count, bool cache_protected, int initial_world_x,
    int initial_world_y, int render_plane, std::uint8_t bounds_flags,
    std::uint32_t packed_classifier, std::uint8_t click_event_selector,
    std::uint32_t reserved_word_0, std::uint32_t reserved_word_1,
    std::uint8_t interaction_event_flags,
    objects::SimpleObjectConstructionHost& construction)
    : SimpleObject(
          sprite_file_id, header_record_index, image_count, cache_protected,
          initial_world_x, initial_world_y, render_plane, bounds_flags,
          static_cast<std::uint8_t>(packed_classifier),
          static_cast<std::uint8_t>(packed_classifier >> 8),
          static_cast<std::uint8_t>(packed_classifier >> 16),
          static_cast<std::uint8_t>(packed_classifier >> 24),
          click_event_selector, reserved_word_0, reserved_word_1,
          interaction_event_flags, construction) {}

void PointerTool::serialize(objects::ObjectArchive& archive,
                            PointerToolArchiveHost& runtime) {
    objects::SimpleObject::serialize(archive);

    if (archive.is_loading()) {
        cursor_hotspot_offset_x = archive.read_int32();
        cursor_hotspot_offset_y = archive.read_int32();
        persistent_bubble = static_cast<objects::Bubble*>(
            archive.read_object_reference("Bubble"));
        archive.read_bytes(text_buffer.data(), text_buffer.size());

        // Native PointerTool::Serialize performs this after its complete
        // custom tail, not inside SimpleObject::Serialize.
        if (uses_unbounded_world_position()) {
            initialize_unbounded_object_placement(runtime);
        }
        runtime.activate_pointer_tool_text_input(
            *this, kTextInputMaximumLength, kTextInputAllowedCharacters);
        return;
    }

    archive.write_int32(cursor_hotspot_offset_x);
    archive.write_int32(cursor_hotspot_offset_y);
    archive.write_object_reference(persistent_bubble, "Bubble");
    archive.write_bytes(text_buffer.data(), text_buffer.size());
}

void PointerTool::handle_queued_event_4(
    const objects::QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    PointerToolRuntimeHost& runtime) {
    objects::Object* source_object = event.source;
    if (source_object == nullptr) {
        return;
    }

    if (!runtime.is_creature_object(*source_object)) {
        set_bounds_mode(static_cast<std::uint32_t>(
                            objects::Object::BoundsMode::unbounded_1),
                        runtime);
        set_bounds_reference_object(nullptr);
        if (entity() != nullptr) {
            entity()->set_render_plane(9999);
            const int mouse_x = runtime.pointer_world_x();
            const int wrapped_x = mouse_x >= 0x20a0 ? mouse_x - 0x20a0
                                                    : mouse_x;
            move_to_and_redraw(wrapped_x, runtime.pointer_world_y(), runtime);
        }

        if (runtime.execute_script_for_classifier(
                *this, this, source_object->classifier_base() + 0x36,
                false) == 0) {
            runtime.execute_script_for_classifier(
                *this, this, classifier_base() + 0x36, false);
        }
        set_bounds_flags(static_cast<BoundsFlags>(bounds_flags() & ~0x10u));
        return;
    }

    creatures1::creatures::Creature* source_creature =
        find_creature_for_object(fanout, *source_object);
    if (source_creature != nullptr) {
        runtime.queue_pointer_tool_stimulus(*source_creature, *this);
    }
}

void PointerTool::handle_queued_event_5(
    const objects::QueuedObjectEvent& event, PointerToolRuntimeHost& runtime) {
    set_bounds_mode(static_cast<std::uint32_t>(
                        objects::Object::BoundsMode::default_world),
                    runtime);

    objects::Object* source_object = event.source;
    if (source_object != nullptr &&
        runtime.execute_script_for_classifier(
            *this, this, source_object->classifier_base() + 0x35, false) ==
            0) {
        runtime.execute_script_for_classifier(
            *this, this, classifier_base() + 0x35, false);
    }
    set_bounds_flags(static_cast<BoundsFlags>(bounds_flags() | 0x10u));

    for (std::size_t index = 0; index < runtime.creature_count(); ++index) {
        creatures1::creatures::Creature* creature =
            runtime.creature_at(index);
        if (creature == nullptr) {
            runtime.report_invalid_creature_index();
            continue;
        }

        const auto attention_index =
            creatures1::creatures::get_attention_record_index(
                attention_classifier(*this));
        auto& records = creature->attention_records();
        if (attention_index < records.size()) {
            auto& record = records[attention_index];
            record.world_x = -1;
            record.world_y = -1;
            if (record.target == this) {
                record.target = nullptr;
                if (creature->brain() != nullptr) {
                    auto& lobe_seven = creature->brain()->lobe(7);
                    if (attention_index < lobe_seven.neuron_count_value()) {
                        auto& neuron = lobe_seven.neuron(attention_index);
                        neuron.firing_strength = 0;
                        neuron.activation = 0;
                    }
                    auto& lobe_two = creature->brain()->lobe(2);
                    if (attention_index < lobe_two.neuron_count_value()) {
                        lobe_two.neuron(attention_index).activation = 0;
                    }
                }
            }
        }
        runtime.update_creature_attention(*creature);
    }
}

void PointerTool::set_text_with_toolbar_update(
    std::string_view text, PointerToolRuntimeHost& runtime) {
    copy_pointer_text(text_buffer, text);
    const std::string_view stored_text = bounded_pointer_text(text_buffer);
    if (persistent_bubble != nullptr) {
        runtime.dismiss_pointer_bubble(*persistent_bubble);
    }
    // Transient bubbles are runtime-owned and deliberately do not occupy the
    // persistent-bubble slot in the native PointerTool record.
    (void)runtime.create_transient_pointer_bubble(*this, stored_text);
    persistent_bubble = nullptr;
    runtime.queue_speech_range_event(*this, objects::ObjectEventId::event_6);
    runtime.synchronize_creature_selector(stored_text);
}

void PointerTool::set_persistent_bubble_text(
    std::string_view text, PointerToolRuntimeHost& runtime) {
    if (persistent_bubble == nullptr) {
        persistent_bubble =
            runtime.create_persistent_pointer_bubble(*this, text);
    } else {
        runtime.set_pointer_bubble_text(*persistent_bubble, text);
    }

    if (text.empty() && persistent_bubble != nullptr) {
        runtime.dismiss_pointer_bubble(*persistent_bubble);
        persistent_bubble = nullptr;
    }
}

void PointerTool::set_transient_bubble_text(
    std::string_view text, PointerToolRuntimeHost& runtime) {
    copy_pointer_text(text_buffer, text);
    const std::string_view stored_text = bounded_pointer_text(text_buffer);
    if (persistent_bubble != nullptr) {
        runtime.dismiss_pointer_bubble(*persistent_bubble);
    }
    (void)runtime.create_transient_pointer_bubble(*this, stored_text);
    persistent_bubble = nullptr;
    runtime.queue_speech_range_event(*this, objects::ObjectEventId::event_6);
}

void PointerTool::execute_click_script_fallback(
    objects::Object* target, objects::ObjectEventId event_id,
    PointerToolRuntimeHost& runtime) {
    if (target == nullptr || event_id == objects::ObjectEventId::no_event) {
        return;
    }
    const std::uint32_t script_event = click_script_event(event_id);
    if (script_event == 0) {
        return;
    }
    if (runtime.execute_script_for_classifier(
            *this, this, target->classifier_base() + script_event, false) ==
        0) {
        runtime.execute_script_for_classifier(
            *this, this, classifier_base() + script_event, false);
    }
}

void PointerTool::process_pending_input(
    objects::SimpleObject& receiver, PointerToolRuntimeHost& runtime) {
    // Native SimpleObject::UpdateUnboundedPositionAndRedraw calls the
    // PointerTool input routine through the current receiver. During a drag
    // that receiver is the held object, not the pointer tool, and native
    // queues EVENT_5 to that receiver to finish the drop. Keep the pointer
    // tool as the implementation object while preserving the native receiver
    // identity for hit testing and event sources.
    objects::SimpleObject& input_receiver = receiver;
    const std::uint32_t flags = runtime.pending_input_flags();
    constexpr std::uint32_t left = static_cast<std::uint32_t>(
        SfcViewPendingInputFlag::left_button);
    constexpr std::uint32_t right = static_cast<std::uint32_t>(
        SfcViewPendingInputFlag::right_button);
    constexpr std::uint32_t shift_left = static_cast<std::uint32_t>(
        SfcViewPendingInputFlag::left_with_shift);
    constexpr std::uint32_t shift_right = static_cast<std::uint32_t>(
        SfcViewPendingInputFlag::right_with_shift);

    if ((flags & shift_left) == 0) {
        if ((flags & (right | shift_right)) == 0) {
            if ((flags & left) != 0) {
                objects::Object* hit = nullptr;
                if (input_receiver.classifier_base() == 0x02010100u) {
                    hit = input_receiver.find_topmost_overlapping_object(
                        4, 4, runtime);
                    if (hit != nullptr) {
                        // Native dispatch uses Creature's click virtual. The
                        // port registers its owned Skeleton as the Object, so
                        // recover the Creature before choosing head/body events.
                        const auto* creature = runtime.creature_for_object(*hit);
                        const objects::ObjectEventId event_id =
                            creature != nullptr
                                ? creature->click_event_id_at_world_position(
                                      runtime.pointer_world_x(),
                                      runtime.pointer_world_y())
                                : hit->click_event_id_at_world_position(
                                runtime.pointer_world_x(),
                                runtime.pointer_world_y());
                        if (event_id != objects::ObjectEventId::no_event) {
                            runtime.queue_immediate_event(
                                input_receiver, *hit, event_id, 0);
                            execute_click_script_fallback(hit, event_id,
                                                         runtime);
                        }
                    }
                } else {
                    hit = input_receiver.find_topmost_overlapping_object(
                        0, 0, runtime);
                    if (hit != nullptr) {
                        runtime.queue_immediate_event(
                            input_receiver, *hit,
                            objects::ObjectEventId::event_3, 0);
                    }
                }
            }
        } else if (&input_receiver == runtime.pointer_tool()) {
            // ProcessPendingInput @0x00428a7c pushes 2, 2: only objects the
            // hand may pick up compete.  A carrier that cannot be picked up
            // (the incubator, 3.1.3, is attr 0x58) must not shadow what is
            // inside it.
            objects::Object* selected =
                input_receiver.find_topmost_overlapping_object(
                    objects::Object::kAllowPointerToolUnboundedPlacement,
                    objects::Object::kAllowPointerToolUnboundedPlacement,
                    runtime);
            if (selected != nullptr) {
                if (runtime.is_creature_object(*selected)) {
                    if (auto* creature = runtime.creature_for_object(*selected);
                        creature != nullptr) {
                        runtime.select_creature(*creature, true);
                    }
                }
                runtime.queue_immediate_event(
                    input_receiver, *selected,
                    objects::ObjectEventId::event_4, 0);
            }
        } else {
            runtime.queue_immediate_event(
                input_receiver, input_receiver,
                objects::ObjectEventId::event_5, 0);
        }
    } else if (&input_receiver == runtime.pointer_tool()) {
        auto* pointer = dynamic_cast<PointerTool*>(&input_receiver);
        if (pointer == nullptr) {
            runtime.finish_pending_input();
            return;
        }
        // Use the same one-pixel pointer hit test as ordinary input.  A
        // registry-order scan selects a containing machine before an object
        // placed inside it (for example cheese in an incubator), even when
        // the contained object is the frontmost visible target.
        objects::Object* edit = input_receiver.find_topmost_overlapping_object(
            0, 0, runtime);
        if (edit == nullptr) {
            const int world_x = entity() != nullptr
                                    ? pointer->entity()->world_x() +
                                          pointer->cursor_hotspot_offset_x
                                    : runtime.pointer_world_x();
            const int world_y = entity() != nullptr
                                    ? pointer->entity()->world_y() +
                                          pointer->cursor_hotspot_offset_y
                                    : runtime.pointer_world_y();
            for (std::size_t index = 0;
                 index < runtime.scenery_object_count(); ++index) {
                objects::Object* candidate = runtime.scenery_object_at(index);
                if (candidate == nullptr) {
                    runtime.report_invalid_scenery_index();
                    continue;
                }
                world::WorldRect bounds{};
                if (candidate->get_bounds(&bounds) &&
                    runtime.point_in_world_rect(bounds, world_x, world_y)) {
                    edit = candidate;
                    break;
                }
            }
        }
        runtime.set_edit_object(edit);
        if (edit != nullptr) {
            edit->update_movement_bounds(runtime);
        }
    }

    runtime.finish_pending_input();
}

} // namespace creatures1::ui
