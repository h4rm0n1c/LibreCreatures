#include "logging.hpp"

namespace creatures1::common {

namespace {

constexpr std::size_t kLogLineCapacity = 0x1280;
constexpr std::size_t kConsoleTrimThreshold = 20000;
constexpr double kDebugLogRetentionRatio = 0.75;

std::size_t highest_set_bit_index(std::uint32_t mask) {
    std::size_t index = 0;
    while ((mask >>= 1u) != 0) {
        ++index;
    }
    return index;
}

} // namespace

void debug_log(DebugLogHost& host,
               std::uint32_t category_mask,
               const char* format,
               ...) {
    // The original tests the global category mask before it performs any
    // formatting, allocation, or UI work.
    if (!host.debug_category_enabled(category_mask)) {
        return;
    }

    std::va_list arguments;
    va_start(arguments, format);
    const std::string formatted_message =
        host.format_message(format, arguments);
    va_end(arguments);

    const std::size_t category_index =
        highest_set_bit_index(category_mask);
    std::string line = host.format_line_prefix(
        host.category_tag(category_index), host.world_tick());
    line += formatted_message;
    line += "\r\n";

    if (line.size() >= kLogLineCapacity) {
        // The native checked concatenation routes an overlong record to its
        // CRT invalid-parameter/fatal boundary.  Keep that behavior behind
        // the host rather than exporting the CRT handler as C1 code.
        host.handle_oversized_log_line(line, kLogLineCapacity);
        return;
    }

    if (host.log_filter_accepts(line)) {
        host.append_console_text(line);
        host.mark_console_log_dirty();

        if (host.console_text_length() > kConsoleTrimThreshold) {
            const std::size_t retained_length = static_cast<std::size_t>(
                static_cast<double>(host.console_text_length()) *
                kDebugLogRetentionRatio);
            host.retain_newest_console_text(retained_length);
        }

        if (host.log_file_is_open()) {
            host.mirror_to_log_file(line);
        }
    }

    // Refresh is deliberately outside the filter-acceptance branch: the
    // binary invokes the dialog refresh routine for every enabled category.
    host.refresh_console_output();
}

} // namespace creatures1::common
