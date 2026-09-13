#include "embedded_kits.hpp"

#include <array>

namespace creatures1::application {

namespace {

constexpr std::size_t kFuneralKitIndex = 9;

} // namespace

void notify_funeral_kit_of_creature_death(EmbeddedKitGateway& gateway,
                                          GenomeFilenameId genome_filename) {
    if (!gateway.embedded_kit_is_connected(kFuneralKitIndex)) {
        gateway.execute_embedded_kit_tool(kFuneralKitIndex);
    }

    if (gateway.embedded_kit_is_connected(kFuneralKitIndex)) {
        gateway.send_kit_message(
            kFuneralKitIndex,
            {embedded_kit_message_header(EmbeddedKitMessageKind::data,
                                         kEmbeddedKitDataCode),
             genome_filename});
    }
}

bool send_your_id_is_message(EmbeddedKitMessageApi& api,
                             std::size_t tool_index) {
    // The slot number travels in the header's aux field, not the payload: the
    // recovered senders both build (tool_index << 16) | 0x0301 and pass zero.
    return api.send_kit_message(
        tool_index,
        {embedded_kit_message_header(
             EmbeddedKitMessageKind::data, kEmbeddedKitIdentityCode,
             static_cast<std::uint16_t>(tool_index)),
         0});
}

namespace {

bool decode_tool_definition(std::string_view encoded,
                            EmbeddedKitToolDefinition& definition) {
    std::array<std::string_view, 4> fields{};
    std::size_t field_count = 0;
    std::size_t field_start = 0;
    for (std::size_t index = 0; index <= encoded.size(); ++index) {
        if (index != encoded.size() && encoded[index] != '|') {
            continue;
        }
        if (field_count == fields.size()) {
            return false;
        }
        fields[field_count++] = encoded.substr(field_start, index - field_start);
        field_start = index + 1;
    }
    if (field_count != fields.size() || fields[0].size() >= 0x20 ||
        fields[1].size() >= 0x20 || fields[2].size() >= 100 ||
        fields[3].empty()) {
        return false;
    }

    definition.prog_id = fields[0];
    definition.display_name = fields[1];
    definition.launch_command = fields[2];
    definition.adjusted_type_code =
        static_cast<std::uint8_t>(fields[3].front() + 6);
    return true;
}

} // namespace

void populate_embedded_kit_menu_and_toolbar(EmbeddedKitMenuApi& api) {
    if (!api.tools_menu_exists()) {
        return;
    }

    constexpr std::uint32_t kFirstToolCommand = 0x8086;
    for (std::size_t tool_index = 0;
         tool_index < kEmbeddedKitSlotCount;
         ++tool_index) {
        std::string encoded_definition;
        if (!api.read_tool_registry_value(tool_index, encoded_definition)) {
            continue;
        }

        EmbeddedKitToolDefinition definition;
        if (!decode_tool_definition(encoded_definition, definition)) {
            continue;
        }

        api.publish_tool_definition(tool_index, definition,
                                    definition.adjusted_type_code);
        int image_index = api.add_toolbar_bitmap(definition.prog_id);
        if (image_index < 0) {
            image_index = static_cast<int>(definition.adjusted_type_code) - 0x30;
        }
        const std::uint32_t command_id =
            kFirstToolCommand + static_cast<std::uint32_t>(tool_index);
        api.set_toolbar_button(tool_index, command_id, image_index);
        api.append_tool_menu_item(command_id, definition.display_name);
    }
    api.invalidate_toolbar_and_menu();
}

bool register_embedded_kit_tool(
    EmbeddedKitRegistrationApi& api,
    const EmbeddedKitToolRegistration& registration) {
    // Native RegQueryValueExA failure falls through with an empty tool count;
    // the first registration therefore uses slot zero rather than aborting.
    std::size_t tool_count = 0;
    api.read_tool_count(tool_count);

    // Both rejection paths still publish the requested name as available in
    // the native branch.  The registry/COM adapter owns the exact record
    // lookup and availability storage.
    if (tool_count >= kEmbeddedKitSlotCount ||
        api.tool_name_exists(registration.registry_name)) {
        api.set_tool_available(registration.registry_name, true);
        return false;
    }

    api.write_tool_record(tool_count, registration);
    api.write_tool_count(tool_count + 1);
    api.rebuild_tool_menu_and_toolbar();
    api.set_tool_available(registration.registry_name, true);
    return true;
}

bool execute_embedded_kit_tool(EmbeddedKitExecutionApi& api,
                               std::size_t tool_index) {
    if (api.running_under_wine()) {
        return api.launch_via_wine_proxy(tool_index);
    }
    return api.launch_via_native_com(tool_index);
}

void broadcast_embedded_control_state(EmbeddedKitControlApi& api,
                                      std::uint8_t state_code) {
    for (std::size_t tool_index = 0;
         tool_index < kEmbeddedKitSlotCount;
         ++tool_index) {
        if (api.has_dispatch(tool_index)) {
            api.send_kit_message(
                tool_index,
                {embedded_kit_message_header(
                     EmbeddedKitMessageKind::control_state, state_code),
                 0});
        }
    }
}

bool shutdown_embedded_kit_tool(EmbeddedKitShutdownApi& api,
                                std::size_t tool_index,
                                int& active_tool_count) {
    if (api.has_dispatch(tool_index)) {
        api.release_dispatch(tool_index);
        --active_tool_count;
        if (api.diagnostics_enabled()) {
            api.log_dispatch_release(tool_index, active_tool_count);
        }
    }

    const EmbeddedKitProcessHandle process =
        api.take_process_handle(tool_index);
    if (process != 0) {
        std::uint32_t exit_code = 0;
        if (api.query_process_exit_code(process, exit_code) &&
            exit_code == 0x103u) { // Win32 STILL_ACTIVE.
            if (api.diagnostics_enabled()) {
                api.log_process_termination(tool_index);
            }
            api.terminate_process(process);
        }
        api.close_process_handle(process);
    }

    api.invalidate_toolbar();
    return true;
}

void set_embedded_kit_tool_availability_by_name(
    EmbeddedKitAvailabilityRecord* records,
    std::size_t record_count,
    std::string_view tool_name,
    bool is_available) {
    for (std::size_t index = 0; index < record_count; ++index) {
        if (records[index].registry_tool_name == tool_name) {
            records[index].is_available = is_available;
            return;
        }
    }
}

} // namespace creatures1::application
