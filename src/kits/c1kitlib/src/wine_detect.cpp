// Whether this process is running under Wine: the same check the game
// uses (windows_embedded_kit_host.cpp's WineVersionExportProbe) so both
// sides agree.

#include "c1kit/c1kit.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace c1kit {

bool running_under_wine() {
    const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll != nullptr &&
           GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

} // namespace c1kit
