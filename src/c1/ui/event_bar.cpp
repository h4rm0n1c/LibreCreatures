#include "event_bar.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kSetStatusBarPartsMessage = 0x404;
constexpr std::uint32_t kDisabledPaneFlag = 0x04000000;
constexpr std::uint32_t kSpringPaneFlag = 0x08000000;
constexpr std::uint32_t kSizeGripStyle = 0x100;
constexpr std::int32_t kSizeGripMetric = 2;
constexpr std::int32_t kSizeGripBorder = 6;

bool is_disabled(const StatusBarPaneInfo& pane) {
    return pane.width == 0 && (pane.style_flags & kDisabledPaneFlag) != 0;
}

bool is_spring(const StatusBarPaneInfo& pane) {
    return (pane.style_flags & kSpringPaneFlag) != 0;
}

} // namespace

void EventBar::window_proc(EventBarWindowApi& window,
                           std::uint32_t message,
                           std::uint32_t pane_count,
                           std::int32_t* pane_right_edges) const {
    if (message == kSetStatusBarPartsMessage && pane_right_edges != nullptr &&
        pane_count > 1) {
        std::int32_t disabled_width_shift = 0;
        std::int32_t previous_edge = 0;
        for (std::uint32_t index = 0; index < pane_count; ++index) {
            const std::int32_t pane_delta =
                pane_right_edges[index] - previous_edge;
            previous_edge = pane_right_edges[index];
            const StatusBarPaneInfo pane = window.pane_info(index);
            if (is_disabled(pane)) {
                disabled_width_shift += pane_delta;
            }
            pane_right_edges[index] -= disabled_width_shift;
        }

        if (disabled_width_shift > 0) {
            pane_right_edges[0] += disabled_width_shift;
            for (std::uint32_t index = 1; index < pane_count; ++index) {
                const StatusBarPaneInfo pane = window.pane_info(index);
                if (is_disabled(pane)) {
                    pane_right_edges[index] = pane_right_edges[index - 1];
                } else {
                    pane_right_edges[index] += disabled_width_shift;
                }
            }
        }

        std::int32_t available_right = window.client_rect().right;
        if ((window.window_style() & kSizeGripStyle) != 0 &&
            window.has_parent_window() && !window.parent_is_zoomed()) {
            available_right -= window.system_metric(kSizeGripMetric);
        }

        const std::int32_t overflow =
            pane_right_edges[pane_count - 1] - available_right;
        if (overflow > 0) {
            std::int32_t accumulated_shrink = 0;
            previous_edge = 0;
            for (std::uint32_t index = 0; index < pane_count; ++index) {
                const std::int32_t edge = pane_right_edges[index];
                std::int32_t pane_width = edge - previous_edge;
                previous_edge = edge;

                if (accumulated_shrink < overflow) {
                    const StatusBarPaneInfo pane = window.pane_info(index);
                    if (pane.width == 0) {
                        if ((pane.style_flags & kDisabledPaneFlag) != 0) {
                            accumulated_shrink += pane_width;
                        } else if (is_spring(pane)) {
                            pane_width = std::min(
                                overflow - accumulated_shrink, pane_width);
                            accumulated_shrink += pane_width;
                        }
                    } else {
                        pane_width = pane_width - pane.width - kSizeGripBorder;
                        if (pane_width > 0) {
                            pane_width = std::min(
                                overflow - accumulated_shrink, pane_width);
                            accumulated_shrink += pane_width;
                        }
                    }
                }

                const std::int32_t shrink =
                    std::min(accumulated_shrink, overflow);
                pane_right_edges[index] -= shrink;
            }
        }
    }

    window.def_window_proc(message,
                            static_cast<std::uintptr_t>(pane_count),
                            reinterpret_cast<std::intptr_t>(pane_right_edges));
}

void EventBar::serialize(EventBarArchiveApi& archive,
                         EventBarLegacyArchiveWords& legacy_words) {
    if (archive.is_loading()) {
        const std::int32_t archived_count =
            archive.read_displayed_object_count();
        const std::size_t archived_size =
            static_cast<std::size_t>(std::max(archived_count, 0));
        const std::size_t stored_count =
            std::min(archived_size, kMaximumDisplayedObjects);
        displayed_objects_.fill(nullptr);
        displayed_count_ = 0;
        for (std::size_t index = 0; index < stored_count; ++index) {
            objects::Object* object = archive.read_object();
            if (object != nullptr) {
                displayed_objects_[displayed_count_] = object;
                ++displayed_count_;
            }
        }
        for (std::size_t index = stored_count;
             index < archived_size; ++index) {
            static_cast<void>(archive.read_object());
        }
        legacy_words.first = archive.read_legacy_state_word();
        legacy_words.second = archive.read_legacy_state_word();
        return;
    }

    archive.write_displayed_object_count(
        static_cast<std::int32_t>(displayed_count_));
    for (std::size_t index = 0; index < displayed_count_; ++index) {
        archive.write_object(displayed_objects_[index]);
    }
    archive.write_legacy_state_word(legacy_words.first);
    archive.write_legacy_state_word(legacy_words.second);
}

void EventBar::add_object(objects::Object* object,
                          EventBarObjectPolicyApi& policy) {
    // A null CAOS object reference cannot be displayed.  The recovered
    // routine stores it and then refreshes immediately, where the native
    // implementation dereferences it unconditionally.  Keep the list's
    // invariant here so malformed or transient references cannot take down
    // the UI.
    if (object == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < displayed_count_; ++index) {
        if (displayed_objects_[index] == object) {
            policy.refresh_display_panes();
            return;
        }
    }

    if (displayed_count_ == kMaximumDisplayedObjects) {
        remove_object(displayed_objects_.front(), true, policy);
    }

    displayed_objects_[displayed_count_] = object;
    ++displayed_count_;
    policy.refresh_display_panes();
}

void EventBar::remove_object(objects::Object* object,
                             bool record_auxiliary_state,
                             EventBarObjectPolicyApi& policy) {
    std::size_t match = displayed_count_;
    for (std::size_t index = 0; index < displayed_count_; ++index) {
        if (displayed_objects_[index] == object) {
            match = index;
            break;
        }
    }
    if (match == displayed_count_) {
        return;
    }

    if (object != nullptr && record_auxiliary_state && policy.is_creature(*object) &&
        policy.creature_is_dead(*object)) {
        if (policy.funeral_state_word_count() < 16) {
            policy.append_funeral_state_word(
                policy.genome_filename_id(*object));
            policy.flush_funeral_kit_state();
        }
        policy.flush_funeral_kit_state();
        policy.disable_viewport_navigation();
    }

    for (std::size_t index = match; index + 1 < displayed_count_; ++index) {
        displayed_objects_[index] = displayed_objects_[index + 1];
    }
    --displayed_count_;
    displayed_objects_[displayed_count_] = nullptr;
    policy.refresh_display_panes();
}

void EventBar::refresh_object_display_panes(
    EventBarStatusApi& status,
    const EventBarObjectPolicyApi& policy) const {
    std::uint32_t pane_index = 10;
    for (std::size_t object_index = 0; object_index < displayed_count_;
         ++object_index, --pane_index) {
        const objects::Object* object = displayed_objects_[object_index];
        if (object == nullptr) {
            continue;
        }
        const std::uint32_t resource_id =
            policy.is_creature(*object)
                ? (policy.creature_is_dead(*object) ? 0xef1b : 0xef1c)
                : 0xef1d;
        const std::string text = status.object_pane_text(*object, resource_id);
        status.set_pane_text(pane_index, text);
        status.set_pane_width(pane_index, status.measure_text(text).width);
    }

    for (std::uint32_t hidden_pane =
             static_cast<std::uint32_t>(kMaximumDisplayedObjects -
                                        displayed_count_);
         hidden_pane != 0; --hidden_pane) {
        status.set_pane_disabled(hidden_pane, true);
    }

    status.set_pane_disabled(0x0b, !status.selected_creature_exists());
    update_status_panes(status);
    status.invalidate_status_bar();
}

namespace {

// The status-bar string resources are fixed-width display templates whose
// trailing placeholder is dropped to leave the label.  The placeholder length
// is per-pane and locale-independent, which is why the native uses a constant.
std::string status_label_prefix(std::string text, std::size_t placeholder) {
    if (text.size() >= placeholder) {
        text.resize(text.size() - placeholder);
    }
    return text;
}

} // namespace

void EventBar::update_status_panes(EventBarStatusApi& status) const {
    if (status.selected_creature_exists()) {
        std::string life_force_text;
        if (status.selected_creature_is_dead()) {
            life_force_text = status.load_string(0xef1b);
        } else {
            const auto glycogen =
                static_cast<std::int32_t>(status.selected_creature_glycogen());
            const std::int32_t percentage = glycogen * 100 / 255;
            // 0xef1a is a pane-width template ("Life Force: 100%"), not a
            // format string: UpdateStatusPanes @004168b0 takes Left(len - 4)
            // to drop the "100%" placeholder and appends the real value.
            life_force_text = status_label_prefix(status.load_string(0xef1a), 4);
            life_force_text += std::to_string(percentage);
            life_force_text += '%';
        }
        status.set_pane_width(0x0b,
                              status.measure_text(life_force_text).width);
        status.set_pane_text(0x0b, life_force_text);
    }

    const EventBarScoreSnapshot score = status.score_snapshot();
    const std::int64_t raw_score =
        static_cast<std::int64_t>(score.population_time_accumulator) +
        static_cast<std::int64_t>(score.natural_eggs_laid) * 256;
    const auto display_score = static_cast<std::int32_t>(std::clamp<std::int64_t>(
        raw_score, 0, 99999));
    // 0xef19 is the same shape with a five-digit placeholder ("Score: 00000"),
    // so the native strips Left(len - 5).
    std::string score_text = status_label_prefix(status.load_string(0xef19), 5);
    score_text += std::to_string(display_score);
    status.set_pane_width(0x0c, status.measure_text(score_text).width);
    status.set_pane_text(0x0c, score_text);

    const std::uint32_t ticks = status.world_tick_count();
    std::ostringstream time_text;
    time_text << (ticks / 36000) << "h:" << std::setfill('0')
              << ((ticks / 600) % 60) << "m";
    const std::string world_time = time_text.str();
    status.set_pane_width(0x0d, status.measure_text(world_time).width);
    status.set_pane_text(0x0d, world_time);
}

void EventBar::on_left_button_down(
    std::uint32_t mouse_flags,
    long mouse_x,
    long mouse_y,
    EventBarInteractionApi& interaction,
    EventBarObjectPolicyApi& policy) {
    static_cast<void>(mouse_flags);
    for (std::size_t object_index = 0; object_index < displayed_count_;
         ++object_index) {
        const std::uint32_t pane_index =
            static_cast<std::uint32_t>(kMaximumDisplayedObjects - object_index);
        if (!interaction.point_is_inside_pane(pane_index, mouse_x, mouse_y)) {
            continue;
        }

        objects::Object* object = displayed_objects_[object_index];
        if (object == nullptr) {
            continue;
        }
        interaction.request_viewport_origin(
            object->sound_source_x() - interaction.viewport_width() / 2,
            object->sound_source_y() - interaction.viewport_height() / 2);

        if (!interaction.is_creature(*object)) {
            if (interaction.viewport_navigation_is_disabled()) {
                return;
            }
            interaction.disable_viewport_navigation();
            return;
        }

        interaction.select_creature(*object);
        if (interaction.creature_is_dead(*object)) {
            interaction.notify_embedded_kit_of_death(*object);
            interaction.flush_funeral_kit_document_state();
            remove_object(object, false, policy);
            return;
        }

        const std::string creature_name = interaction.object_display_name(*object);
        if (creature_name == interaction.unnamed_creature_label() &&
            !interaction.embedded_kit_is_connected(2)) {
            interaction.execute_embedded_kit_tool(2);
        }
        remove_object(object, false, policy);
        return;
    }
}

void add_object_to_event_bar_display_list(
    EventBar& event_bar,
    objects::Object* object,
    EventBarObjectPolicyApi& policy) {
    event_bar.add_object(object, policy);
}

} // namespace creatures1::ui
