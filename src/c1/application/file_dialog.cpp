#include "file_dialog.hpp"

#include <algorithm>

namespace creatures1::application {
namespace {

constexpr std::size_t kPathCapacity = 0x100;
constexpr std::uint32_t kFileDialogFlags = 0x2000e;

std::string normalize_export_path(std::string_view selected_path) {
    std::string reversed(selected_path.rbegin(), selected_path.rend());
    const std::string::size_type dot = reversed.find('.');
    // CString::Find returns -1 when there is no dot.  The recovered code then
    // adds one, so the prefix length is zero in that case.
    const std::size_t extension_prefix_end =
        dot == std::string::npos ? 0 : dot + 1;

    std::string normalized;
    normalized.reserve(reversed.size());
    normalized.append(reversed, 0, extension_prefix_end);

    bool leading_space_pending = true;
    for (std::size_t index = extension_prefix_end;
         index < reversed.size();
         ++index) {
        const char character = reversed[index];
        if (leading_space_pending && character == ' ') {
            continue;
        }
        normalized.push_back(character);
        leading_space_pending = false;
    }

    std::reverse(normalized.begin(), normalized.end());
    return normalized;
}

} // namespace

bool prompt_for_exp_file_path(bool opening,
                              char* output_path,
                              std::string_view file_filter,
                              std::string_view dialog_title,
                              FileDialogPlatform& platform) {
    std::unique_ptr<ExportFileDialog> dialog =
        platform.create_export_file_dialog(opening,
                                           "exp",
                                           output_path,
                                           kFileDialogFlags,
                                           file_filter,
                                           dialog_title);
    if (dialog == nullptr) {
        return false;
    }

    for (;;) {
        if (dialog->show_modal() != 1) {
            return false;
        }

        const std::string selected_path = dialog->selected_path();
        if (selected_path.size() < kPathCapacity) {
            const std::string normalized_path =
                normalize_export_path(selected_path);
            platform.copy_path(output_path,
                               kPathCapacity,
                               normalized_path);
            return true;
        }

        platform.show_filename_too_long_message();
    }
}

} // namespace creatures1::application
