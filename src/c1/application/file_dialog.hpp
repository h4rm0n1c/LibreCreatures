#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace creatures1::application {

class ExportFileDialog {
public:
    virtual ~ExportFileDialog() = default;

    virtual int show_modal() = 0;
    virtual std::string selected_path() const = 0;
};

class FileDialogPlatform {
public:
    virtual ~FileDialogPlatform() = default;

    virtual std::unique_ptr<ExportFileDialog> create_export_file_dialog(
        bool opening,
        std::string_view default_extension,
        char* initial_path,
        std::uint32_t flags,
        std::string_view file_filter,
        std::string_view dialog_title) = 0;

    virtual void show_filename_too_long_message() = 0;

    // This is the CRT-secure copy used by the original wrapper.  Keeping it
    // here makes the buffer contract explicit without making CRT code part of
    // the application source.
    virtual void copy_path(char* destination,
                           std::size_t destination_capacity,
                           std::string_view path) const = 0;
};

bool prompt_for_exp_file_path(bool opening,
                              char* output_path,
                              std::string_view file_filter,
                              std::string_view dialog_title,
                              FileDialogPlatform& platform);

} // namespace creatures1::application
