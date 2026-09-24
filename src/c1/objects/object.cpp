#include "object.hpp"

#include "../objects/events.hpp"
#include "../creatures/events.hpp"
#include "../scripting/classifier_scripts.hpp"
#include "../scripting/macro.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace creatures1::objects {

std::unique_ptr<Object> Object::create_object(ObjectRegistryHost* registry) {
    return std::make_unique<Object>(registry);
}

Object::Object(ObjectRegistryHost* registry, ObjectCleanupHost* cleanup)
    : classifier_{0, 0, 0, 0},
      bounds_mode_(BoundsMode::default_world),
      bounds_flags_(0),
      movement_bounds_{0, 0, 0x20a0, 0x4b0},
      bounds_reference_object_(nullptr),
      current_interaction_event_id_(0),
      caos_object_variable_0_(0),
      caos_object_variable_1_(0),
      caos_object_variable_2_(0),
      timer_period_(0),
      timer_countdown_(0),
      tick_enabled_(true),
      sound_handle_(-1),
      continuous_sound_descriptor_(0),
      continuous_sound_persist_flag_(false),
      caos_object_pointer_(nullptr),
      gallery_(nullptr),
      registry_(registry),
      cleanup_(cleanup) {
    if (registry != nullptr) {
        registry->add_object(this);
    }
}

Object::~Object() {
    // C++ emits the deleting-destructor wrapper around this source-level
    // destructor.  The wrapper's storage-release flag is ABI machinery, not
    // a second game operation.
    if (cleanup_ != nullptr) {
        cleanup_->remove_from_renderable_set(*this);
        if (sound_handle_ >= 0) {
            cleanup_->stop_continuous_sound(sound_handle_);
            clear_continuous_sound_state();
        }
        if (gallery_ != nullptr) {
            display::Gallery* owned_gallery = gallery_;
            cleanup_->release_gallery(*owned_gallery);
            gallery_ = nullptr;
        }
        cleanup_->unregister_from_object_registry(*this);
    } else if (registry_ != nullptr) {
        unregister_from_non_scenery_object_registry(*registry_);
    }
}

void Object::serialize(ObjectArchive& archive) {
    if (archive.is_loading()) {
        const std::uint32_t packed_classifier = archive.read_uint32();
        classifier_.event = static_cast<std::uint8_t>(packed_classifier);
        classifier_.species = static_cast<std::uint8_t>(packed_classifier >> 8);
        classifier_.genus = static_cast<std::uint8_t>(packed_classifier >> 16);
        classifier_.family = static_cast<std::uint8_t>(packed_classifier >> 24);
        bounds_mode_ = static_cast<BoundsMode>(archive.read_byte());
        bounds_flags_ = archive.read_byte();
        archive.read_bytes(&movement_bounds_, sizeof(movement_bounds_));
        bounds_reference_object_ = static_cast<Object*>(
            archive.read_object_reference("Object"));
        current_interaction_event_id_ = archive.read_byte();
        gallery_ = static_cast<display::Gallery*>(
            archive.read_object_reference("CGallery"));
        timer_period_ = archive.read_int32();
        timer_countdown_ = archive.read_int32();
        caos_object_pointer_ = static_cast<Object*>(
            archive.read_object_reference("Object"));
        continuous_sound_descriptor_ = archive.read_uint32();
        sound_handle_ = -1;
        continuous_sound_persist_flag_ =
            continuous_sound_descriptor_ != 0;
        caos_object_variable_0_ = archive.read_uint32();
        caos_object_variable_1_ = archive.read_uint32();
        caos_object_variable_2_ = archive.read_uint32();

        scripting::ScriptArchiveReader* script_reader = archive.script_reader();
        scripting::ScriptDefinitionInstallHost* script_install =
            archive.script_install_host();
        if (script_reader != nullptr && script_install != nullptr) {
            scripting::deserialize_scripts_for_classifier(*script_reader,
                                                          *script_install);
            return;
        }
        const std::int32_t script_count = archive.read_script_count();
        for (std::int32_t index = 0; index < script_count; ++index) {
            (void)archive.read_script_classifier();
            (void)archive.read_script_text();
        }
        return;
    }

    const std::uint32_t packed_classifier =
        static_cast<std::uint32_t>(classifier_.event) |
        (static_cast<std::uint32_t>(classifier_.species) << 8) |
        (static_cast<std::uint32_t>(classifier_.genus) << 16) |
        (static_cast<std::uint32_t>(classifier_.family) << 24);
    archive.write_uint32(packed_classifier);
    archive.write_byte(static_cast<std::uint8_t>(bounds_mode_));
    archive.write_byte(bounds_flags_);
    archive.write_bytes(&movement_bounds_, sizeof(movement_bounds_));
    archive.write_object_reference(bounds_reference_object_, "Object");
    archive.write_byte(static_cast<std::uint8_t>(
        current_interaction_event_id_));
    archive.write_object_reference(gallery_, "CGallery");
    archive.write_int32(timer_period_);
    archive.write_int32(timer_countdown_);
    archive.write_object_reference(caos_object_pointer_, "Object");
    archive.write_uint32(continuous_sound_persist_flag_
                             ? continuous_sound_descriptor_
                             : 0);
    archive.write_uint32(caos_object_variable_0_);
    archive.write_uint32(caos_object_variable_1_);
    archive.write_uint32(caos_object_variable_2_);

    // This is the native SerializeScriptsForClassifier call. The script
    // table and its filtering policy remain in the scripting subsystem; the
    // archive adapter supplies the actual record writes.
    scripting::serialize_scripts_for_classifier(
        archive, scripting::ScriptClassifier{
                     static_cast<scripting::ScriptEvent>(classifier_.event),
                     classifier_.species, classifier_.genus,
                     classifier_.family});
}

int Object::relative_image_index(int part_index) const {
    (void)part_index;
    return 0;
}

bool Object::image_sequence_is_empty(int part_index) const {
    (void)part_index;
    return true;
}

void Object::move_by(int delta_x, int delta_y) {
    (void)delta_x;
    (void)delta_y;
}

bool Object::get_bounds(world::WorldRect* out_bounds) const {
    // The binary delegates to the platform SetRectEmpty import. The source
    // contract is an output rectangle that is always made empty.
    *out_bounds = {};
    return true;
}

void Object::get_part_center(int* out_x, int* out_y,
                            std::int32_t part_index) const {
    (void)out_x;
    (void)out_y;
    (void)part_index;
}

int Object::render_plane() const {
    return 0;
}

int Object::sound_source_x() const {
    return 0;
}

int Object::sound_source_y() const {
    return 0;
}

int Object::current_visual_width() const {
    return 0;
}

int Object::current_visual_height() const {
    return 0;
}

char* Object::parse_image_sequence(char* sequence_text,
                                   const char* sequence_end, int part_index) {
    (void)part_index;
    char* closing_bracket = sequence_text;
    while (closing_bracket < sequence_end && *closing_bracket != ']') {
        ++closing_bracket;
    }
    if (closing_bracket >= sequence_end) {
        return nullptr;
    }
    return closing_bracket + 2;
}

bool Object::set_relative_image_index(CaosValue relative_index,
                                      int part_index) {
    (void)relative_index;
    (void)part_index;
    return true;
}

char* Object::preload_image_sequence(char* sequence_text,
                                     const char* sequence_end, int part_index) {
    return parse_image_sequence(sequence_text, sequence_end, part_index);
}

ObjectEventId Object::click_event_id_at_world_position(int world_x,
                                                       int world_y) const {
    (void)world_x;
    (void)world_y;
    // The base object has no specialised click mapping.  Event zero is the
    // neutral value used by the existing object-event domain until a derived
    // object supplies its own mapping.
    return ObjectEventId::event_0;
}

bool Object::references_object(Object* candidate) const {
    return bounds_reference_object_ == candidate ||
           caos_object_pointer_ == candidate;
}

namespace {

bool encloses(const world::WorldRect& outer, const world::WorldRect& inner) {
    return outer.min_x <= inner.min_x && outer.min_y <= inner.min_y &&
           outer.max_x >= inner.max_x && outer.max_y >= inner.max_y &&
           (outer.min_x != inner.min_x || outer.min_y != inner.min_y ||
            outer.max_x != inner.max_x || outer.max_y != inner.max_y);
}

} // namespace

Object* Object::find_topmost_overlapping_object(
    BoundsFlags bounds_flag_mask, BoundsFlags bounds_flag_value,
    const ObjectOverlapHost& world) const {
    world::WorldRect search_bounds{};
    if (world.is_pointer_tool(*this)) {
        return find_object_under_pointer(bounds_flag_mask, bounds_flag_value,
                                         world);
    }
    get_bounds(&search_bounds);

    Object* topmost_object = nullptr;
    int topmost_render_plane = -1;
    for (std::size_t index = 0; index < world.object_count(); ++index) {
        Object* candidate = world.object_at(index);
        if (candidate == nullptr) {
            world.report_invalid_index();
            return topmost_object;
        }
        if (candidate == this ||
            (candidate->bounds_flags_ & bounds_flag_mask) !=
                bounds_flag_value) {
            continue;
        }

        const int candidate_render_plane = candidate->render_plane();
        if (candidate_render_plane <= topmost_render_plane) {
            continue;
        }

        world::WorldRect candidate_bounds{};
        const std::uint32_t classifier = candidate->classifier_base();
        const bool has_special_vehicle_footprint =
            (bounds_flag_value & kIsVehicle) != 0 &&
            ((classifier & 0xffff0000U) == 0x03010000U ||
             (classifier & 0xffff0000U) == 0x03020000U);
        if (has_special_vehicle_footprint) {
            candidate_bounds = world.vehicle_interaction_bounds(*candidate);
        } else {
            candidate->get_bounds(&candidate_bounds);
        }

        if (!world::wrapped_world_rects_overlap(search_bounds,
                                                candidate_bounds)) {
            continue;
        }
        topmost_render_plane = candidate_render_plane;
        topmost_object = candidate;
    }
    return topmost_object;
}

Object* Object::find_object_under_pointer(
    BoundsFlags bounds_flag_mask, BoundsFlags bounds_flag_value,
    const ObjectOverlapHost& world) const {
    // Deliberate deviation from FindTopmostOverlappingObject @0x00425c30.
    // Native hands the hand's click to the highest render plane, so a lift,
    // the incubator or the pianola swallowed clicks meant for a creature or
    // object inside or behind it.  Here any candidate whose bounds strictly
    // enclose another candidate's is dropped first -- the smaller thing
    // inside it is what the hand is aimed at -- and the native plane rule
    // decides among the rest, so partial overlaps behave as before.
    const world::WorldRect search_bounds = world.pointer_tool_bounds(*this);

    struct Hit {
        Object* object;
        world::WorldRect bounds;
        int render_plane;
    };
    std::vector<Hit> hits;
    for (std::size_t index = 0; index < world.object_count(); ++index) {
        Object* candidate = world.object_at(index);
        if (candidate == nullptr) {
            world.report_invalid_index();
            break;
        }
        if (candidate == this ||
            (candidate->bounds_flags_ & bounds_flag_mask) !=
                bounds_flag_value) {
            continue;
        }
        world::WorldRect candidate_bounds{};
        candidate->get_bounds(&candidate_bounds);
        if (!world::wrapped_world_rects_overlap(search_bounds,
                                                candidate_bounds)) {
            continue;
        }
        hits.push_back({candidate, candidate_bounds,
                        candidate->render_plane()});
    }

    Object* topmost_object = nullptr;
    int topmost_render_plane = -1;
    for (const Hit& hit : hits) {
        const bool encloses_another_hit = std::any_of(
            hits.begin(), hits.end(), [&hit](const Hit& other) {
                return &other != &hit && encloses(hit.bounds, other.bounds);
            });
        if (encloses_another_hit ||
            hit.render_plane <= topmost_render_plane) {
            continue;
        }
        topmost_render_plane = hit.render_plane;
        topmost_object = hit.object;
    }
    return topmost_object;
}

void Object::unregister_from_non_scenery_object_registry(
    ObjectRegistryHost& registry) {
    for (std::size_t index = 0; index < registry.object_count(); ++index) {
        Object* object = registry.object_at(index);
        if (object == nullptr) {
            registry.report_invalid_index();
            return;
        }
        if (object == this) {
            registry.remove_object_at(index);
            return;
        }
    }
}

void Object::dispatch_event_8(ObjectEventDispatchHost& dispatcher) {
    dispatcher.dispatch_event_8(*this);
}

void Object::queue_event_8_after_bounds_update(
    ObjectMovementBoundsHost& world_host,
    ObjectImmediateEventQueueHost& event_queue) {
    world_host.clear_edit_object();
    update_movement_bounds(world_host);
    event_queue.queue_immediate_event(*this, *this,
                                      ObjectEventId::event_8, 0);
}

void Object::dispatch_event_7_after_bounds_update(
    ObjectMovementBoundsHost& world_host,
    ObjectScriptDispatchHost& scripts) {
    update_movement_bounds(world_host);
    dispatch_script_event(ObjectEventId::event_7, this, false, scripts);
}

void Object::initialize_runtime_state(ObjectInitializationHost& runtime) {
    if (runtime.is_edit_object(*this)) {
        runtime.clear_edit_object();
    }
    runtime.remove_from_event_bar(*this, true);
    runtime.purge_destroy_when_finished_macros(*this);

    movement_bounds_ = {0, 0, 0x4140, 0x960};
    runtime.move_to_and_redraw(*this, 1000, 4000);
    tick_enabled_ = false;

    if (runtime.is_selected_creature(*this)) {
        runtime.clear_selected_creature(true);
    }
    if ((classifier_base() & 0xff000000u) == 0x04000000u) {
        runtime.report_creature_base_function_misuse();
        runtime.rebuild_creature_selection_menu();
    }
    runtime.add_to_world_object_registry(*this);
}

void Object::set_deletion_movement_bounds() {
    movement_bounds_ = {0, 0, 0x4140, 0x960};
}

void Object::enable_ticking() {
    tick_enabled_ = true;
}

void Object::disable_ticking() {
    tick_enabled_ = false;
}

void Object::compute_sound_attenuation_and_pan(
    const ObjectSoundViewportHost& renderer, int& out_attenuation,
    int& out_pan) const {
    const world::ViewportBounds viewport = renderer.sound_viewport();
    const int source_x = sound_source_x();
    const int source_y = sound_source_y();
    const int viewport_center_y = (viewport.bottom + viewport.top) / 2;

    int horizontal_offset =
        source_x - (viewport.left + viewport.right) / 2;
    if (horizontal_offset < -0x104f) {
        horizontal_offset += world::kWorldWidth;
    } else if (horizontal_offset >= 0x1050) {
        horizontal_offset -= world::kWorldWidth;
    }

    const int viewport_width = viewport.right - viewport.left;
    const int raw_pan = (horizontal_offset * 5000) / viewport_width;
    out_pan = std::clamp(raw_pan, -10000, 10000);

    const int horizontal_distance = std::abs(horizontal_offset);
    const int vertical_distance =
        std::abs(source_y - viewport_center_y);
    const int viewport_height = viewport.bottom - viewport.top;
    const int vertical_excess = std::max(
        0, vertical_distance - viewport_height / 2);
    const int horizontal_excess = std::max(
        0, horizontal_distance - viewport_width / 2);

    const int attenuation =
        (horizontal_excess * -10000) / viewport_width -
        (vertical_excess * 10000) / viewport_height;
    out_attenuation = std::max(-10000, attenuation);
}

void Object::play_sound_effect(sound::SoundId sound_id,
                               int queue_delay_ticks,
                               bool force_during_archive,
                               ObjectSoundPlaybackHost& playback) {
    if (playback.sounds_muted() && !force_during_archive) {
        return;
    }
    if (sound_audibility_state(playback) ==
        SoundAudibilityState::out_of_range) {
        return;
    }

    int attenuation = 0;
    int pan = 0;
    compute_sound_attenuation_and_pan(playback, attenuation, pan);
    const int result = playback.sound_manager().play_or_queue(
        sound_id, queue_delay_ticks, attenuation, pan);

    if (playback.debug_console_available()) {
        playback.log_sound_event(
            queue_delay_ticks == 0
                ? ObjectSoundPlaybackHost::LogEvent::play_immediate
                : ObjectSoundPlaybackHost::LogEvent::queue_sound,
            sound_id, queue_delay_ticks);
        if (result != sound::kSoundSuccess) {
            playback.log_sound_event(
                ObjectSoundPlaybackHost::LogEvent::sound_error, sound_id,
                result);
        }
    }
}

void Object::set_continuous_sound(
    sound::SoundId sound_id, bool persist_when_out_of_range,
    ObjectSoundPlaybackHost& playback) {
    if (playback.sounds_muted()) {
        return;
    }

    const sound::SoundId previous_sound_id = continuous_sound_descriptor_;
    if (sound_audibility_state(playback) ==
        SoundAudibilityState::out_of_range) {
        if (!persist_when_out_of_range) {
            return;
        }
        if (sound_handle_ >= 0) {
            if (playback.debug_console_available()) {
                playback.log_sound_event(
                    ObjectSoundPlaybackHost::LogEvent::
                        continuous_offscreen_replace,
                    previous_sound_id, sound_handle_);
            }
            playback.sound_manager().stop_continuous_sound(
                static_cast<std::uint32_t>(sound_handle_), true);
        }
        sound_handle_ = -1;
    } else {
        int attenuation = 0;
        int pan = 0;
        compute_sound_attenuation_and_pan(playback, attenuation, pan);

        sound::SoundChannelHandle new_handle = -1;
        const int result = playback.sound_manager().start_continuous_sound(
            sound_id, new_handle, attenuation, pan,
            persist_when_out_of_range);
        if (result != sound::kSoundSuccess) {
            if (playback.debug_console_available()) {
                playback.log_sound_event(
                    ObjectSoundPlaybackHost::LogEvent::sound_error,
                    sound_id, result);
            }
            return;
        }

        if (sound_handle_ >= 0) {
            if (playback.debug_console_available()) {
                playback.log_sound_event(
                    ObjectSoundPlaybackHost::LogEvent::continuous_replace,
                    previous_sound_id, sound_handle_);
            }
            playback.sound_manager().stop_continuous_sound(
                static_cast<std::uint32_t>(sound_handle_), true);
        }
        sound_handle_ = new_handle;
    }

    continuous_sound_persist_flag_ = persist_when_out_of_range;
    continuous_sound_descriptor_ = sound_id;
}

void Object::fade_continuous_sound(ObjectSoundPlaybackHost& playback) {
    if (sound_handle_ >= 0) {
        playback.sound_manager().stop_continuous_sound(
            static_cast<std::uint32_t>(sound_handle_), true);
    }
    clear_continuous_sound_state();
}

void Object::stop_continuous_sound(ObjectSoundPlaybackHost& playback) {
    if (sound_handle_ >= 0) {
        playback.sound_manager().stop_continuous_sound(
            static_cast<std::uint32_t>(sound_handle_), false);
    }
    clear_continuous_sound_state();
}

void Object::update_sound(ObjectSoundPlaybackHost& playback) {
    if (continuous_sound_descriptor_ == 0) {
        return;
    }

    if (sound_handle_ < 0) {
        if (sound_audibility_state(playback) ==
            SoundAudibilityState::out_of_range) {
            return;
        }
        if (playback.sounds_muted()) {
            return;
        }

        int attenuation = 0;
        int pan = 0;
        compute_sound_attenuation_and_pan(playback, attenuation, pan);
        sound::SoundChannelHandle new_handle = -1;
        const int result = playback.sound_manager().start_continuous_sound(
            continuous_sound_descriptor_, new_handle, attenuation, pan,
            continuous_sound_persist_flag_);
        if (result == sound::kSoundSuccess && new_handle != -1) {
            sound_handle_ = new_handle;
            if (playback.debug_console_available()) {
                playback.log_sound_event(
                    ObjectSoundPlaybackHost::LogEvent::
                        continuous_reentered_range,
                    continuous_sound_descriptor_, sound_handle_);
            }
            return;
        }

        if (playback.debug_console_available()) {
            playback.log_sound_event(
                ObjectSoundPlaybackHost::LogEvent::sound_error,
                continuous_sound_descriptor_, result);
        }
        return;
    }

    sound::SoundManager& sound_manager = playback.sound_manager();
    if (!sound_manager.mixer_suspended() &&
        sound_handle_ < static_cast<std::int32_t>(sound::kSoundChannelCount)) {
        if (sound_manager.continuous_channel_is_playing(sound_handle_)) {
            if (sound_audibility_state(playback) ==
                SoundAudibilityState::out_of_range) {
                if (playback.debug_console_available()) {
                    playback.log_sound_event(
                        ObjectSoundPlaybackHost::LogEvent::
                            continuous_gone_out_of_range,
                        continuous_sound_descriptor_, sound_handle_);
                }
                sound_manager.stop_continuous_sound(
                    static_cast<std::uint32_t>(sound_handle_), false);
                sound_handle_ = -1;
                if (continuous_sound_persist_flag_) {
                    return;
                }
                continuous_sound_descriptor_ = 0;
                return;
            }

            int attenuation = 0;
            int pan = 0;
            compute_sound_attenuation_and_pan(playback, attenuation, pan);
            if (sound_manager.update_continuous_channel(
                    sound_handle_, attenuation, pan)) {
                return;
            }
            return;
        }

        if (playback.debug_console_available()) {
            playback.log_sound_event(
                ObjectSoundPlaybackHost::LogEvent::
                    continuous_finished_channel,
                continuous_sound_descriptor_, sound_handle_);
        }
    }

    if (playback.debug_console_available()) {
        playback.log_sound_event(
            ObjectSoundPlaybackHost::LogEvent::continuous_finished_sound,
            continuous_sound_descriptor_, sound_handle_);
    }
    sound_manager.stop_continuous_sound(
        static_cast<std::uint32_t>(sound_handle_), false);
    sound_handle_ = -1;
    continuous_sound_descriptor_ = 0;
}

SoundAudibilityState Object::sound_audibility_state(
    const ObjectSoundViewportHost& renderer) const {
    const world::ViewportBounds viewport = renderer.sound_viewport();
    world::WorldRect object_bounds{};
    get_bounds(&object_bounds);

    const world::WorldRect active_viewport{
        viewport.left, viewport.top, viewport.right, viewport.bottom};
    if (world::wrapped_world_rects_overlap(active_viewport,
                                           object_bounds)) {
        return SoundAudibilityState::active_viewport;
    }

    const int half_width = (viewport.right - viewport.left) / 2;
    const int half_height = (viewport.bottom - viewport.top) / 2;
    const int extended_left = viewport.left - half_width;
    const int extended_right = viewport.right + half_width;
    const int extended_top = std::max(0, viewport.top - half_height);
    const int extended_bottom =
        std::min(world::kWorldHeight, viewport.bottom + half_height);

    // A negative left edge is represented in the wrapped copy of the world,
    // matching the native routine's paired +world-width comparisons.
    const world::WorldRect extended_viewport{
        extended_left < 0 ? extended_left + world::kWorldWidth
                          : extended_left,
        extended_top,
        extended_left < 0 ? extended_right + world::kWorldWidth
                          : extended_right,
        extended_bottom};
    if (world::wrapped_world_rects_overlap(extended_viewport,
                                           object_bounds)) {
        return SoundAudibilityState::extended_range;
    }
    return SoundAudibilityState::out_of_range;
}

void Object::set_bounds_mode(std::uint32_t requested_mode,
                             ObjectRenderableSetHost& renderables) {
    const BoundsMode requested = static_cast<BoundsMode>(
        static_cast<std::uint8_t>(requested_mode));
    const auto is_unbounded = [](BoundsMode mode) {
        return mode == BoundsMode::unbounded_1 ||
               mode == BoundsMode::unbounded_2;
    };

    const bool was_unbounded = is_unbounded(bounds_mode_);
    const bool is_now_unbounded = is_unbounded(requested);
    if (was_unbounded && !is_now_unbounded) {
        renderables.erase(*this);
    } else if (!was_unbounded && is_now_unbounded &&
               !renderables.contains(*this)) {
        renderables.insert(*this);
    }

    bounds_mode_ = requested;
}

void Object::update_movement_bounds(ObjectMovementBoundsHost& world_host) {
    switch (bounds_mode_) {
    case BoundsMode::default_world:
        if ((bounds_flags_ & 0x40u) == 0) {
            movement_bounds_ = {0, 0, world::kWorldWidth,
                                world::kWorldHeight};
        } else {
            world::WorldRect object_bounds{};
            get_bounds(&object_bounds);
            world_host.find_nearest_room_bounds_at_point(
                object_bounds.min_x, object_bounds.max_y, movement_bounds_);
        }
        break;

    case BoundsMode::unbounded_1:
    case BoundsMode::unbounded_2:
        movement_bounds_ = {0, 0, 0x7fff, 0x7fff};
        break;

    case BoundsMode::vehicle_local: {
        // The native path requires a Vehicle reference. Vehicle geometry and
        // its primary Entity are supplied by the owning world adapter.
        Object& vehicle = *bounds_reference_object_;
        movement_bounds_ = world_host.vehicle_local_bounds(vehicle);
        const int vehicle_x = world_host.vehicle_primary_entity_x(vehicle);
        const int vehicle_y = world_host.vehicle_primary_entity_y(vehicle);
        movement_bounds_.min_x += vehicle_x;
        movement_bounds_.max_x += vehicle_x;
        movement_bounds_.min_y += vehicle_y;
        movement_bounds_.max_y += vehicle_y;
        break;
    }

    case BoundsMode::explicit_rectangle:
        if (bounds_reference_object_ != nullptr) {
            movement_bounds_ = bounds_reference_object_->movement_bounds_;
        }
        break;
    }

    if (world_host.edit_object() == this) {
        movement_bounds_ = {0, 0, world::kWorldWidth,
                            world::kWorldHeight};
    }
}

int Object::dispatch_script_event(ObjectEventId event_id,
                                  Object* from_object,
                                  bool force_restart,
                                  ObjectScriptDispatchHost& scripts) {
    const std::uint32_t classifier =
        classifier_base() | static_cast<std::uint32_t>(event_id);
    return scripts.execute_script_for_classifier(*this, from_object,
                                                 classifier, force_restart);
}

void Object::clear_reference_and_set_default_bounds(
    ObjectRenderableSetHost& renderables) {
    bounds_reference_object_ = nullptr;
    set_bounds_mode(static_cast<std::uint32_t>(BoundsMode::default_world),
                    renderables);
}

namespace {

creatures1::creatures::Creature* find_creature_source(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout) {
    if (event.source == nullptr) {
        return nullptr;
    }
    for (std::size_t index = 0; index < fanout.creature_count(); ++index) {
        creatures1::creatures::Creature* candidate =
            fanout.creature_at(index);
        if (candidate == nullptr) {
            fanout.report_invalid_creature_index();
            continue;
        }
        if (fanout.is_same_object(*candidate, *event.source) &&
            fanout.is_creature_classifier(*candidate)) {
            return candidate;
        }
    }
    return nullptr;
}

void queue_creature_context_zero(
    Object& target,
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout) {
    creatures1::creatures::Creature* source =
        find_creature_source(event, fanout);
    if (source == nullptr) {
        return;
    }

    QueuedCreatureStimulus stimulus{};
    if (fanout.copy_built_in_stimulus(*source, 0, target, stimulus)) {
        fanout.queue_creature_stimulus(stimulus);
    }
}

} // namespace

void Object::handle_queued_creature_event(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout) {
    queue_creature_context_zero(*this, event, fanout);
}

void Object::handle_queued_event_3(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout) {
    queue_creature_context_zero(*this, event, fanout);
}

bool Object::can_be_destroyed(const ObjectLifetimeHost& runtime) const {
    for (std::size_t index = 0; index < runtime.object_count(); ++index) {
        Object* object = runtime.object_at(index);
        if (object == nullptr) {
            runtime.report_invalid_index();
            return false;
        }
        if (object->references_object(const_cast<Object*>(this))) {
            return false;
        }
    }

    for (std::size_t index = 0; index < runtime.running_macro_count();
         ++index) {
        const scripting::MacroObjectContext* context =
            runtime.running_macro_at(index);
        if (context == nullptr ||
            context->script_owner == const_cast<Object*>(this) ||
            context->from_object == const_cast<Object*>(this) ||
            context->exec_object == const_cast<Object*>(this) ||
            context->it_object == const_cast<Object*>(this) ||
            context->target_object == const_cast<Object*>(this)) {
            return false;
        }
    }

    if (scripting::deferred_script_events_reference(this)) {
        return false;
    }

    for (std::size_t index = 0; index < runtime.immediate_event_count();
         ++index) {
        const QueuedObjectEvent* event = runtime.immediate_event_at(index);
        if (event == nullptr || event->target == this || event->source == this) {
            return false;
        }
    }

    for (std::size_t index = 0; index < runtime.delayed_event_count();
         ++index) {
        const QueuedObjectEvent* event = runtime.delayed_event_at(index);
        if (event == nullptr || !runtime.delayed_event_is_active(*event)) {
            continue;
        }
        if (event->target == this || event->source == this) {
            return false;
        }
    }

    for (std::size_t index = 0; index < runtime.queued_stimulus_count();
         ++index) {
        const QueuedCreatureStimulus* stimulus =
            runtime.queued_stimulus_at(index);
        if (stimulus == nullptr || runtime.stimulus_targets_object(*stimulus,
                                                                    *this)) {
            return false;
        }
    }
    return true;
}

bool Object::is_sound_source_below_world_y() const {
    constexpr int kWorldBottom = 0x4b0;
    return sound_source_y() > kWorldBottom;
}

void Object::clear_references_to(Object* candidate) {
    if (bounds_reference_object_ == candidate) {
        bounds_reference_object_ = nullptr;
    }
    if (caos_object_pointer_ == candidate) {
        caos_object_pointer_ = nullptr;
    }
}

void Object::set_classifier_components(std::uint8_t event,
                                       std::uint8_t species,
                                       std::uint8_t genus,
                                       std::uint8_t family) {
    classifier_.event = event;
    classifier_.species = species;
    classifier_.genus = genus;
    classifier_.family = family;
}

void Object::set_classifier_family(std::uint8_t family) {
    classifier_.family = family;
}

std::uint32_t Object::classifier_base() const {
    return static_cast<std::uint32_t>(classifier_.event) |
           (static_cast<std::uint32_t>(classifier_.species) << 8) |
           (static_cast<std::uint32_t>(classifier_.genus) << 16) |
           (static_cast<std::uint32_t>(classifier_.family) << 24);
}

} // namespace creatures1::objects
