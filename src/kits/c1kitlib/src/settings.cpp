// The kits' CRegistryHandler (Observation @ 0x00404310..0x00404ba0).
//
// Path: SOFTWARE\<company>\<product>\<version>.  The user key is created
// under HKEY_CURRENT_USER with KEY_ALL_ACCESS; the machine key is opened
// under HKEY_LOCAL_MACHINE with KEY_READ.  (The Ghidra comment on
// OpenRegistryKeyFromComponents names the hives the other way round; the
// pushed constants are 0x80000001 and 0x80000002.)

#include "c1kit/c1kit.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace c1kit {
namespace {

class RegistrySettings final : public KitSettings {
public:
    RegistrySettings(const char* company, const char* product,
                     const char* version, SettingsOpenPolicy policy) {
        char path[256];
        std::snprintf(path, sizeof(path), "SOFTWARE\\%s\\%s\\%s", company,
                      product, version);
        DWORD disposition = 0;
        const bool user_ok =
            RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, nullptr, 0,
                            KEY_ALL_ACCESS, nullptr, &user_, &disposition) ==
            ERROR_SUCCESS;
        const bool machine_ok =
            RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &machine_) ==
            ERROR_SUCCESS;
        if (!user_ok) {
            user_ = nullptr;
        }
        if (!machine_ok) {
            machine_ = nullptr;
        }
        open_ = policy == SettingsOpenPolicy::require_both_keys
                    ? (user_ok && machine_ok)
                    : user_ok;
    }

    ~RegistrySettings() override {
        if (user_ != nullptr) {
            RegCloseKey(user_);
        }
        if (machine_ != nullptr) {
            RegCloseKey(machine_);
        }
    }

    bool is_open() const override { return open_; }

    bool read_dword(SettingsScope scope, const char* name,
                    std::uint32_t& value) const override {
        DWORD raw = 0;
        if (!query(scope, name, &raw, sizeof(raw))) {
            return false;
        }
        value = raw;
        return true;
    }

    bool read_binary(SettingsScope scope, const char* name, void* buffer,
                     std::size_t size) const override {
        return query(scope, name, buffer, size);
    }

    bool read_string(SettingsScope scope, const char* name, char* buffer,
                     std::size_t size) const override {
        if (buffer == nullptr || size == 0) {
            return false;
        }
        if (!query(scope, name, buffer, size)) {
            return false;
        }
        buffer[size - 1] = '\0';
        return true;
    }

    bool write_dword(const char* name, std::uint32_t value) override {
        const DWORD raw = value;
        return set(name, REG_DWORD, &raw, sizeof(raw));
    }

    bool write_binary(const char* name, const void* data,
                      std::size_t size) override {
        return set(name, REG_BINARY, data, size);
    }

    bool write_string(const char* name, const char* value) override {
        if (value == nullptr) {
            return false;
        }
        return set(name, REG_SZ, value, std::strlen(value) + 1);
    }

    void release() override { delete this; }

private:
    bool query(SettingsScope scope, const char* name, void* buffer,
               std::size_t size) const {
        if (!open_) {
            return false;
        }
        const HKEY key = scope == SettingsScope::user ? user_ : machine_;
        if (key == nullptr) {
            return false;
        }
        DWORD type = 0;
        DWORD bytes = static_cast<DWORD>(size);
        return RegQueryValueExA(key, name, nullptr, &type,
                                static_cast<BYTE*>(buffer), &bytes) ==
               ERROR_SUCCESS;
    }

    bool set(const char* name, DWORD type, const void* data,
             std::size_t size) {
        if (!open_ || user_ == nullptr) {
            return false;
        }
        return RegSetValueExA(user_, name, 0, type,
                              static_cast<const BYTE*>(data),
                              static_cast<DWORD>(size)) == ERROR_SUCCESS;
    }

    HKEY user_ = nullptr;
    HKEY machine_ = nullptr;
    bool open_ = false;
};

} // namespace

KitSettings* open_kit_settings(const char* company, const char* product,
                               const char* version,
                               SettingsOpenPolicy policy) {
    if (company == nullptr || product == nullptr || version == nullptr) {
        return nullptr;
    }
    return new RegistrySettings(company, product, version, policy);
}

bool read_game_directory(const char* value_name, GameDirectory which,
                         char* buffer, std::size_t size) {
    if (value_name == nullptr || buffer == nullptr || size == 0) {
        return false;
    }
    buffer[0] = '\0';
    KitSettings* settings = open_kit_settings(
        "Gameware Development", "Creatures 1", "1.0",
        SettingsOpenPolicy::user_key_only);
    if (settings == nullptr) {
        return false;
    }
    const SettingsScope first = which == GameDirectory::world
                                    ? SettingsScope::user
                                    : SettingsScope::machine;
    const SettingsScope second = which == GameDirectory::world
                                     ? SettingsScope::machine
                                     : SettingsScope::user;
    const bool found = settings->read_string(first, value_name, buffer, size) ||
                       settings->read_string(second, value_name, buffer, size);
    settings->release();
    if (!found) {
        buffer[0] = '\0';
    }
    return found && buffer[0] != '\0';
}

// Writes Tool<slot> as "%s|%s|%s|%d" (Observation
// InitializeOverviewOleRegistration @ 0x00401f10).  The game reads it from
// HKCU.  The 1996 kits opened the key through a handler that also required
// the HKLM key, so without a machine-wide install they never appeared in the
// Tools menu; only the user key is needed here.
bool write_tool_registration(int slot, const char* value_prog_id,
                             const char* name, const char* help) {
    KitSettings* settings =
        open_kit_settings("Gameware Development", "Creatures 1", "1.0",
                          SettingsOpenPolicy::user_key_only);
    if (settings == nullptr) {
        return false;
    }
    char value_name[32];
    char value[256];
    std::snprintf(value_name, sizeof(value_name), "Tool%d", slot);
    // The game reads each Tool<N> value into a 64-byte buffer
    // (PopulateEmbeddedKitMenuAndToolbarFromRegistry @ 0x004440e0); a longer
    // value fails to read, and the slot gets no menu item and no record --
    // its command then fails with "Cannot find executable path for kit."
    // Shorten the help text until the value fits, NUL included.
    constexpr std::size_t kGameValueBytes = 0x40;
    char suffix[16];
    std::snprintf(suffix, sizeof(suffix), "|%d", slot);
    const std::string head = std::string(value_prog_id == nullptr ? "" : value_prog_id) +
                             "|" + (name == nullptr ? "" : name) + "|";
    std::string help_text = help == nullptr ? "" : help;
    const std::size_t fixed = head.size() + std::strlen(suffix);
    if (fixed + help_text.size() >= kGameValueBytes) {
        help_text.resize(fixed < kGameValueBytes - 1 ? kGameValueBytes - 1 - fixed : 0);
    }
    std::snprintf(value, sizeof(value), "%s%s%s", head.c_str(), help_text.c_str(),
                  suffix);
    const bool ok = settings->is_open() && settings->write_string(value_name, value);
    // The menu reads all twenty Tool<N> values whatever NumTools says; the
    // count is what CAOS tool registration appends after.  A kit in a slot
    // past the ten 1996 kits (the Ecology Kit, slot 10) raises it so a later
    // registration does not take the same slot.
    std::uint32_t tool_count = 0;
    // Only raised, never made: with no count yet, the game's own default
    // stands.
    if (ok && slot >= 0 &&
        settings->read_dword(SettingsScope::user, "NumTools", tool_count) &&
        tool_count < static_cast<std::uint32_t>(slot) + 1) {
        settings->write_dword("NumTools", static_cast<std::uint32_t>(slot) + 1);
    }
    settings->release();
    return ok;
}

} // namespace c1kit

namespace c1kit {
namespace {

bool set_default_value(HKEY root, const char* path, const char* value) {
    HKEY key = nullptr;
    if (RegCreateKeyExA(root, path, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LONG status = RegSetValueExA(
        key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
        static_cast<DWORD>(std::strlen(value) + 1));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

} // namespace

bool register_local_server(const KitIdentity& identity, const char* exe_path) {
    if (identity.prog_id == nullptr || exe_path == nullptr) {
        return false;
    }
    GUID guid;
    std::memcpy(&guid, identity.clsid, sizeof(guid));
    char clsid[64];
    std::snprintf(clsid, sizeof(clsid),
                  "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                  static_cast<unsigned long>(guid.Data1), guid.Data2,
                  guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2],
                  guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6],
                  guid.Data4[7]);
    // Twice: per user, under HKCU\Software\Classes, which needs no
    // administrator and which Windows merges into the HKEY_CLASSES_ROOT view
    // COM reads (before a machine entry, such as a 1996 kit's); and under
    // HKEY_CLASSES_ROOT itself, which Wine's COM and the game's own lookup
    // there read.  On Windows a new key (the Ecology Kit's) under
    // HKEY_CLASSES_ROOT goes to HKLM, which only an administrator can write,
    // and the game then reported "Invalid class string".  Either is enough.
    const std::string prog_id = identity.prog_id;
    const std::string class_key = std::string("CLSID\\") + clsid;
    const auto register_under = [&](HKEY root, const std::string& prefix) {
        bool written = set_default_value(root, (prefix + prog_id).c_str(), identity.prog_id);
        written = set_default_value(root, (prefix + prog_id + "\\CLSID").c_str(), clsid) && written;
        written = set_default_value(root, (prefix + class_key).c_str(), identity.prog_id) && written;
        written = set_default_value(root, (prefix + class_key + "\\ProgID").c_str(),
                                    identity.prog_id) && written;
        written = set_default_value(root, (prefix + class_key + "\\LocalServer32").c_str(),
                                    exe_path) && written;
        return written;
    };
    const bool per_user = register_under(HKEY_CURRENT_USER, "Software\\Classes\\");
    const bool shared = register_under(HKEY_CLASSES_ROOT, "");
    const bool ok = per_user || shared;
    return ok;
}

} // namespace c1kit
