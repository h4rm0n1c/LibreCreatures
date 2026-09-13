#include "windows_registry_host.hpp"

#include <vector>

namespace creatures1::platform {

const char kC1UserRegistryPath[] =
    "Software\\Gameware Development\\Creatures 1\\1.0";
const char kC1MachineRegistryPath[] =
    "SOFTWARE\\Gameware Development\\Creatures 1\\1.0";

bool read_registry_string(HKEY key, const char* value_name,
                          std::string& value) {
    DWORD type = 0;
    DWORD byte_count = 0;
    if (RegQueryValueExA(key, value_name, nullptr, &type, nullptr,
                         &byte_count) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return false;
    }

    std::vector<char> buffer(byte_count == 0 ? 1 : byte_count + 1, '\0');
    DWORD returned_bytes = byte_count;
    if (RegQueryValueExA(key, value_name, nullptr, &type,
                         reinterpret_cast<LPBYTE>(buffer.data()),
                         &returned_bytes) != ERROR_SUCCESS) {
        return false;
    }
    buffer.back() = '\0';
    value.assign(buffer.data());
    return true;
}


void write_empty_registry_string(HKEY key, const char* value_name) {
    static constexpr char empty[] = "";
    RegSetValueExA(key, value_name, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(empty), sizeof(empty));
}


bool open_c1_secondary_registry(HKEY& key, REGSAM access) {
    return RegCreateKeyExA(HKEY_CURRENT_USER, kC1UserRegistryPath, 0,
                           nullptr, 0, access, nullptr, &key, nullptr) ==
           ERROR_SUCCESS;
}


bool read_c1_window_rect(HKEY key, RECT& rect) {
    DWORD type = 0;
    DWORD byte_count = sizeof(rect);
    return RegQueryValueExA(
               key, "WindowPosn", nullptr, &type,
               reinterpret_cast<LPBYTE>(&rect), &byte_count) == ERROR_SUCCESS &&
           type == REG_BINARY && byte_count == sizeof(rect);
}


void write_c1_window_rect(HKEY key, const RECT& rect) {
    RegSetValueExA(key, "WindowPosn", 0, REG_BINARY,
                   reinterpret_cast<const BYTE*>(&rect), sizeof(rect));
}


bool open_c1_primary_registry(HKEY& key) {
    return RegOpenKeyExA(HKEY_LOCAL_MACHINE, kC1MachineRegistryPath, 0,
                         KEY_READ, &key) == ERROR_SUCCESS;
}


bool read_registry_dword(HKEY key, const char* value_name,
                         std::uint32_t& value) {
    DWORD type = 0;
    DWORD byte_count = sizeof(value);
    return RegQueryValueExA(key, value_name, nullptr, &type,
                            reinterpret_cast<LPBYTE>(&value), &byte_count) ==
               ERROR_SUCCESS &&
           byte_count == sizeof(value);
}


void write_registry_dword(HKEY key, const char* value_name,
                          std::uint32_t value) {
    RegSetValueExA(key, value_name, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));
}


} // namespace creatures1::platform
