#include "world_statistics.hpp"

#include <array>
#include <cstdio>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kDisplayControlId = 100;
constexpr std::uint32_t kStatisticsTimerId = 1;
constexpr std::uint32_t kStatisticsTimerIntervalMs = 1000;
constexpr int kStatisticsFontPointSize = 0x55;
constexpr std::uint32_t kKeyDownMessage = 0x100;
constexpr std::uint32_t kPeriodKey = 0x2e;
constexpr int kDisplayLeftMargin = 8;
constexpr int kDisplayTopMargin = 8;
constexpr int kDisplayRightMargin = 8;
constexpr int kDisplayBottomMargin = 4;
constexpr int kQueueCapacity = 200;

constexpr std::string_view kStatisticsFormat =
    "Creatures:       %d\r\n"
    "Eggs:            %d\r\n"
    "\r\n"
    "Objects:         %d\r\n"
    "Entities:        %d\r\n"
    "\r\n"
    "Total Scripts:   %d\r\n"
    "Active Scripts:  %d\r\n"
    "\r\n"
    "Death Row:       %d\r\n"
    "Stuffed Norns:   %d\r\n"
    "\r\n"
    "Messages:        %d / %d\r\n"
    "Delayed Msgs:    %d / %d\r\n"
    "Stimuli:         %d / %d\r\n"
    "\r\n"
    "Idle Time:       %d";

int clamp_queue_count(int count) {
    if (count < 0) {
        return 0;
    }
    return count > kQueueCapacity ? kQueueCapacity : count;
}

} // namespace

std::string format_world_statistics(const WorldStatisticsSnapshot& snapshot) {
    std::array<char, 512> buffer{};
    const int length = std::snprintf(
        buffer.data(), buffer.size(), kStatisticsFormat.data(),
        snapshot.creature_count, snapshot.egg_count, snapshot.object_count,
        snapshot.entity_count, snapshot.total_script_count,
        snapshot.active_script_count, snapshot.death_row_count,
        snapshot.stuffed_norn_count, clamp_queue_count(snapshot.message_count),
        kQueueCapacity, clamp_queue_count(snapshot.delayed_message_count),
        kQueueCapacity, clamp_queue_count(snapshot.stimulus_count),
        kQueueCapacity, snapshot.idle_time);
    if (length <= 0) {
        return {};
    }
    if (static_cast<std::size_t>(length) >= buffer.size()) {
        return std::string(buffer.data(), buffer.size() - 1);
    }
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

void refresh_world_statistics(WorldStatisticsHost& host) {
    host.set_display_text(format_world_statistics(host.read_snapshot()));
}

int create_world_statistics_frame(WorldStatisticsHost& host) {
    if (host.initialise_base_frame() == -1) {
        return -1;
    }

    const WorldStatisticsRect client = host.client_rect();
    const WorldStatisticsRect display_bounds{
        kDisplayLeftMargin,
        kDisplayTopMargin,
        client.right - kDisplayRightMargin,
        client.bottom - kDisplayBottomMargin,
    };
    // Native (CWorldStatisticsFrame::OnCreate @ 0x00449c00) creates the
    // static control first and only sends WM_SETFONT to it afterward --
    // the font handle is picked (custom Consolas point font, or the stock
    // DEFAULT_GUI_FONT fallback) and applied to an already-live HWND. This
    // port's apply_statistics_font folds "create the font" and "send it to
    // the control" into one call, so create_display must run first or the
    // SendMessage lands on a not-yet-created window and is silently
    // dropped -- the control is left with no font applied but, more
    // importantly, this ordering is what native's own disassembly shows.
    host.create_display("", display_bounds, kDisplayControlId);
    host.apply_statistics_font(kStatisticsFontPointSize);
    refresh_world_statistics(host);
    host.start_timer(kStatisticsTimerId, kStatisticsTimerIntervalMs);
    return 0;
}

void on_world_statistics_timer(WorldStatisticsHost& host,
                               std::uint32_t timer_id) {
    if (timer_id == kStatisticsTimerId) {
        refresh_world_statistics(host);
    }
    host.default_window_message();
}

bool pretranslate_world_statistics(
    WorldStatisticsHost& host,
    const WorldStatisticsKeyMessage& message) {
    if (message.message == kKeyDownMessage && message.key == kPeriodKey &&
        message.control_down) {
        close_world_statistics_frame(host);
        return true;
    }
    return host.base_pre_translate(message);
}

void close_world_statistics_frame(WorldStatisticsHost& host) {
    host.kill_timer(kStatisticsTimerId);
    host.close_frame();
}

} // namespace creatures1::ui
