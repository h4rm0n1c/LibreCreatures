#pragma once

// c1kitlib.dll: the data side shared by every C1 kit.  No UI, graphics or
// sound live here; those belong to the kit shell.
//
// The DLL boundary carries only abstract interfaces, factory functions and
// plain C types, so a kit and the DLL never share STL or MFC object layouts.
// Protocol values and reply parsing are header-only (c1kit/protocol.hpp).

#include <cstddef>
#include <cstdint>

#ifdef C1KITLIB_BUILD
#define C1KIT_API __declspec(dllexport)
#else
#define C1KIT_API __declspec(dllimport)
#endif

namespace c1kit {

// ---------------------------------------------------------------------------
// Kit -> game: the SFC.OLE macro conversation
// ---------------------------------------------------------------------------

// One primitive per SFC.OLE method.  Every call returns false when the
// automation call failed or the game answered FALSE.
class MacroTransport {
public:
    virtual ~MacroTransport() = default;

    // CreateMacro: request { VT_I2 mode } -> response { VT_I4 handle }.
    virtual bool create_macro(short mode, long& handle) = 0;
    // DestroyMacro: request { VT_I4 handle }.
    virtual bool destroy_macro(long handle) = 0;
    // LoadMacro: copies the script into the command buffer and sends it.
    virtual bool load_macro(long handle, const char* script) = 0;
    // ExecuteMacro: load + run (+ reply) in one call.
    virtual bool execute_macro(long handle, const char* script) = 0;
    // RequestMacro: runs the loaded script.  On success the reply (the
    // game's replacement BSTR) is readable through reply()/reply_length()
    // until the next call.
    virtual bool request_macro(long handle) = 0;
    virtual const char* reply() const = 0;
    virtual std::size_t reply_length() const = 0;

    virtual void release() = 0;
};

enum class ConnectResult {
    connected,
    not_registered,   // CLSIDFromProgID("SFC.OLE") failed
    create_failed,    // CoCreateInstance failed; see hresult
    run_failed,       // OleRun failed; see hresult
    no_dispatch,      // the object has no IDispatch; see hresult
};

// Connects to the running game's SFC.OLE automation object in the same steps
// as MFC's COleDispatchDriver::CreateDispatch, which the 1996 kits used:
// CoCreateInstance for IUnknown with CLSCTX_ALL, OleRun, then IDispatch.
// (Under Wine this reaches OLEKitProxy's in-process forwarder.)  `buffer_bytes` is the
// size of the byte-length BSTR command buffer (Observation 0x400, Science
// Kit 0x1000).  Returns nullptr on failure.
C1KIT_API MacroTransport* connect_sfc_ole(std::size_t buffer_bytes,
                                          ConnectResult* result,
                                          long* hresult);

// Writes the text a kit shows when connect_sfc_ole fails: the system's
// message for `hresult` (the 1996 kits' CException::ReportError), else "Can
// not communicate with application", followed by the failing step and the
// code.  Always NUL-terminates a non-empty buffer.
C1KIT_API void describe_connect_failure(ConnectResult result, long hresult,
                                        char* buffer, std::size_t size);

// ---------------------------------------------------------------------------
// Game -> kit: the kit's own OLE server and its Communicate method
// ---------------------------------------------------------------------------

// Implemented by the shell.  Called on the thread that pumps the kit's
// messages (the Wine proxy marshals onto the kit's main thread).
class KitEvents {
public:
    virtual ~KitEvents() = default;
    // Raw Communicate(header, payload); decode with c1kit::decode_communicate.
    // The return value is the kit's VT_BOOL answer.
    virtual bool on_communicate(std::int32_t header, std::int32_t payload) = 0;
    // The last external reference to the kit object went away.
    virtual void on_last_client_released() {}
};

struct KitIdentity {
    const char* prog_id = nullptr;         // e.g. "OVERVIEW.OLE"
    unsigned char clsid[16] = {};          // GUID bytes, little-endian fields
};

class KitServer {
public:
    virtual ~KitServer() = default;
    // CoRegisterClassObject(REGCLS_MULTIPLEUSE).  OLEKitProxy hooks this
    // call to find the kit, so it must happen in the kit's main thread.
    virtual bool register_class_object(long* hresult) = 0;
    virtual void revoke_class_object() = 0;
    virtual void release() = 0;
};

C1KIT_API KitServer* create_kit_server(const KitIdentity& identity,
                                       KitEvents& events);

// What COleObjectFactory::UpdateRegistryAll does for a kit run without
// /Embedding: HKCR\<ProgID>\CLSID, HKCR\CLSID\{clsid}\ProgID and
// HKCR\CLSID\{clsid}\LocalServer32 = "<exe_path>".  Leaves any existing
// InprocServer32/InprocHandler32 keys alone.
C1KIT_API bool register_local_server(const KitIdentity& identity,
                                     const char* exe_path);

// Writes HKCU\Software\Gameware Development\Creatures 1\1.0\Tool<slot> as
// REG_SZ "<value_prog_id>|<name>|<help>|<slot>", as each 1996 kit does when
// run without /Embedding.
C1KIT_API bool write_tool_registration(int slot, const char* value_prog_id,
                                       const char* name, const char* help);

// ---------------------------------------------------------------------------
// Settings: the kits' CRegistryHandler
// ---------------------------------------------------------------------------

enum class SettingsScope {
    user,     // HKCU\SOFTWARE\<company>\<product>\<version>, read/write
    machine,  // HKLM\SOFTWARE\<company>\<product>\<version>, read-only
};

class KitSettings {
public:
    virtual ~KitSettings() = default;
    virtual bool is_open() const = 0;
    virtual bool read_dword(SettingsScope scope, const char* name,
                            std::uint32_t& value) const = 0;
    virtual bool read_binary(SettingsScope scope, const char* name,
                             void* buffer, std::size_t size) const = 0;
    // Reads a string into `buffer` (always NUL-terminated on success).
    virtual bool read_string(SettingsScope scope, const char* name,
                             char* buffer, std::size_t size) const = 0;
    virtual bool write_dword(const char* name, std::uint32_t value) = 0;
    virtual bool write_binary(const char* name, const void* data,
                              std::size_t size) = 0;
    virtual bool write_string(const char* name, const char* value) = 0;
    virtual void release() = 0;
};

enum class SettingsOpenPolicy {
    // The 1996 CRegistryHandler (Observation @ 0x00404460): the handler is
    // open only if the HKCU key is created AND the HKLM key opens.
    require_both_keys,
    // Open whenever the HKCU key can be created; HKLM reads just fail.
    user_key_only,
};

C1KIT_API KitSettings* open_kit_settings(const char* company,
                                         const char* product,
                                         const char* version,
                                         SettingsOpenPolicy policy);

// A directory the game records under SOFTWARE\Gameware Development\
// Creatures 1\1.0 ("Main Directory", "Palette Directory", ...).  The
// machine-wide key describes the installation; the per-user key follows the
// world being played (the launcher and the game switch it with the world).
// `prefer_world` reads the per-user key first; otherwise the machine key
// first.  Writes the path, NUL-terminated, and returns false when neither
// key has the value.
enum class GameDirectory {
    installation,  // art and data shipped with the game
    world,         // files that belong to the world being played
};
C1KIT_API bool read_game_directory(const char* value_name, GameDirectory which,
                                   char* buffer, std::size_t size);

} // namespace c1kit
