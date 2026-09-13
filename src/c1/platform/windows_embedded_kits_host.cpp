#include "windows_shell.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <array>

namespace creatures1::platform {

C1NativeEmbeddedKitMenuPlatform::C1NativeEmbeddedKitMenuPlatform(C1MainFrame& frame) : frame_(frame) {}


bool C1NativeEmbeddedKitMenuPlatform::tools_menu_exists() const {
    HMENU root = frame_.GetMenu() == nullptr
                     ? nullptr
                     : frame_.GetMenu()->GetSafeHmenu();
    if (root == nullptr) {
        return false;
    }

    CStringA tools_caption;
    tools_caption.LoadStringA(0xef28);
    const int menu_count = ::GetMenuItemCount(root);
    for (int index = 0; index < menu_count; ++index) {
        char item_text[256]{};
        ::GetMenuStringA(root, static_cast<UINT>(index), item_text,
                         static_cast<int>(sizeof(item_text)),
                         MF_BYPOSITION);
        if (std::strcmp(item_text, tools_caption.GetString()) == 0) {
            tools_menu_ = ::GetSubMenu(root, index);
            return tools_menu_ != nullptr;
        }
    }
    return false;
}

bool C1NativeEmbeddedKitMenuPlatform::read_tool_registry_value(std::size_t tool_index, std::string& value) const {
    if (tool_index >= creatures1::application::kEmbeddedKitSlotCount) {
        return false;
    }

    HKEY registry_key = nullptr;
    if (!open_c1_secondary_registry(registry_key, KEY_READ)) {
        return false;
    }

    char value_name[32]{};
    std::snprintf(value_name, sizeof(value_name), "Tool%u",
                  static_cast<unsigned>(tool_index));
    std::array<char, 0xb4> encoded{};
    DWORD type = 0;
    DWORD byte_count = static_cast<DWORD>(encoded.size());
    const LSTATUS status = RegQueryValueExA(
        registry_key, value_name, nullptr, &type,
        reinterpret_cast<LPBYTE>(encoded.data()), &byte_count);
    RegCloseKey(registry_key);
    // PopulateEmbeddedKitMenuAndToolbarFromRegistry @ 0x004440e0 reads the
    // value type into a local and then tests only the query status -- it never
    // looks at the type.  That matters: the CAOS `tool` command writes these
    // as REG_BINARY (type 3), but a real installation's registration script
    // writes them as REG_SZ, and both must load.  Requiring REG_BINARY here
    // meant the Tools menu came up empty against a genuine C1 install.
    static_cast<void>(type);
    if (status != ERROR_SUCCESS || byte_count == 0 ||
        byte_count > encoded.size()) {
        return false;
    }

    value.assign(encoded.data(), byte_count);
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return !value.empty();
}

void C1NativeEmbeddedKitMenuPlatform::publish_tool_definition( std::size_t tool_index, const creatures1::application::EmbeddedKitToolDefinition& definition, std::uint8_t adjusted_type_code) {
    if (tool_index >= frame_.embedded_kit_definitions().size()) {
        return;
    }
    creatures1::application::EmbeddedKitToolDefinition published =
        definition;
    published.adjusted_type_code = adjusted_type_code;
    frame_.publish_embedded_kit_definition(tool_index, published,
                                            adjusted_type_code);
}

int C1NativeEmbeddedKitMenuPlatform::add_toolbar_bitmap(std::string_view /*prog_id*/) {
    // AddKitToolbarBitmapFromProgId is a native kit/icon resource
    // lookup.  The recovered fallback is the type-code image already in
    // the stock toolbar bitmap; keep that fallback until the external
    // kit icon ownership boundary is bound.
    return -1;
}

void C1NativeEmbeddedKitMenuPlatform::set_toolbar_button(std::size_t tool_index, std::uint32_t command_id, int image_index) {
    frame_.set_embedded_kit_toolbar_button(tool_index, command_id,
                                            image_index);
}

void C1NativeEmbeddedKitMenuPlatform::append_tool_menu_item(std::uint32_t command_id, std::string_view display_name) {
    if (tools_menu_ == nullptr) {
        return;
    }
    ::AppendMenuA(tools_menu_, MF_STRING, static_cast<UINT>(command_id),
                  std::string(display_name).c_str());
}

void C1NativeEmbeddedKitMenuPlatform::invalidate_toolbar_and_menu() {
    frame_.invalidate_main_toolbar();
    frame_.DrawMenuBar();
}

namespace {

// The registry side of `tool`.  PopulateEmbeddedKitMenuAndToolbarFromRegistry
// @ 0x004440e0 reads each Tool<n> value as a '|'-delimited record of prog id,
// display name, description and a type byte, and the menu loader adds its own
// +6 to that byte -- so the fourth field is stored exactly as the CAOS rvalue
// supplied it.  Macro's own writer at 0x0041f4?? fills three fixed-width stack
// buffers instead and writes 0xb4 bytes from the first, which its own reader
// cannot parse back; the delimited form is the one both readers agree on, so
// that is what is written here.
class WindowsEmbeddedKitRegistration final
    : public creatures1::application::EmbeddedKitRegistrationApi {
public:
    explicit WindowsEmbeddedKitRegistration(C1MainFrame* frame)
        : frame_(frame) {}

    bool read_tool_count(std::size_t& count) const override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_READ)) {
            return false;
        }
        DWORD value = 0;
        DWORD type = 0;
        DWORD size = sizeof(value);
        const LSTATUS status =
            RegQueryValueExA(key, "NumTools", nullptr, &type,
                             reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS || type != REG_DWORD) {
            return false;
        }
        count = static_cast<std::size_t>(value);
        return true;
    }

    bool tool_name_exists(std::string_view registry_name) const override {
        std::size_t count = 0;
        if (!read_tool_count(count)) {
            return false;
        }
        C1NativeEmbeddedKitMenuPlatform reader(*frame_);
        for (std::size_t index = 0;
             index < count &&
             index < creatures1::application::kEmbeddedKitSlotCount;
             ++index) {
            std::string record;
            if (!reader.read_tool_registry_value(index, record)) {
                continue;
            }
            const std::string_view stored(record);
            const std::size_t end = stored.find('|');
            if (stored.substr(0, end) == registry_name) {
                return true;
            }
        }
        return false;
    }

    void write_tool_record(
        std::size_t tool_index,
        const creatures1::application::EmbeddedKitToolRegistration&
            registration) override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_SET_VALUE)) {
            return;
        }
        std::string record(registration.registry_name);
        record += '|';
        record += registration.display_name;
        record += '|';
        record += registration.launch_command;
        record += '|';
        record += static_cast<char>(registration.raw_type_code);
        std::array<char, 0xb4> stored{};
        const std::size_t copied = (std::min)(record.size(), stored.size() - 1);
        std::memcpy(stored.data(), record.data(), copied);
        char value_name[32]{};
        std::snprintf(value_name, sizeof(value_name), "Tool%u",
                      static_cast<unsigned>(tool_index));
        RegSetValueExA(key, value_name, 0, REG_BINARY,
                       reinterpret_cast<const BYTE*>(stored.data()),
                       static_cast<DWORD>(stored.size()));
        RegCloseKey(key);
    }

    void write_tool_count(std::size_t count) override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_SET_VALUE)) {
            return;
        }
        const DWORD value = static_cast<DWORD>(count);
        RegSetValueExA(key, "NumTools", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }

    void rebuild_tool_menu_and_toolbar() override {
        if (frame_ != nullptr) {
            frame_->populate_embedded_kit_menu_and_toolbar();
        }
    }

    void set_tool_available(std::string_view registry_name,
                            bool is_available) override {
        // Availability is the DDE conversation's flag, and that surface has
        // no platform owner yet, so there is nowhere truthful to record it.
        static_cast<void>(registry_name);
        static_cast<void>(is_available);
    }

private:
    C1MainFrame* frame_;
};

} // namespace

bool register_embedded_kit_tool_for_frame(
    C1MainFrame* frame,
    const creatures1::application::EmbeddedKitToolRegistration& registration) {
    WindowsEmbeddedKitRegistration api(frame);
    return creatures1::application::register_embedded_kit_tool(api,
                                                               registration);
}

} // namespace creatures1::platform
