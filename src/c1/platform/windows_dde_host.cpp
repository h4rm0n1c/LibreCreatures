#include "windows_dde_host.hpp"

#include "windows_shell.hpp"

#include <ddeml.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace creatures1::platform {
namespace {

// InitializeDdeService @ 0x00401090 registers four items under the service
// name "Vivarium", in this order, with a command filter of 0x00184000.
constexpr std::uint32_t kCommandFilter = 0x00184000U;
constexpr std::string_view kServiceName = "Vivarium";

creatures1::scripting::DdeServiceItem g_macro_item{
    "Macro", 0, creatures1::scripting::DdeItemKind::macro_output};
creatures1::scripting::DdeServiceItem g_brain_activity_item{
    "BrainActivity", 0, creatures1::scripting::DdeItemKind::brain_activity};
creatures1::scripting::DdeServiceItem g_brain_wiring_item{
    "BrainWiring", 0, creatures1::scripting::DdeItemKind::brain_wiring};
creatures1::scripting::DdeServiceItem g_system_info_item{
    "SysInfo", 0, creatures1::scripting::DdeItemKind::system_info};

creatures1::scripting::DdeServiceItem* const g_items[] = {
    &g_macro_item, &g_brain_activity_item, &g_brain_wiring_item,
    &g_system_info_item};

WindowsDdeRuntime g_runtime;
creatures1::scripting::DdeServiceState g_state;
std::unique_ptr<WindowsDdeCallbackHost> g_callback_host;
bool g_service_running = false;

creatures1::scripting::DdeTransactionKind transaction_kind(UINT type) {
    switch (type) {
        case XTYP_CONNECT: return creatures1::scripting::DdeTransactionKind::connect;
        case XTYP_CONNECT_CONFIRM:
            return creatures1::scripting::DdeTransactionKind::connect_confirm;
        case XTYP_EXECUTE: return creatures1::scripting::DdeTransactionKind::execute;
        case XTYP_POKE: return creatures1::scripting::DdeTransactionKind::poke;
        case XTYP_REQUEST: return creatures1::scripting::DdeTransactionKind::request;
        case XTYP_DISCONNECT:
            return creatures1::scripting::DdeTransactionKind::disconnect;
        default: return creatures1::scripting::DdeTransactionKind::other;
    }
}

HDDEDATA CALLBACK dde_callback_entry(UINT type, UINT format, HCONV conversation,
                                     HSZ item_or_topic, HSZ service_or_item,
                                     HDDEDATA data, ULONG_PTR data1,
                                     ULONG_PTR data2) {
    if (g_callback_host == nullptr) {
        return nullptr;
    }
    creatures1::scripting::DdeCallbackRequest request;
    request.transaction = transaction_kind(type);
    request.clipboard_format = format;
    request.conversation =
        static_cast<creatures1::scripting::DdeConversationHandle>(
            reinterpret_cast<std::uintptr_t>(conversation));
    request.item_or_topic =
        static_cast<creatures1::scripting::DdeStringHandle>(
            reinterpret_cast<std::uintptr_t>(item_or_topic));
    request.service_or_item =
        static_cast<creatures1::scripting::DdeStringHandle>(
            reinterpret_cast<std::uintptr_t>(service_or_item));
    request.data = static_cast<creatures1::scripting::DdeDataHandle>(
        reinterpret_cast<std::uintptr_t>(data));
    request.data1 = static_cast<std::uint32_t>(data1);
    request.data2 = static_cast<std::uint32_t>(data2);
    const creatures1::scripting::DdeDataHandle result =
        creatures1::scripting::handle_dde_callback(g_state, request,
                                                   *g_callback_host);
    return reinterpret_cast<HDDEDATA>(static_cast<std::uintptr_t>(result));
}

std::string g_accessed_data;

} // namespace

bool WindowsDdeRuntime::initialize(
    creatures1::scripting::DdeInstanceId& instance_id,
    creatures1::scripting::DdeCallbackRegistration, std::uint32_t filter) {
    DWORD native_id = 0;
    const UINT status =
        DdeInitializeA(&native_id, dde_callback_entry, filter, 0);
    instance_id = static_cast<creatures1::scripting::DdeInstanceId>(native_id);
    return status == DMLERR_NO_ERROR;
}

creatures1::scripting::DdeStringHandle WindowsDdeRuntime::create_string_handle(
    creatures1::scripting::DdeInstanceId instance_id, std::string_view text) {
    const std::string value(text);
    const HSZ handle = DdeCreateStringHandleA(
        static_cast<DWORD>(instance_id), value.c_str(), CP_WINANSI);
    return static_cast<creatures1::scripting::DdeStringHandle>(
        reinterpret_cast<std::uintptr_t>(handle));
}

void WindowsDdeRuntime::publish_service(
    creatures1::scripting::DdeInstanceId instance_id,
    creatures1::scripting::DdeStringHandle service_handle) {
    // InitializeDdeService @ 0x00401090 passes 5: DNS_REGISTER with
    // DNS_FILTERON, so DDEML drops connections to any other service name
    // before they reach the callback.
    DdeNameService(static_cast<DWORD>(instance_id),
                   reinterpret_cast<HSZ>(
                       static_cast<std::uintptr_t>(service_handle)),
                   nullptr, DNS_REGISTER | DNS_FILTERON);
}

void WindowsDdeRuntime::unpublish_service(
    creatures1::scripting::DdeInstanceId instance_id,
    creatures1::scripting::DdeStringHandle service_handle) {
    // ShutdownDdeService @ 0x0044edf0 passes a null name, which unregisters
    // every service the instance published rather than naming this one.
    static_cast<void>(service_handle);
    DdeNameService(static_cast<DWORD>(instance_id), nullptr, nullptr,
                   DNS_UNREGISTER);
}

void WindowsDdeRuntime::free_string_handle(
    creatures1::scripting::DdeInstanceId instance_id,
    creatures1::scripting::DdeStringHandle string_handle) {
    DdeFreeStringHandle(
        static_cast<DWORD>(instance_id),
        reinterpret_cast<HSZ>(static_cast<std::uintptr_t>(string_handle)));
}

void WindowsDdeRuntime::uninitialize(
    creatures1::scripting::DdeInstanceId instance_id) {
    DdeUninitialize(static_cast<DWORD>(instance_id));
}

void WindowsDdeRuntime::register_shutdown() {
    // The native registers its teardown through atexit; the port tears the
    // service down from the application's own shutdown instead, so there is
    // nothing to hook here.
}

bool WindowsDdeCallbackHost::debug_console_visible() const { return false; }

bool WindowsDdeCallbackHost::debug_logging_enabled() const { return false; }

void WindowsDdeCallbackHost::log_execute_script(std::string_view script_text) {
    const std::string message =
        "DDE Execute string: \"" + std::string(script_text) + "\"\n";
    ::OutputDebugStringA(message.c_str());
}

void WindowsDdeCallbackHost::log_missing_conversation() {
    ::OutputDebugStringA("DDEService::FindConversation() failed\n");
}

void WindowsDdeCallbackHost::log_missing_item() {
    ::OutputDebugStringA("DDEService::FindItem() failed\n");
}

std::string WindowsDdeCallbackHost::query_topic(
    creatures1::scripting::DdeStringHandle topic_handle) {
    std::array<char, 0x20> topic{};
    DdeQueryStringA(static_cast<DWORD>(g_state.instance_id),
                    reinterpret_cast<HSZ>(
                        static_cast<std::uintptr_t>(topic_handle)),
                    topic.data(), static_cast<DWORD>(topic.size()),
                    CP_WINANSI);
    return std::string(topic.data());
}

std::string_view WindowsDdeCallbackHost::access_data(
    creatures1::scripting::DdeDataHandle data_handle) {
    DWORD length = 0;
    const LPBYTE bytes = DdeAccessData(
        reinterpret_cast<HDDEDATA>(static_cast<std::uintptr_t>(data_handle)),
        &length);
    if (bytes == nullptr) {
        g_accessed_data.clear();
        return g_accessed_data;
    }
    // The native hands DdeAccessData's pointer straight to
    // Macro::LoadScriptText as a C string, so the script ends at the first
    // NUL, not at the end of the data.
    const char* text = reinterpret_cast<const char*>(bytes);
    std::size_t text_length = 0;
    while (text_length < length && text[text_length] != '\0') {
        ++text_length;
    }
    g_accessed_data.assign(text, text_length);
    return g_accessed_data;
}

void WindowsDdeCallbackHost::unaccess_data(
    creatures1::scripting::DdeDataHandle data_handle) {
    DdeUnaccessData(
        reinterpret_cast<HDDEDATA>(static_cast<std::uintptr_t>(data_handle)));
}

void WindowsDdeCallbackHost::start_macro_execution(
    creatures1::scripting::Macro& macro) {
    WindowsMacroHost::start_macro(macro);
}

void WindowsDdeCallbackHost::disconnect(
    creatures1::scripting::DdeConversationHandle conversation) {
    DdeDisconnect(
        reinterpret_cast<HCONV>(static_cast<std::uintptr_t>(conversation)));
}

void WindowsDdeCallbackHost::set_tool_available(std::string_view topic_name,
                                                bool available) {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return;
    }
    const auto& definitions = frame->embedded_kit_definitions();
    std::array<creatures1::application::EmbeddedKitAvailabilityRecord,
               creatures1::application::kEmbeddedKitSlotCount>
        records{};
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        records[index].registry_tool_name = definitions[index].prog_id;
    }
    creatures1::application::set_embedded_kit_tool_availability_by_name(
        records.data(), definitions.size(), topic_name, available);
}

std::string WindowsDdeCallbackHost::render_macro_output(
    creatures1::scripting::Macro& macro, std::size_t output_capacity) {
    // Not execute_macro_to_output_buffer: that helper serves the pipe and
    // caps a reply at its 0x1000 bytes, while CreateMacroData's own buffer is
    // 0x4000.  The count Macro::execute_to_output_buffer returns includes the
    // NUL over the final separator, which is exactly what the native sends.
    creatures1::scripting::MacroSchedulerHostAdapter scheduler(*this, *this);
    std::string output;
    const std::size_t written =
        macro.execute_to_output_buffer(scheduler, output);
    output.resize((std::min)(written, output_capacity));
    if (!output.empty()) {
        output.back() = '\0';
    }
    return output;
}

std::string WindowsDdeCallbackHost::render_brain_activity(
    creatures1::scripting::Macro& macro, std::size_t output_capacity) {
    std::string buffer(output_capacity, '\0');
    // CreateBrainActivityData @ 0x004101e0 reports on the macro's TARGET
    // object, and takes the report mode and rule index from the macro's first
    // two work values -- so a kit selects what it wants to see by setting
    // those before it requests the item.
    const char* report = WindowsMacroHost::format_brain_activity_report(
        macro.object_context.target_object, buffer.data(),
        macro.caos_work_values[0], macro.caos_work_values[1],
        macro.caos_work_values[2] == 1);
    if (report == nullptr) {
        return std::string();
    }
    buffer.resize(std::strlen(buffer.c_str()));
    return buffer;
}

creatures1::scripting::DdeSystemInfoSnapshot
WindowsDdeCallbackHost::read_system_info() const {
    // The document owns the fourteen live counters CreateSystemInfoData
    // reports, in the same snapshot the debug readout uses.
    return document_ref_.system_info_snapshot();
}

creatures1::scripting::DdeDataHandle WindowsDdeCallbackHost::create_data(
    const creatures1::scripting::DdeServiceItem& item, std::string_view bytes) {
    std::string value(bytes);
    const HDDEDATA handle = DdeCreateDataHandle(
        static_cast<DWORD>(g_state.instance_id),
        reinterpret_cast<LPBYTE>(value.data()),
        static_cast<DWORD>(value.size()), 0,
        reinterpret_cast<HSZ>(static_cast<std::uintptr_t>(item.handle)),
        CF_TEXT, 0);
    return static_cast<creatures1::scripting::DdeDataHandle>(
        reinterpret_cast<std::uintptr_t>(handle));
}

creatures1::scripting::DdeDataHandle
WindowsDdeCallbackHost::create_object_owned_item_data(
    const creatures1::scripting::DdeServiceItem&,
    creatures1::scripting::Macro&) {
    // Not a hold.  The item table at 0x0046674c pairs each name with its
    // producer, and "BrainWiring" is paired with 0x00410260 --
    // Object::DefaultImageVirtualReturnZero, whose whole body is `return 0`.
    // The shipped game registers the item and answers nothing for it, so a
    // wiring dump here would be a feature the original does not have.
    return 0;
}

bool start_dde_service(C1WindowsDocument& document) {
    if (g_service_running) {
        return true;
    }
    g_callback_host = std::make_unique<WindowsDdeCallbackHost>(document);
    g_state = creatures1::scripting::DdeServiceState{};
    g_state.service_name = kServiceName;
    if (!creatures1::scripting::initialize_dde_service(
            g_runtime, g_state, g_items,
            sizeof(g_items) / sizeof(g_items[0]))) {
        g_callback_host.reset();
        return false;
    }
    g_service_running = true;
    return true;
}

void stop_dde_service() {
    if (!g_service_running) {
        return;
    }
    creatures1::scripting::shutdown_dde_service(g_runtime, g_state);
    g_callback_host.reset();
    g_service_running = false;
}

} // namespace creatures1::platform
