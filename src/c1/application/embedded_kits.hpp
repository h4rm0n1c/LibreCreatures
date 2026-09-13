#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::application {

using GenomeFilenameId = std::uint32_t;

// The embedded-kit wire protocol.
//
// Every message the executable sends a kit is the same call, and there is only
// one: COleDispatchDriver::InvokeHelper with DISPID 1, DISPATCH_METHOD, a
// VT_BOOL return, and parameter info "\x4c\x4c" -- two VTS_PVARIANT arguments,
// each a VARIANT of VT_I4.  All nine senders in the image build it identically
// (ExecuteEmbeddedKitTool's two branches, BroadcastEmbeddedControlState,
// NotifyEmbeddedKit9OfCreatureDeath, FlushFuneralKitDocumentStateWords,
// NotifyDDEScoreChanged, SFCDoc::UpdateWorld, Creature::Deserialize and
// Creature::RemoveFromWorld), so the shape is the protocol rather than one
// caller's convention.
//
// The first argument is a packed header and the second a payload.  On the kit
// side -- read out of Science Kit's dispatch map entry for "Communicate", which
// declares the same "\x4c\x4c" and VT_BOOL -- CScienceSheet::InvokeAutomationMethod
// reads only the header's low byte to choose between two sheet handlers, so
// that byte is the message kind and the rest is the kind's own business.
struct EmbeddedKitMessage {
    std::uint32_t header = 0;
    std::uint32_t payload = 0;
};

enum class EmbeddedKitMessageKind : std::uint8_t {
    data = 1,
    control_state = 2,
};

// header = aux:16 | code:8 | kind:8.
constexpr std::uint32_t embedded_kit_message_header(
    EmbeddedKitMessageKind kind, std::uint8_t code, std::uint16_t aux = 0) {
    return (static_cast<std::uint32_t>(aux) << 16) |
           (static_cast<std::uint32_t>(code) << 8) |
           static_cast<std::uint32_t>(kind);
}

// Codes seen on the wire.  `identity` carries the slot number in the header's
// aux field and an empty payload; `data` hands the kit one integer and lets it
// decide what to do with it.
constexpr std::uint8_t kEmbeddedKitIdentityCode = 3;
constexpr std::uint8_t kEmbeddedKitDataCode = 4;

// Constructing the VARIANT pair and invoking the dispatch is the platform's
// job; choosing the message is the application's.
class EmbeddedKitMessageApi {
public:
    virtual ~EmbeddedKitMessageApi() = default;
    virtual bool send_kit_message(std::size_t tool_index,
                                  const EmbeddedKitMessage& message) = 0;
};

// The application owns the decision to notify an embedded kit.  COM/OLE and
// process launch belong to the platform adapter that implements this boundary.
class EmbeddedKitGateway : public EmbeddedKitMessageApi {
public:
    virtual bool embedded_kit_is_connected(std::size_t tool_index) const = 0;
    virtual void execute_embedded_kit_tool(std::size_t tool_index) = 0;
};

void notify_funeral_kit_of_creature_death(EmbeddedKitGateway& gateway,
                                          GenomeFilenameId genome_filename);

// YOUR_ID_IS tells a freshly launched kit which tool slot it occupies.  Both
// launch branches send it, the native one inline and the Wine one through the
// adapter at 0042d860; the message is identical either way.
bool send_your_id_is_message(EmbeddedKitMessageApi& api,
                             std::size_t tool_index);

constexpr std::size_t kEmbeddedKitSlotCount = 20;

// The C1 policy broadcasts one state value to every connected kit.  The slot
// walk and the null-dispatch guard are policy; the invoke is the platform's.
class EmbeddedKitControlApi : public EmbeddedKitMessageApi {
public:
    virtual bool has_dispatch(std::size_t tool_index) const = 0;
};

void broadcast_embedded_control_state(EmbeddedKitControlApi& api,
                                      std::uint8_t state_code);

struct EmbeddedKitToolDefinition {
    std::string prog_id;
    std::string display_name;
    std::string launch_command;
    std::uint8_t adjusted_type_code = 0;
};

// `tool` writes the fourth byte exactly as supplied by the CAOS rvalue.  The
// menu loader later applies its +6 presentation adjustment, so keep the raw
// registration value distinct from EmbeddedKitToolDefinition's decoded form.
struct EmbeddedKitToolRegistration {
    std::string registry_name;
    std::string display_name;
    std::string launch_command;
    std::uint8_t raw_type_code = 0;
};

// The application owns the registry-record grammar and the fixed 20-slot
// iteration. Registry handles, MFC menu/toolbar objects, and CMainFrame's
// 0xb4-byte record storage are platform/owner operations.
class EmbeddedKitMenuApi {
public:
    virtual ~EmbeddedKitMenuApi() = default;
    virtual bool tools_menu_exists() const = 0;
    virtual bool read_tool_registry_value(std::size_t tool_index,
                                          std::string& value) const = 0;
    virtual void publish_tool_definition(
        std::size_t tool_index,
        const EmbeddedKitToolDefinition& definition,
        std::uint8_t adjusted_type_code) = 0;
    virtual int add_toolbar_bitmap(std::string_view prog_id) = 0;
    virtual void set_toolbar_button(std::size_t tool_index,
                                    std::uint32_t command_id,
                                    int image_index) = 0;
    virtual void append_tool_menu_item(std::uint32_t command_id,
                                       std::string_view display_name) = 0;
    virtual void invalidate_toolbar_and_menu() = 0;
};

void populate_embedded_kit_menu_and_toolbar(EmbeddedKitMenuApi& api);

// Registry handles, the native 0xb4-byte record copy, and the MFC/COM
// lifetime are platform operations.  This interface exposes only the
// application policy recovered from Macro::ExecuteInterpreter's `tool`
// branch: fixed capacity, duplicate-name rejection, count update, menu
// rebuild, and availability publication.
class EmbeddedKitRegistrationApi {
public:
    virtual ~EmbeddedKitRegistrationApi() = default;
    virtual bool read_tool_count(std::size_t& count) const = 0;
    virtual bool tool_name_exists(std::string_view registry_name) const = 0;
    virtual void write_tool_record(
        std::size_t tool_index,
        const EmbeddedKitToolRegistration& registration) = 0;
    virtual void write_tool_count(std::size_t count) = 0;
    virtual void rebuild_tool_menu_and_toolbar() = 0;
    virtual void set_tool_available(std::string_view registry_name,
                                    bool is_available) = 0;
};

bool register_embedded_kit_tool(EmbeddedKitRegistrationApi& api,
                                const EmbeddedKitToolRegistration& registration);

// The branch choice is C1 application policy. Native COM activation and the
// Wine OLEKitProxy/DLL-injection/pipe handshake are complete platform
// operations and remain behind this adapter.
class EmbeddedKitExecutionApi {
public:
    virtual ~EmbeddedKitExecutionApi() = default;
    virtual bool running_under_wine() const = 0;
    virtual bool launch_via_native_com(std::size_t tool_index) = 0;
    virtual bool launch_via_wine_proxy(std::size_t tool_index) = 0;
};

bool execute_embedded_kit_tool(EmbeddedKitExecutionApi& api,
                               std::size_t tool_index);

using EmbeddedKitProcessHandle = std::uintptr_t;

// The shutdown policy owns ordering and the STILL_ACTIVE decision.  The
// concrete dispatch, HANDLE, toolbar, and diagnostic operations stay behind
// this narrow platform boundary.
class EmbeddedKitShutdownApi {
public:
    virtual ~EmbeddedKitShutdownApi() = default;
    virtual bool has_dispatch(std::size_t tool_index) const = 0;
    virtual void release_dispatch(std::size_t tool_index) = 0;
    virtual EmbeddedKitProcessHandle take_process_handle(
        std::size_t tool_index) = 0;
    virtual bool query_process_exit_code(EmbeddedKitProcessHandle handle,
                                         std::uint32_t& exit_code) const = 0;
    virtual void terminate_process(EmbeddedKitProcessHandle handle) = 0;
    virtual void close_process_handle(EmbeddedKitProcessHandle handle) = 0;
    virtual void invalidate_toolbar() = 0;
    virtual bool diagnostics_enabled() const = 0;
    virtual void log_dispatch_release(std::size_t tool_index,
                                      int active_tool_count) = 0;
    virtual void log_process_termination(std::size_t tool_index) = 0;
};

bool shutdown_embedded_kit_tool(EmbeddedKitShutdownApi& api,
                                std::size_t tool_index,
                                int& active_tool_count);

struct EmbeddedKitAvailabilityRecord {
    std::string_view registry_tool_name;
    bool is_available = false;
};

// The CMainFrame owner supplies records in its original order.  The
// application policy changes only the matching availability flag; record
// storage and MFC ownership stay outside this helper.
void set_embedded_kit_tool_availability_by_name(
    EmbeddedKitAvailabilityRecord* records,
    std::size_t record_count,
    std::string_view tool_name,
    bool is_available);

} // namespace creatures1::application
