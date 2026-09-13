#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::common {

// The native implementation formats through MSVC's checked printf runtime and
// writes through an MFC CString/edit-control and an MFC stream.  Those are
// platform/toolchain services, not C1 source ownership, so the semantic logger
// receives them through this small adapter.
class DebugLogHost {
public:
    virtual ~DebugLogHost() = default;

    virtual std::string format_message(const char* format,
                                       std::va_list arguments) = 0;
    virtual std::string format_line_prefix(std::string_view category_tag,
                                           std::uint32_t world_tick) = 0;
    virtual std::string category_tag(std::size_t highest_set_bit) const = 0;
    virtual std::uint32_t world_tick() const = 0;
    virtual bool debug_category_enabled(std::uint32_t category_mask) const = 0;
    virtual void handle_oversized_log_line(std::string_view line,
                                           std::size_t capacity) = 0;

    virtual bool log_filter_accepts(std::string_view line) const = 0;
    virtual void append_console_text(std::string_view line) = 0;
    virtual std::size_t console_text_length() const = 0;
    virtual void retain_newest_console_text(std::size_t character_count) = 0;
    virtual void mark_console_log_dirty() = 0;
    virtual void refresh_console_output() = 0;

    virtual bool log_file_is_open() const = 0;
    virtual void mirror_to_log_file(std::string_view line) = 0;
};

// Recovered C1 logging policy.  The host owns formatting and native UI/stream
// layouts; this function owns category selection, framing, filtering, trim
// policy, dirty-state publication, and mirroring order.
void debug_log(DebugLogHost& host,
               std::uint32_t category_mask,
               const char* format,
               ...);

} // namespace creatures1::common
