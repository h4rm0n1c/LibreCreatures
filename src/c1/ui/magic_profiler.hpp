#pragma once

#include "../brain/classifiers.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::ui {

// The report generator receives a snapshot from the world/application owner.
// These values are C1 diagnostics; the MFC window, CString, and stream
// implementations used to obtain or display them remain platform boundaries.
struct MagicProfilerSnapshot {
    std::string world_save_path;
    std::string world_name;
    std::uint32_t tick_count = 0;

    std::uint32_t creature_count = 0;
    std::uint32_t object_count = 0;
    std::uint32_t scenery_count = 0;
    std::uint32_t entity_count = 0;
    std::uint32_t active_script_count = 0;
    std::uint32_t message_queue_count = 0;
    std::uint32_t delayed_message_count = 0;
    std::uint32_t stimulus_count = 0;
    std::uint32_t death_row_count = 0;
    std::uint32_t stuffed_norn_word_count = 0;
    double smoothed_idle_cycle_time = 0.0;

    brain::ClassifierNameMap classifier_names;

    struct ClassifierRow {
        brain::ClassifierId classifier{};
        std::string display_name;
        std::uint32_t population = 0;
        std::uint32_t active_script_count = 0;
    };
    std::vector<ClassifierRow> classifier_rows;
};

// CWnd::MessageBoxA is deliberately represented as an interface.  It is an
// output boundary, not a reason to expose MFC types in the recovered source.
class MagicProfilerWindowApi {
public:
    virtual ~MagicProfilerWindowApi() = default;

    virtual void show_message(std::string_view title,
                              std::string_view message) = 0;
};

// Writes the developer diagnostic report beside the current world save.  The
// returned path is empty when the report could not be opened or written.
std::string generate_magic_profiler_report(
    MagicProfilerWindowApi& window,
    const MagicProfilerSnapshot& snapshot);

}  // namespace creatures1::ui
