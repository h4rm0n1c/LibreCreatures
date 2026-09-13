#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::ui {

struct WorldStatisticsRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

// This is the small, platform-neutral message projection consumed by the
// Ctrl+period close shortcut. USER32's MSG and GetKeyState remain in the
// native window adapter.
struct WorldStatisticsKeyMessage {
    std::uint32_t message = 0;
    std::uint32_t key = 0;
    bool control_down = false;
};

// The frame formats these values but does not own the registries or queues
// from which they are read. The application/world adapter supplies a
// snapshot, keeping CObArray, MFC CString, and the packed C1 globals outside
// the recovered UI policy.
struct WorldStatisticsSnapshot {
    int creature_count = 0;
    int egg_count = 0;
    int object_count = 0;
    int entity_count = 0;
    int total_script_count = 0;
    int active_script_count = 0;
    int death_row_count = 0;
    int stuffed_norn_count = 0;
    int message_count = 0;
    int delayed_message_count = 0;
    int stimulus_count = 0;
    int idle_time = 0;
};

// MFC CFrameWnd/CStatic/CFont, USER32 timers/messages, CString formatting,
// and world-global registry access are supplied by this host. The functions
// below retain C1's ordering and constants without recreating framework
// object layouts or compiler-generated deleting glue.
class WorldStatisticsHost {
public:
    virtual ~WorldStatisticsHost() = default;

    virtual int initialise_base_frame() = 0;
    virtual WorldStatisticsRect client_rect() const = 0;
    virtual void create_display(std::string_view initial_text,
                                const WorldStatisticsRect& bounds,
                                std::uint32_t control_id) = 0;
    virtual void apply_statistics_font(int point_size) = 0;
    virtual WorldStatisticsSnapshot read_snapshot() const = 0;
    virtual void set_display_text(std::string_view text) = 0;
    virtual void start_timer(std::uint32_t timer_id,
                             std::uint32_t interval_ms) = 0;
    virtual void kill_timer(std::uint32_t timer_id) = 0;
    virtual void close_frame() = 0;
    virtual void default_window_message() = 0;
    virtual bool base_pre_translate(
        const WorldStatisticsKeyMessage& message) = 0;
};

std::string format_world_statistics(const WorldStatisticsSnapshot& snapshot);
void refresh_world_statistics(WorldStatisticsHost& host);
int create_world_statistics_frame(WorldStatisticsHost& host);
void on_world_statistics_timer(WorldStatisticsHost& host,
                               std::uint32_t timer_id);
bool pretranslate_world_statistics(
    WorldStatisticsHost& host,
    const WorldStatisticsKeyMessage& message);
void close_world_statistics_frame(WorldStatisticsHost& host);

} // namespace creatures1::ui
