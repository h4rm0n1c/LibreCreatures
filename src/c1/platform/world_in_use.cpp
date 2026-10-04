#include "world_in_use.hpp"

#include "windows_error_dialog.hpp"

#include <windows.h>

#include <cctype>
#include <cstdint>
#include <cstdio>

namespace creatures1::platform {
namespace {

HANDLE g_claimed_world = nullptr;
std::string g_claimed_name;

// One name per world file, however the path was spelled: the full path,
// lower case, with backslashes, hashed (a mutex name cannot hold one).
std::string world_mutex_name(const std::string& world_file) {
    char full[MAX_PATH * 2] = {};
    const DWORD length =
        ::GetFullPathNameA(world_file.c_str(), sizeof(full), full, nullptr);
    std::string path = length == 0 || length >= sizeof(full) ? world_file
                                                             : std::string(full);
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (char character : path) {
        if (character == '/') {
            character = '\\';
        }
        hash ^= static_cast<unsigned char>(
            std::tolower(static_cast<unsigned char>(character)));
        hash *= 0x100000001b3ULL;
    }
    char name[64] = {};
    std::snprintf(name, sizeof(name), "Local\\LibreCreatures.World.%016llx",
                  static_cast<unsigned long long>(hash));
    return name;
}

} // namespace

bool claim_world(const std::string& world_file) {
    const std::string name = world_mutex_name(world_file);
    if (g_claimed_world != nullptr && name == g_claimed_name) {
        return true;
    }
    HANDLE mutex = ::CreateMutexA(nullptr, FALSE, name.c_str());
    if (mutex == nullptr) {
        return true;
    }
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(mutex);
        return false;
    }
    if (g_claimed_world != nullptr) {
        ::CloseHandle(g_claimed_world);
    }
    g_claimed_world = mutex;
    g_claimed_name = name;
    return true;
}

void report_world_in_use(const std::string& world_file) {
    show_error_report(
        nullptr, "Creatures",
        "This world is already open in another copy of Creatures.",
        "World: " + world_file +
            "\n\nTwo copies running one world would save over each other, and "
            "one can stop the other saving at all. Close the other copy of "
            "Creatures and try again.\n\nIf you cannot see another copy, it "
            "may have stopped responding: end Creatures.exe in Task Manager, "
            "then start the game again.");
}

} // namespace creatures1::platform
