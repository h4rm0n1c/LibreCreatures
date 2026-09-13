#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../platform/com.hpp"

namespace creatures1::ui {

struct ToolbarRect {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

struct ToolbarFontSpec {
    std::int32_t height = 0;
    std::int32_t weight = 0;
    std::string_view family;
};

using BitmapHandle = std::uintptr_t;

// The adapter owns CToolBar/CComboBox/CFont, USER32 messages, GDI handles,
// and resource loading.  The MainToolbar owner keeps only C1's button and
// selector policy here.
class ToolbarPlatform {
public:
    virtual ~ToolbarPlatform() = default;

    virtual bool create_toolbar(std::uintptr_t parent_window,
                                std::uint32_t window_style,
                                std::uint32_t control_id) = 0;
    virtual bool load_toolbar_bitmap(std::uint32_t resource_id) = 0;
    virtual bool set_button_count(std::uint32_t count) = 0;
    virtual bool set_button(std::uint32_t index,
                            std::uint32_t command_id,
                            std::uint32_t style,
                            std::uint32_t image_index) = 0;
    virtual std::uint32_t button_command(std::uint32_t index) const = 0;
    virtual void remove_button(std::uint32_t index) = 0;
    virtual ToolbarRect item_rect(std::uint32_t index) const = 0;
    virtual bool create_creature_selector(const ToolbarRect& rect,
                                          std::uintptr_t parent_window,
                                          std::uint32_t control_id) = 0;
    virtual void populate_embedded_kit_menu_and_toolbar() = 0;
    virtual bool uses_system_gui_font() const = 0;
    virtual bool set_selector_font(const ToolbarFontSpec& font) = 0;
    virtual void use_default_selector_font() = 0;
    virtual void apply_selector_font() = 0;

    virtual bool file_exists(std::string_view path) const = 0;
    virtual BitmapHandle load_bitmap_file(std::string_view path,
                                          std::uint32_t width,
                                          std::uint32_t height) = 0;
    virtual std::int32_t add_bitmap_to_toolbar(BitmapHandle bitmap) = 0;
    virtual void destroy_bitmap(BitmapHandle bitmap) = 0;
};

class ToolbarArchiveApi {
public:
    virtual ~ToolbarArchiveApi() = default;

    virtual bool loading() const = 0;
    virtual std::int16_t read_selector_count() = 0;
    virtual void write_selector_count(std::int16_t count) = 0;
    virtual std::string read_string() = 0;
    virtual void write_string(std::string_view value) = 0;
};

class CreatureSelectorApi {
public:
    virtual ~CreatureSelectorApi() = default;

    virtual void clear() = 0;
    virtual std::size_t count() const = 0;
    virtual std::string text_at(std::size_t index) const = 0;
    virtual void add(std::string_view value) = 0;
};

class MainToolbar {
public:
    explicit MainToolbar(ToolbarPlatform& platform) : platform_(platform) {}

    bool create(std::uintptr_t parent_window);
    void serialize(ToolbarArchiveApi& archive,
                  CreatureSelectorApi& selector) const;

private:
    ToolbarPlatform& platform_;
};

// The archive record belongs to the semantic toolbar policy; it does not
// require a live HWND.  The Windows adapter uses this entry point while a
// document is being opened or saved, before/after the native controls are
// necessarily materialized.
void serialize_main_toolbar(ToolbarArchiveApi& archive,
                            CreatureSelectorApi& selector);

std::int32_t add_kit_toolbar_bitmap(ToolbarPlatform& platform,
                                    platform::ComLocalServerApi& com,
                                    std::string_view kit_prog_id);

} // namespace creatures1::ui
