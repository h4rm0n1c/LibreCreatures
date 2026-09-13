#pragma once

#include "windows_prelude.hpp"

#include <cstdint>
#include <string>

#include "../application/application.hpp"
#include "windows_app_state.hpp"
#include "windows_environment_host.hpp"
#include "windows_registry_host.hpp"

#include <shellapi.h>
#include <afxwin.h>

namespace creatures1::platform {

// Concrete Win32 settings persistence behind application::SfcAppSettingsHost.
class C1SettingsHost final
    : public creatures1::application::SfcAppSettingsHost {
public:
    void publish_sfc_app_instance(
        creatures1::application::SfcAppState& state) override {
        published_state_ = &state;
    }

    bool query_burble_enabled(bool& enabled) const override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_READ)) {
            return false;
        }
        std::uint32_t value = 0;
        const bool present = read_registry_dword(key, "Burble", value);
        RegCloseKey(key);
        if (present) {
            enabled = value != 0;
        }
        return present;
    }

    void write_burble_enabled(bool enabled) override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_SET_VALUE)) {
            return;
        }
        write_registry_dword(key, "Burble", enabled ? 1u : 0u);
        RegCloseKey(key);
    }

    bool query_autosave_interval_ms(std::uint32_t& interval_ms) const override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_READ)) {
            return false;
        }
        const bool present = read_registry_dword(
            key, "Autosave Interval", interval_ms);
        RegCloseKey(key);
        return present;
    }

    bool query_privileges(std::string& privilege_name) const override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_READ)) {
            return false;
        }
        const bool present = read_registry_string(key, "Privileges",
                                                  privilege_name);
        RegCloseKey(key);
        return present;
    }

    void write_default_privileges(std::string_view privilege_name) override {
        HKEY key = nullptr;
        if (!open_c1_secondary_registry(key, KEY_SET_VALUE)) {
            return;
        }
        const std::string value(privilege_name);
        RegSetValueExA(key, "Privileges", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(value.c_str()),
                       static_cast<DWORD>(value.size() + 1));
        RegCloseKey(key);
    }

    bool is_user_privilege(std::string_view privilege_name) const override {
        return equals_ignore_case(privilege_name, "doctor");
    }

    bool is_elevated_privilege(
        std::string_view privilege_name) const override {
        return equals_ignore_case(privilege_name, "skirty");
    }

private:
    static bool equals_ignore_case(std::string_view left,
                                   std::string_view right) {
        return left.size() == right.size() &&
               std::equal(left.begin(), left.end(), right.begin(),
                          [](char lhs, char rhs) {
                              return std::tolower(static_cast<unsigned char>(lhs)) ==
                                     std::tolower(static_cast<unsigned char>(rhs));
                          });
    }

    creatures1::application::SfcAppState* published_state_ = nullptr;
};

// Concrete Win32 shell launch behind application::WebpageShortcutHost.
class C1NativeWebpageShortcut final
    : public creatures1::application::WebpageShortcutHost {
public:
    std::string primary_resource_directory() const override {
        if (g_active_primary_directories != nullptr &&
            !g_active_primary_directories->paths[0].empty()) {
            return g_active_primary_directories->paths[0];
        }
        return current_directory_with_separator();
    }

    bool is_regular_file(std::string_view path) const override {
        const std::string native_path(path);
        const DWORD attributes = GetFileAttributesA(native_path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    void show_missing_webpage_error() override {
        AfxMessageBox("The webpage shortcut file could not be found.",
                      MB_ICONWARNING, 0);
    }

    void launch_webpage_shortcut(std::string_view shortcut_name,
                                 std::string_view working_directory) override {
        const std::string shortcut_path =
            std::string(working_directory) + "\\" + std::string(shortcut_name);
        ShellExecuteA(
            AfxGetMainWnd() == nullptr ? nullptr : AfxGetMainWnd()->GetSafeHwnd(),
            "open", shortcut_path.c_str(), nullptr,
            std::string(working_directory).c_str(), SW_SHOWNORMAL);
    }
};

} // namespace creatures1::platform
