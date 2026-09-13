#include "windows_com_host.hpp"

#include <array>

namespace creatures1::platform {

bool WindowsComLocalServer::resolve_local_server_path(
    std::string_view prog_id, std::string& server_path) {
    // Recovered ResolveComProgIdLocalServerPath @ 00443e20.
    server_path.clear();

    const std::string prog_id_text(prog_id);
    std::array<wchar_t, 0x104> wide_prog_id{};
    MultiByteToWideChar(CP_ACP, 0, prog_id_text.c_str(), -1,
                        wide_prog_id.data(),
                        static_cast<int>(wide_prog_id.size()));

    CLSID clsid{};
    if (FAILED(CLSIDFromProgID(wide_prog_id.data(), &clsid))) {
        return false;
    }

    LPOLESTR clsid_string = nullptr;
    if (FAILED(StringFromCLSID(clsid, &clsid_string))) {
        return false;
    }
    std::array<char, 0x40> clsid_text{};
    WideCharToMultiByte(CP_ACP, 0, clsid_string, -1, clsid_text.data(),
                        static_cast<int>(clsid_text.size()), nullptr, nullptr);
    CoTaskMemFree(clsid_string);

    std::array<char, 0x100> registry_path{};
    wsprintfA(registry_path.data(), "CLSID\\%s\\LocalServer32",
              clsid_text.data());

    HKEY local_server_key = nullptr;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, registry_path.data(), 0, KEY_READ,
                      &local_server_key) != ERROR_SUCCESS) {
        return false;
    }
    std::array<char, 0x104> value{};
    DWORD value_type = 0;
    DWORD value_size = static_cast<DWORD>(value.size());
    RegQueryValueExA(local_server_key, nullptr, nullptr, &value_type,
                     reinterpret_cast<LPBYTE>(value.data()), &value_size);
    RegCloseKey(local_server_key);
    if (value[0] == '\0') {
        return false;
    }

    // A quoted path ends at its closing quote; an unquoted one ends at the
    // first space that introduces a /switch or -switch, so paths containing
    // spaces survive.
    char* start = value.data();
    if (*start == '"') {
        start += 1;
        if (char* closing = strchr(start, '"')) {
            *closing = '\0';
        }
    } else {
        for (char* space = strchr(start, ' '); space != nullptr;
             space = strchr(space + 1, ' ')) {
            if (space[1] == '/' || space[1] == '-') {
                *space = '\0';
                break;
            }
        }
    }

    server_path.assign(start);
    return true;
}

} // namespace creatures1::platform
