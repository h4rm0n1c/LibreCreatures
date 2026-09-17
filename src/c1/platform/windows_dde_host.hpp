#pragma once

#include "windows_prelude.hpp"

#include "windows_macro_host.hpp"

#include "../scripting/dde.hpp"

namespace creatures1::platform {

// DDEML side of InitializeDdeService @ 0x00401090.
class WindowsDdeRuntime final : public creatures1::scripting::DdeRuntimeHost {
public:
    bool initialize(creatures1::scripting::DdeInstanceId& instance_id,
                    creatures1::scripting::DdeCallbackRegistration callback,
                    std::uint32_t command_filter) override;
    creatures1::scripting::DdeStringHandle create_string_handle(
        creatures1::scripting::DdeInstanceId instance_id,
        std::string_view bytes) override;
    void publish_service(
        creatures1::scripting::DdeInstanceId instance_id,
        creatures1::scripting::DdeStringHandle service_handle) override;
    void unpublish_service(
        creatures1::scripting::DdeInstanceId instance_id,
        creatures1::scripting::DdeStringHandle service_handle) override;
    void free_string_handle(
        creatures1::scripting::DdeInstanceId instance_id,
        creatures1::scripting::DdeStringHandle string_handle) override;
    void uninitialize(
        creatures1::scripting::DdeInstanceId instance_id) override;
    void register_shutdown() override;
};

// The conversation side.  The MacroExecutionHost half is the existing macro
// host, shared by virtual inheritance rather than forwarded.
class WindowsDdeCallbackHost final
    : public WindowsMacroHost,
      public creatures1::scripting::DdeCallbackHost {
public:
    explicit WindowsDdeCallbackHost(C1WindowsDocument& document)
        : WindowsMacroHost(document), document_ref_(document) {}

    bool debug_console_visible() const override;
    bool debug_logging_enabled() const override;
    void log_execute_script(std::string_view script_text) override;
    void log_missing_conversation() override;
    void log_missing_item() override;
    std::string query_topic(
        creatures1::scripting::DdeStringHandle topic_handle) override;
    std::string_view access_data(
        creatures1::scripting::DdeDataHandle data_handle) override;
    void unaccess_data(
        creatures1::scripting::DdeDataHandle data_handle) override;
    void start_macro_execution(creatures1::scripting::Macro& macro) override;
    void disconnect(
        creatures1::scripting::DdeConversationHandle conversation) override;
    void set_tool_available(std::string_view topic_name,
                            bool available) override;

    std::string render_macro_output(creatures1::scripting::Macro& macro,
                                    std::size_t output_capacity) override;
    std::string render_brain_activity(creatures1::scripting::Macro& macro,
                                      std::size_t output_capacity) override;
    creatures1::scripting::DdeSystemInfoSnapshot read_system_info()
        const override;
    creatures1::scripting::DdeDataHandle create_data(
        const creatures1::scripting::DdeServiceItem& item,
        std::string_view bytes) override;
    creatures1::scripting::DdeDataHandle create_object_owned_item_data(
        const creatures1::scripting::DdeServiceItem& item,
        creatures1::scripting::Macro& macro) override;

private:
    C1WindowsDocument& document_ref_;
};

// Publishes the "Vivarium" service so external kits can converse.  Returns
// false when DDEML refuses to start.
bool start_dde_service(C1WindowsDocument& document);
void stop_dde_service();

} // namespace creatures1::platform
