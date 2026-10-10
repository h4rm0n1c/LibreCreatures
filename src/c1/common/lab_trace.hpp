#pragma once

#include <cstdint>
#include <string>

namespace creatures1::common {

// Not native.  An opt-in event trace for the c1-lab harness, off unless
// C1_LAB_TRACE names a file.  C1_LAB_TRACE_CATEGORIES selects categories by
// name, comma separated, or "all" (the default when it is unset).
//
// One line per event, tab separated:
//     <world tick> <milliseconds> <category> <text>
// The milliseconds are Windows' uptime (GetTickCount64), the clock the Lab
// Kit's log uses too.
// Each line is flushed as it is written, so the lab can read the file while
// the game runs.  The trace observes only; it never changes behaviour.
enum class LabTrace : std::uint32_t {
    kits = 1u << 0,      // kit launch and shutdown, broadcasts, messages
    selection = 1u << 1, // selected creature changes
    camera = 1u << 2,    // viewport moves, follow, script camera requests
    scheduler = 1u << 3, // scripts installed, finished and removed
    sound = 1u << 4,     // the sound manager's trace lines
    lifecycle = 1u << 5, // world load and save, births, deaths
    kit_pipe = 1u << 6,  // every kit request on the pipe and its answer
};

bool lab_trace_enabled(LabTrace category);

void lab_trace(LabTrace category, const char* format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

// What the game is doing on the trace's behalf, for lines that need a cause:
// a script's turn sets "script <family> <genus> <species> <event>", and a
// camera move made during it records that.  Scopes nest; empty when no scope
// is open.  Formatting is skipped when the trace is off.
class LabTraceContext {
public:
    explicit LabTraceContext(const char* format, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;
    ~LabTraceContext();
    LabTraceContext(const LabTraceContext&) = delete;
    LabTraceContext& operator=(const LabTraceContext&) = delete;

private:
    bool active_ = false;
    std::string previous_;
};

const char* lab_trace_context();

// The world tick written on each line.  The document sets it whenever its
// tick count changes.
void lab_trace_set_world_tick(std::uint32_t tick);

} // namespace creatures1::common
