#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../scripting/macro_holder.hpp"

namespace creatures1::application {

// These are the small records crossing the automation boundary.  Their
// storage is supplied by OLE; the game owns only the fields used by the
// recovered CSfcOLE methods.
enum class OleVariantType : std::uint16_t {
    bstr = 8,
};

// A VARIANT: the type occupies the first two bytes and the value sits at
// offset eight.  CSfcOLE::LoadMacro @ 0x0042fb60 tests that type for 8 and
// then reads the value as `*(char **)`, so despite the VT_BSTR tag the
// pointer is an ANSI buffer -- the kit hands over its own command buffer and
// the game writes the reply back into that same storage.
struct OleScriptVariant {
    OleVariantType type = OleVariantType::bstr;
    std::uint16_t reserved_flags = 0;
    std::uint32_t reserved_value = 0;
    char* bstr_value = nullptr;
    std::uint32_t reserved_tail = 0;
};

enum class MacroRequestKind : std::uint32_t {
    create = 2,
};

enum class MacroResponseKind : std::uint32_t {
    created = 3,
};

struct MacroCreateRequest {
    MacroRequestKind kind = MacroRequestKind::create;
    std::uint16_t execution_mode = 0;
};

struct MacroCreateResponse {
    MacroResponseKind kind = MacroResponseKind::created;
    scripting::MacroHolder* holder = nullptr;
};

// The concrete MFC/OLE implementation remains a platform adapter.  The
// recovered source policy below sees typed operations instead of CObList,
// CString, BSTR, CCmdTarget, or compiler EH records.
// Virtual so a platform host can supply the MacroHolderHost half from the
// existing macro host rather than forwarding every method to it.
class CSfcOLEHost : public virtual scripting::MacroHolderHost {
public:
    ~CSfcOLEHost() override = default;

    virtual std::unique_ptr<scripting::MacroHolder> create_macro_holder(
        scripting::MacroExecutionMode execution_mode) = 0;
    virtual bool debug_console_exists() const = 0;
    virtual void log(std::string_view message) = 0;
    virtual std::string ansi_text_from_bstr(const char* text) const = 0;
    virtual void assign_output_bstr(std::string_view text,
                                    wchar_t** output_bstr) = 0;
    virtual void unlock_ole_application() = 0;
};

class CSfcOLE final {
public:
    explicit CSfcOLE(CSfcOLEHost& host) : host_(host) {}
    ~CSfcOLE();

    CSfcOLE(const CSfcOLE&) = delete;
    CSfcOLE& operator=(const CSfcOLE&) = delete;

    bool CreateMacro(const MacroCreateRequest& request,
                     MacroCreateResponse& response);
    bool DestroyMacro(scripting::MacroHolder* holder);
    bool LoadMacro(scripting::MacroHolder* holder,
                   const OleScriptVariant& script);
    // CSfcOLE::RequestMacro @ 0x0042fbf0 hands the callback its VARIANT
    // POINTER, because the callback answers by replacing the BSTR inside it.
    // A copy here would publish the reply into a temporary and leave the
    // caller holding the script it sent.
    std::uint32_t RequestMacro(scripting::MacroHolder* holder,
                               OleScriptVariant* script);
    std::uint32_t ExecuteMacro(scripting::MacroHolder* holder,
                               const OleScriptVariant& script);

    scripting::MacroHolder* CreateCommand(std::uint16_t execution_mode);
    bool DestroyCommand(scripting::MacroHolder* holder);
    void LoadCommand(scripting::MacroHolder* holder,
                     std::string_view command_text);
    std::uint32_t RequestCommand(scripting::MacroHolder* holder,
                                 wchar_t** output_bstr);
    bool FireCommand(std::uint16_t execution_mode,
                     std::string_view command_text,
                     wchar_t** output_bstr);

private:
    scripting::MacroHolder* find_registered(
        scripting::MacroHolder* holder) const;
    bool remove_registered(scripting::MacroHolder* holder);
    void log_if_debug(std::string_view message) const;

    CSfcOLEHost& host_;
    std::vector<std::unique_ptr<scripting::MacroHolder>> macro_holders_;
};

} // namespace creatures1::application
