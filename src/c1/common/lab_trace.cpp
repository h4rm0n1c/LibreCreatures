#include "lab_trace.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace creatures1::common {

namespace {

struct CategoryName {
    const char* name;
    LabTrace category;
};

constexpr CategoryName kCategoryNames[] = {
    {"kits", LabTrace::kits},           {"selection", LabTrace::selection},
    {"camera", LabTrace::camera},       {"scheduler", LabTrace::scheduler},
    {"sound", LabTrace::sound},         {"lifecycle", LabTrace::lifecycle},
    {"kitpipe", LabTrace::kit_pipe},    {"attention", LabTrace::attention},
};

const char* category_name(LabTrace category) {
    for (const CategoryName& entry : kCategoryNames) {
        if (entry.category == category) {
            return entry.name;
        }
    }
    return "?";
}

std::uint32_t parse_categories(const char* setting) {
    if (setting == nullptr || *setting == '\0' ||
        std::strcmp(setting, "all") == 0) {
        return 0xffffffffu;
    }
    std::uint32_t mask = 0;
    const std::string text(setting);
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find(',', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string word = text.substr(start, end - start);
        for (const CategoryName& entry : kCategoryNames) {
            if (word == entry.name) {
                mask |= static_cast<std::uint32_t>(entry.category);
            }
        }
        start = end + 1;
    }
    return mask;
}

class Trace {
public:
    static Trace& instance() {
        static Trace trace;
        return trace;
    }

    bool enabled(LabTrace category) const {
        return file_ != nullptr &&
               (mask_ & static_cast<std::uint32_t>(category)) != 0;
    }

    void write(LabTrace category, const char* format, std::va_list arguments) {
        char text[1024];
        std::vsnprintf(text, sizeof(text), format, arguments);
        // One event per line: fold any line breaks in the text.
        for (char* c = text; *c != '\0'; ++c) {
            if (*c == '\n' || *c == '\r' || *c == '\t') {
                *c = ' ';
            }
        }
        std::size_t length = std::strlen(text);
        while (length != 0 && text[length - 1] == ' ') {
            text[--length] = '\0';
        }
        const long long elapsed_ms = now_ms();
        std::lock_guard<std::mutex> lock(mutex_);
        std::fprintf(file_, "%lu\t%lld\t%s\t%s\n",
                     static_cast<unsigned long>(world_tick_),
                     elapsed_ms,
                     category_name(category), text);
        std::fflush(file_);
    }

    void set_world_tick(std::uint32_t tick) { world_tick_ = tick; }

    // Windows' uptime in milliseconds, the clock the Lab Kit's log uses
    // too, so the lab can put the two in one timeline.
    static long long now_ms() {
#if defined(_WIN32)
        return static_cast<long long>(GetTickCount64());
#else
        return static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
#endif
    }

    bool active() const { return file_ != nullptr && mask_ != 0; }
    std::string& context() { return context_; }

private:
    Trace() {
        const char* path = std::getenv("C1_LAB_TRACE");
        if (path == nullptr || *path == '\0') {
            return;
        }
        mask_ = parse_categories(std::getenv("C1_LAB_TRACE_CATEGORIES"));
        file_ = std::fopen(path, "a");
    }

    std::FILE* file_ = nullptr;
    std::uint32_t mask_ = 0;
    std::uint32_t world_tick_ = 0;
    std::string context_;
    std::mutex mutex_;
};

} // namespace

bool lab_trace_enabled(LabTrace category) {
    return Trace::instance().enabled(category);
}

void lab_trace(LabTrace category, const char* format, ...) {
    Trace& trace = Trace::instance();
    if (!trace.enabled(category)) {
        return;
    }
    std::va_list arguments;
    va_start(arguments, format);
    trace.write(category, format, arguments);
    va_end(arguments);
}

LabTraceContext::LabTraceContext(const char* format, ...) {
    Trace& trace = Trace::instance();
    if (!trace.active()) {
        return;
    }
    char text[256];
    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    active_ = true;
    previous_ = trace.context();
    trace.context() = text;
}

LabTraceContext::~LabTraceContext() {
    if (active_) {
        Trace::instance().context() = std::move(previous_);
    }
}

const char* lab_trace_context() {
    const std::string& context = Trace::instance().context();
    return context.empty() ? "-" : context.c_str();
}

void lab_trace_set_world_tick(std::uint32_t tick) {
    Trace::instance().set_world_tick(tick);
}

} // namespace creatures1::common
