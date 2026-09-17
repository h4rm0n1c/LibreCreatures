#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "macro.hpp"

namespace creatures1::scripting {

using DdeInstanceId = std::uint32_t;
using DdeStringHandle = std::uint32_t;
using DdeConversationHandle = std::uint32_t;
using DdeDataHandle = std::uint32_t;

// The platform adapter converts the DDEML XTYP_* values into this semantic
// vocabulary before entering the C1 owner.  The C1 owner therefore does not
// depend on Windows' bit-packed transaction constants.
enum class DdeTransactionKind {
    connect,
    connect_confirm,
    execute,
    poke,
    request,
    disconnect,
    other,
};

struct DdeCallbackRequest {
    DdeTransactionKind transaction = DdeTransactionKind::other;
    std::uint32_t clipboard_format = 0;
    DdeConversationHandle conversation = 0;
    DdeStringHandle item_or_topic = 0;
    DdeStringHandle service_or_item = 0;
    DdeDataHandle data = 0;
    std::uint32_t data1 = 0;
    std::uint32_t data2 = 0;
};

// The callback entry is a Windows DDEML ABI concern.  The clean owner only
// needs to select the service callback registered by the application.
enum class DdeCallbackRegistration {
    service,
};

enum class DdeItemKind {
    macro_output,
    brain_activity,
    brain_wiring,
    system_info,
    unknown,
};

struct DdeServiceItem {
    std::string_view name;
    DdeStringHandle handle = 0;
    DdeItemKind kind = DdeItemKind::unknown;
};

// These names are the semantic form of the fourteen %d arguments recovered
// from CreateSystemInfoData.  The platform adapter supplies the live values;
// the owner controls their order and delimiter format.
struct DdeSystemInfoSnapshot {
    std::int32_t non_scenery_object_count = 0;
    std::int32_t entity_count = 0;
    std::int32_t creature_count = 0;
    std::int32_t script_definition_count = 0;
    std::int32_t running_macro_count = 0;
    std::int32_t gallery_count = 0;
    std::int32_t image_cache_entry_count = 0;
    std::int32_t image_cache_bytes = 0;
    std::int32_t room_count = 0;
    std::int32_t ambient_environment_index = 0;
    std::int32_t selected_action_id = 0;
    std::int32_t selected_action_activation_boost = 0;
    std::int32_t selected_creature_motion_link = 0;
    std::int32_t smoothed_idle_cycle_index = 0;
};

// DDE item producers own the response semantics.  Only creation of a native
// data handle and live registry/Brain access remain host responsibilities.
class DdeItemDataHost {
public:
    virtual ~DdeItemDataHost() = default;

    virtual std::string render_macro_output(Macro& macro,
                                            std::size_t output_capacity) = 0;
    virtual std::string render_brain_activity(
        Macro& macro, std::size_t output_capacity) = 0;
    virtual DdeSystemInfoSnapshot read_system_info() const = 0;
    // Creates the DDE data handle from exactly these bytes.  Each producer
    // decides whether a terminator is part of its reply, as the native ones
    // do by the byte count they hand DdeCreateDataHandle.
    virtual DdeDataHandle create_data(const DdeServiceItem& item,
                                      std::string_view bytes) = 0;

    // BrainWiring's entry in the item table is 0x00410260,
    // Object::DefaultImageVirtualReturnZero -- the shipped game answers the
    // item with nothing.  The hook stays so the item keeps its slot and its
    // registration order, but there is no producer to translate.
    virtual DdeDataHandle create_object_owned_item_data(
        const DdeServiceItem& item, Macro& macro) = 0;
};

struct DdeConversation {
    DdeConversationHandle handle = 0;
    std::string topic_name;
    Macro* macro = nullptr;
};

struct DdeServiceState {
    static constexpr std::size_t kMaximumItems = 8;
    static constexpr std::size_t kMaximumConversations = 100;

    std::array<DdeServiceItem*, kMaximumItems> item_slots{};
    std::array<DdeConversation*, kMaximumConversations> conversation_slots{};
    std::size_t item_count = 0;
    std::size_t conversation_count = 0;
    DdeInstanceId instance_id = 0;
    DdeStringHandle service_handle = 0;
    std::string_view service_name;
};

class DdeRuntimeHost {
public:
    virtual ~DdeRuntimeHost() = default;

    virtual bool initialize(DdeInstanceId& instance_id,
                            DdeCallbackRegistration callback,
                            std::uint32_t command_filter) = 0;
    virtual DdeStringHandle create_string_handle(DdeInstanceId instance_id,
                                                 std::string_view text) = 0;
    virtual void publish_service(DdeInstanceId instance_id,
                                 DdeStringHandle service_handle) = 0;
    virtual void unpublish_service(DdeInstanceId instance_id,
                                   DdeStringHandle service_handle) = 0;
    virtual void free_string_handle(DdeInstanceId instance_id,
                                    DdeStringHandle string_handle) = 0;
    virtual void uninitialize(DdeInstanceId instance_id) = 0;
    virtual void register_shutdown() = 0;
};

class DdeScoreNotificationHost {
public:
    virtual ~DdeScoreNotificationHost() = default;

    virtual bool score_notification_endpoint_available() const = 0;
    virtual void notify_score_changed() = 0;
};

// C1-owned DDE callback policy sees only semantic data and service
// operations.  DDEML access/unaccess/query/create/disconnect, debug UI,
// embedded-kit state, and Macro execution are implemented by the application
// adapter.
class DdeCallbackHost : public virtual MacroExecutionHost,
                        public DdeItemDataHost {
public:
    ~DdeCallbackHost() override = default;

    virtual bool debug_console_visible() const = 0;
    virtual bool debug_logging_enabled() const = 0;
    virtual void log_execute_script(std::string_view script_text) = 0;
    virtual void log_missing_conversation() = 0;
    virtual void log_missing_item() = 0;

    virtual std::string query_topic(DdeStringHandle topic_handle) = 0;
    virtual std::string_view access_data(DdeDataHandle data_handle) = 0;
    virtual void unaccess_data(DdeDataHandle data_handle) = 0;
    virtual void start_macro_execution(Macro& macro) = 0;
    virtual void disconnect(DdeConversationHandle conversation) = 0;
    virtual void set_tool_available(std::string_view topic_name,
                                    bool available) = 0;
};

// Source-native owner for the recovered 00401090 startup body.  The four
// item definitions are supplied by the DDE module composition; their names
// and handlers are recovered by the later DDE item slices.
bool initialize_dde_service(DdeRuntimeHost& runtime,
                            DdeServiceState& state,
                            DdeServiceItem* const* items,
                            std::size_t item_count);

// Source-native owners for the recovered DDE lifecycle and item producers.
void shutdown_dde_service(DdeRuntimeHost& runtime,
                          const DdeServiceState& state);
DdeDataHandle create_dde_item_data(const DdeServiceItem& item,
                                   Macro& macro,
                                   DdeItemDataHost& host);
void notify_dde_score_changed(DdeScoreNotificationHost& host);

// Source-native owner for 0040fdb0. The Windows DDE callback thunk converts
// the ABI request to DdeCallbackRequest and calls this function.
DdeDataHandle handle_dde_callback(DdeServiceState& state,
                                   const DdeCallbackRequest& request,
                                   DdeCallbackHost& host);

} // namespace creatures1::scripting
