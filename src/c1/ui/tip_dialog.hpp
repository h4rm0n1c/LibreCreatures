#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace creatures1::ui {

class TipFile {
public:
    virtual ~TipFile() = default;

    virtual bool read_line(std::string& line) = 0;
    virtual bool rewind() = 0;
    virtual bool seek(std::int32_t position) = 0;
    virtual std::int32_t position() const = 0;
    virtual std::string modification_timestamp() const = 0;
};

class TipDialog;

// MFC, USER32, registry, CRT file, and GDI operations are supplied here.
// TipDialog owns the tip-file policy and callback ordering, not their native
// object representations.
class TipDialogPlatform {
public:
    virtual ~TipDialogPlatform() = default;

    virtual bool read_registry_dword(std::string_view name,
                                     std::uint32_t& value) const = 0;
    virtual void write_registry_dword(std::string_view name,
                                      std::uint32_t value) = 0;
    virtual bool read_registry_string(std::string_view name,
                                      std::string& value) const = 0;
    virtual void write_registry_string(std::string_view name,
                                       std::string_view value) = 0;
    virtual std::string primary_resource_directory() const = 0;
    virtual std::unique_ptr<TipFile> open_tips_file(
        std::string_view path) const = 0;
    virtual std::string load_string(std::uint32_t resource_id) const = 0;
    virtual void show_resource_message(std::uint32_t resource_id) = 0;

    virtual void update_data(bool save_and_validate) = 0;
    virtual void forward_default_timer() = 0;
    virtual bool is_tip_control(std::uint32_t control_id) const = 0;
    virtual void use_tip_control_brush() = 0;
    virtual void forward_default_control_color() = 0;
    virtual void paint_tip(std::string_view text) = 0;
    virtual int show_modal(TipDialog& dialog) = 0;
    virtual void close_dialog() = 0;
};

class TipDialog {
public:
    explicit TipDialog(TipDialogPlatform& platform);
    ~TipDialog();

    void on_timer();
    void on_next_tip();
    void on_ctl_color(std::uint32_t control_id);
    void on_ok();
    void on_paint();

    // Reads the next non-comment tip, rewinding at EOF as the original does.
    bool read_next_tip_text();
    std::string_view current_tip_text() const { return current_tip_text_; }
    bool show_at_startup() const { return show_at_startup_; }
    void set_show_at_startup(bool enabled) { show_at_startup_ = enabled; }

private:
    TipDialogPlatform& platform_;
    std::unique_ptr<TipFile> tips_file_;
    std::string current_tip_text_;
    bool show_at_startup_ = true;
};

void show_startup_tip_dialog(TipDialogPlatform& platform);

}  // namespace creatures1::ui
