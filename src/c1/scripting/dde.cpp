#include "dde.hpp"

#include <algorithm>
#include <new>

namespace creatures1::scripting {

bool initialize_dde_service(DdeRuntimeHost& runtime,
                            DdeServiceState& state,
                            DdeServiceItem* const* items,
                            std::size_t item_count) {
    if (items == nullptr || state.item_count > DdeServiceState::kMaximumItems ||
        item_count > DdeServiceState::kMaximumItems - state.item_count) {
        return false;
    }

    // The native initializer appends the four static DDE items to the global
    // item table before clearing the active conversation count.
    for (std::size_t index = 0; index < item_count; ++index) {
        if (items[index] == nullptr) {
            return false;
        }
        state.item_slots[state.item_count++] = items[index];
    }
    state.conversation_count = 0;
    state.instance_id = 0;

    // 0x184000 is the exact DDEML command filter passed by Creatures.exe.
    if (!runtime.initialize(state.instance_id,
                            DdeCallbackRegistration::service,
                            0x184000u)) {
        return false;
    }

    state.service_name = "Vivarium";
    state.service_handle =
        runtime.create_string_handle(state.instance_id, state.service_name);
    runtime.publish_service(state.instance_id, state.service_handle);

    for (std::size_t index = 0; index < state.item_count; ++index) {
        DdeServiceItem& item = *state.item_slots[index];
        item.handle = 0;
        item.handle = runtime.create_string_handle(state.instance_id,
                                                   item.name);
    }

    // The native 0040fda0 entry is a one-instruction tail jump to the real
    // callback.  Callback registration owns that ABI detail; it is not a
    // separate C1 behavior that needs a synthetic function body here.
    runtime.register_shutdown();
    return true;
}

void shutdown_dde_service(DdeRuntimeHost& runtime,
                          const DdeServiceState& state) {
    // Preserve the exact native order from 0044edf0.  These operations are
    // DDEML boundary calls; the clean owner only expresses their lifecycle.
    runtime.unpublish_service(state.instance_id, state.service_handle);
    runtime.free_string_handle(state.instance_id, state.service_handle);
    runtime.uninitialize(state.instance_id);
}

namespace {

constexpr DdeDataHandle kAccepted = 0x8000u;

DdeConversation* find_conversation(DdeServiceState& state,
                                   DdeConversationHandle handle,
                                   DdeCallbackHost& host) {
    for (std::size_t index = 0; index < state.conversation_count; ++index) {
        DdeConversation* conversation = state.conversation_slots[index];
        if (conversation != nullptr && conversation->handle == handle) {
            return conversation;
        }
    }
    host.log_missing_conversation();
    return nullptr;
}

DdeServiceItem* find_item(DdeServiceState& state, DdeStringHandle handle) {
    for (std::size_t index = 0; index < state.item_count; ++index) {
        DdeServiceItem* item = state.item_slots[index];
        if (item != nullptr && item->handle == handle) {
            return item;
        }
    }
    return nullptr;
}

void remove_conversation(DdeServiceState& state,
                         std::size_t removed_index) {
    for (std::size_t index = removed_index + 1;
         index < state.conversation_count; ++index) {
        state.conversation_slots[index - 1] = state.conversation_slots[index];
    }
    if (state.conversation_count != 0) {
        --state.conversation_count;
        state.conversation_slots[state.conversation_count] = nullptr;
    }
}

void append_system_info_field(std::string& output, std::int32_t value) {
    output += std::to_string(value);
    output.push_back('|');
}

} // namespace

DdeDataHandle create_dde_item_data(const DdeServiceItem& item,
                                   Macro& macro,
                                   DdeItemDataHost& host) {
    switch (item.kind) {
    case DdeItemKind::macro_output: {
        // CreateMacroData @ 0x00410120 renders into a 0x4000-byte buffer and
        // passes DdeCreateDataHandle ExecuteToOutputBuffer's count as the
        // size.  That count already includes the NUL written over the final
        // '|', so the rendered bytes go out as they are -- and an empty
        // reply goes out as zero bytes.
        const std::string output = host.render_macro_output(macro, 0x4000u);
        return host.create_data(item, output);
    }
    case DdeItemKind::brain_activity: {
        // CreateBrainActivityData @ 0x004101e0 uses a 9000-byte buffer and
        // passes FormatActivityReport's count, which includes its NUL.
        std::string output = host.render_brain_activity(macro, 9000u);
        output.push_back('\0');
        return host.create_data(item, output);
    }
    case DdeItemKind::system_info: {
        const DdeSystemInfoSnapshot info = host.read_system_info();
        std::string output;
        output.reserve(96);
        append_system_info_field(output, info.non_scenery_object_count);
        append_system_info_field(output, info.entity_count);
        append_system_info_field(output, info.creature_count);
        append_system_info_field(output, info.script_definition_count);
        append_system_info_field(output, info.running_macro_count);
        append_system_info_field(output, info.gallery_count);
        append_system_info_field(output, info.image_cache_entry_count);
        append_system_info_field(output, info.image_cache_bytes);
        append_system_info_field(output, info.room_count);
        append_system_info_field(output, info.ambient_environment_index);
        append_system_info_field(output, info.selected_action_id);
        append_system_info_field(output,
                                 info.selected_action_activation_boost);
        append_system_info_field(output, info.selected_creature_motion_link);
        append_system_info_field(output, info.smoothed_idle_cycle_index);
        // CreateSystemInfoData @ 0x00410270 passes snprintf's length + 1:
        // the text and its NUL, with the final '|' kept.
        output.push_back('\0');
        return host.create_data(item, output);
    }
    case DdeItemKind::brain_wiring:
        return host.create_object_owned_item_data(item, macro);
    case DdeItemKind::unknown:
        return 0;
    }
    return 0;
}

void notify_dde_score_changed(DdeScoreNotificationHost& host) {
    // Native NotifyDDEScoreChanged checks embedded record 8's IDispatch
    // pointer before crossing into MFC/COM automation.
    if (host.score_notification_endpoint_available()) {
        host.notify_score_changed();
    }
}

DdeDataHandle handle_dde_callback(DdeServiceState& state,
                                   const DdeCallbackRequest& request,
                                   DdeCallbackHost& host) {
    switch (request.transaction) {
    case DdeTransactionKind::connect:
        // DdeCallback @ 0x0040fdb0 compares the SERVICE name (the second
        // string handle) with "Vivarium" and accepts any topic: the topic is
        // a kit's ProgID, recorded at CONNECT_CONFIRM.  Up to 100
        // conversations.
        return state.conversation_count <
                       DdeServiceState::kMaximumConversations &&
                       request.service_or_item == state.service_handle
                   ? 1u
                   : 0u;

    case DdeTransactionKind::execute: {
        DdeConversation* conversation =
            find_conversation(state, request.conversation, host);
        if (conversation == nullptr || conversation->macro == nullptr) {
            return 0;
        }
        const std::string_view script = host.access_data(request.data);
        if (host.debug_console_visible() && host.debug_logging_enabled()) {
            host.log_execute_script(script);
        }
        conversation->macro->load_script_text(script);
        host.start_macro_execution(*conversation->macro);
        host.unaccess_data(request.data);
        return kAccepted;
    }

    case DdeTransactionKind::request: {
        DdeConversation* conversation =
            find_conversation(state, request.conversation, host);
        DdeServiceItem* item = find_item(state, request.service_or_item);
        if (item == nullptr) {
            if (host.debug_console_visible()) {
                host.log_missing_item();
            }
            return 0;
        }
        if (conversation == nullptr || conversation->macro == nullptr) {
            return 0;
        }
        return create_dde_item_data(*item, *conversation->macro, host);
    }

    case DdeTransactionKind::connect_confirm: {
        if (state.conversation_count >=
            DdeServiceState::kMaximumConversations) {
            return 0;
        }

        DdeConversation* conversation = new (std::nothrow) DdeConversation;
        if (conversation == nullptr) {
            return 0;
        }
        conversation->handle = request.conversation;
        conversation->topic_name = host.query_topic(request.item_or_topic);
        host.set_tool_available(conversation->topic_name, true);

        conversation->macro = new (std::nothrow) Macro;
        if (conversation->macro == nullptr) {
            host.set_tool_available(conversation->topic_name, false);
            delete conversation;
            return 0;
        }
        conversation->macro->object_context.script_owner =
            host.selected_creature();
        conversation->macro->object_context.from_object = nullptr;
        conversation->macro->object_context.exec_object =
            host.initial_auxiliary_object();
        conversation->macro->reset_execution_state(host);
        state.conversation_slots[state.conversation_count++] = conversation;
        return 0;
    }

    case DdeTransactionKind::poke: {
        DdeConversation* conversation =
            find_conversation(state, request.conversation, host);
        if (conversation == nullptr || conversation->macro == nullptr) {
            return 0;
        }
        const std::string_view script = host.access_data(request.data);
        conversation->macro->load_script_text(script);
        host.unaccess_data(request.data);
        return kAccepted;
    }

    case DdeTransactionKind::disconnect:
        for (std::size_t index = 0; index < state.conversation_count;
             ++index) {
            DdeConversation* conversation = state.conversation_slots[index];
            if (conversation == nullptr ||
                conversation->handle != request.conversation) {
                continue;
            }
            host.disconnect(conversation->handle);
            if (!conversation->topic_name.empty()) {
                host.set_tool_available(conversation->topic_name, false);
            }
            if (conversation->macro != nullptr) {
                conversation->macro->remove_from_running_scheduler_and_release();
            }
            delete conversation;
            remove_conversation(state, index);
            break;
        }
        return 0;

    case DdeTransactionKind::other:
        return 0;
    }
    return 0;
}

} // namespace creatures1::scripting
