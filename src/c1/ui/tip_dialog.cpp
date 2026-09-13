#include "tip_dialog.hpp"

#include <cctype>

namespace creatures1::ui {
namespace {

constexpr std::string_view kTipStartup = "TipStartup";
constexpr std::string_view kTipFilePosition = "TipFilePos";
constexpr std::string_view kTipTimestamp = "TipTimeStamp";
constexpr std::string_view kTipsFileName = "tips.txt";
constexpr std::uint32_t kDefaultTipResource = 0x6e;
constexpr std::uint32_t kTipFileErrorResource = 0x6f;

bool is_ignored_tip_line(std::string_view line) {
    if (line.empty()) {
        return true;
    }
    const unsigned char first = static_cast<unsigned char>(line.front());
    return first == ' ' || first == '\t' || first == '\n' || first == ';';
}

}  // namespace

TipDialog::TipDialog(TipDialogPlatform& platform) : platform_(platform) {
    std::uint32_t startup_value = 1;
    if (!platform_.read_registry_dword(kTipStartup, startup_value)) {
        platform_.write_registry_dword(kTipStartup, startup_value);
    }
    show_at_startup_ = startup_value != 0;

    std::uint32_t saved_position = 0;
    if (!platform_.read_registry_dword(kTipFilePosition, saved_position)) {
        platform_.write_registry_dword(kTipFilePosition, 0);
    }

    std::string tips_path = platform_.primary_resource_directory();
    tips_path += kTipsFileName;
    tips_file_ = platform_.open_tips_file(tips_path);
    if (!tips_file_) {
        current_tip_text_ = platform_.load_string(kDefaultTipResource);
        return;
    }

    const std::string file_timestamp = tips_file_->modification_timestamp();
    std::string stored_timestamp;
    if (!platform_.read_registry_string(kTipTimestamp, stored_timestamp)) {
        stored_timestamp = "NULL";
        platform_.write_registry_string(kTipTimestamp, stored_timestamp);
    }
    if (file_timestamp != stored_timestamp) {
        saved_position = 0;
        platform_.write_registry_string(kTipTimestamp, file_timestamp);
    }

    if (!tips_file_->seek(static_cast<std::int32_t>(saved_position)) ||
        !read_next_tip_text()) {
        platform_.show_resource_message(kTipFileErrorResource);
    }
}

TipDialog::~TipDialog() {
    if (tips_file_) {
        platform_.write_registry_dword(kTipFilePosition,
                                       static_cast<std::uint32_t>(
                                           tips_file_->position()));
    }
}

void TipDialog::on_timer() {
    platform_.forward_default_timer();
}

void TipDialog::on_next_tip() {
    platform_.update_data(true);
    read_next_tip_text();
    platform_.update_data(false);
}

bool TipDialog::read_next_tip_text() {
    if (!tips_file_) {
        return false;
    }

    std::string line;
    for (;;) {
        if (!tips_file_->read_line(line)) {
            if (!tips_file_->rewind()) {
                platform_.show_resource_message(kTipFileErrorResource);
                return false;
            }
            continue;
        }
        if (is_ignored_tip_line(line)) {
            continue;
        }
        current_tip_text_ = std::move(line);
        return true;
    }
}

void TipDialog::on_ctl_color(std::uint32_t control_id) {
    if (platform_.is_tip_control(control_id)) {
        platform_.use_tip_control_brush();
    } else {
        platform_.forward_default_control_color();
    }
}

void TipDialog::on_ok() {
    platform_.close_dialog();
    platform_.write_registry_dword(kTipStartup, show_at_startup_ ? 1 : 0);
}

void TipDialog::on_paint() {
    // The bitmap, CDC, stock brush, and text drawing are all native GDI/MFC
    // objects.  The platform adapter owns those representations; C1 owns the
    // fact that the current tip is what gets painted.
    platform_.paint_tip(current_tip_text_);
}

void show_startup_tip_dialog(TipDialogPlatform& platform) {
    TipDialog dialog(platform);
    platform.show_modal(dialog);
}

}  // namespace creatures1::ui
