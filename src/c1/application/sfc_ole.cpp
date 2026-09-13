#include "sfc_ole.hpp"

#include <array>

namespace creatures1::application {

CSfcOLE::~CSfcOLE() {
    macro_holders_.clear();
    host_.unlock_ole_application();
}

void CSfcOLE::log_if_debug(std::string_view message) const {
    if (host_.debug_console_exists()) {
        host_.log(message);
    }
}

scripting::MacroHolder* CSfcOLE::find_registered(
    scripting::MacroHolder* holder) const {
    for (const auto& registered : macro_holders_) {
        if (registered.get() == holder) {
            return registered.get();
        }
    }
    return nullptr;
}

bool CSfcOLE::remove_registered(scripting::MacroHolder* holder) {
    for (auto position = macro_holders_.begin();
         position != macro_holders_.end(); ++position) {
        if (position->get() == holder) {
            macro_holders_.erase(position);
            return true;
        }
    }
    return false;
}

bool CSfcOLE::CreateMacro(const MacroCreateRequest& request,
                          MacroCreateResponse& response) {
    log_if_debug("CreateMacro.\n");
    if (request.kind != MacroRequestKind::create) {
        return false;
    }

    auto holder = host_.create_macro_holder(
        static_cast<scripting::MacroExecutionMode>(request.execution_mode));
    if (holder == nullptr || holder->macro() == nullptr) {
        return false;
    }

    scripting::MacroHolder* holder_address = holder.get();
    macro_holders_.insert(macro_holders_.begin(), std::move(holder));
    response.kind = MacroResponseKind::created;
    response.holder = holder_address;
    return true;
}

bool CSfcOLE::DestroyMacro(scripting::MacroHolder* holder) {
    log_if_debug("DestroyMacro.\n");
    return remove_registered(holder);
}

bool CSfcOLE::LoadMacro(scripting::MacroHolder* holder,
                        const OleScriptVariant& script) {
    log_if_debug("LoadMacro.\n");
    if (script.type != OleVariantType::bstr || script.bstr_value == nullptr ||
        find_registered(holder) == nullptr || holder->macro() == nullptr) {
        return false;
    }

    if (host_.debug_console_exists()) {
        host_.log("LoadMacro string: \"%s\"\n");
    }
    holder->macro()->load_script_text(
        host_.ansi_text_from_bstr(script.bstr_value));
    return true;
}

std::uint32_t CSfcOLE::RequestMacro(scripting::MacroHolder* holder,
                                    OleScriptVariant* script) {
    log_if_debug("RequestMacro.\n");
    if (script == nullptr || script->type != OleVariantType::bstr ||
        script->bstr_value == nullptr || find_registered(holder) == nullptr ||
        holder->macro() == nullptr) {
        return 0;
    }

    holder->macro()->object_context.target_object = host_.selected_creature();
    const bool callback_succeeded = holder->invoke_dispatch_entry(script);
    if (host_.debug_console_exists()) {
        host_.log("RequestMacro string: \"%s\"\n");
    }
    return callback_succeeded ? 1u : 0u;
}

std::uint32_t CSfcOLE::ExecuteMacro(
    scripting::MacroHolder* holder, const OleScriptVariant& script) {
    log_if_debug("ExecuteMacro.\n");
    log_if_debug("LoadMacro.\n");
    if (script.type != OleVariantType::bstr || script.bstr_value == nullptr ||
        find_registered(holder) == nullptr || holder->macro() == nullptr) {
        return 0;
    }

    if (host_.debug_console_exists()) {
        host_.log("LoadMacro string: \"%s\"\n");
    }
    holder->macro()->load_script_text(
        host_.ansi_text_from_bstr(script.bstr_value));
    holder->macro()->object_context.target_object = host_.selected_creature();
    return holder->invoke_dispatch_entry(nullptr) ? 1u : 0u;
}

scripting::MacroHolder* CSfcOLE::CreateCommand(
    std::uint16_t execution_mode) {
    log_if_debug("CreateCommand.\n");
    if (execution_mode >= 5) {
        return nullptr;
    }

    auto holder = host_.create_macro_holder(
        static_cast<scripting::MacroExecutionMode>(execution_mode));
    if (holder == nullptr || holder->macro() == nullptr) {
        return nullptr;
    }

    scripting::MacroHolder* holder_address = holder.get();
    macro_holders_.insert(macro_holders_.begin(), std::move(holder));
    return holder_address;
}

bool CSfcOLE::DestroyCommand(scripting::MacroHolder* holder) {
    log_if_debug("DestroyMacro.\n");
    return remove_registered(holder);
}

void CSfcOLE::LoadCommand(scripting::MacroHolder* holder,
                          std::string_view command_text) {
    log_if_debug("LoadCommand.\n");
    if (find_registered(holder) == nullptr || holder->macro() == nullptr) {
        return;
    }

    if (host_.debug_console_exists()) {
        host_.log("    Command: %s\n");
    }
    holder->macro()->load_script_text(command_text);
}

std::uint32_t CSfcOLE::RequestCommand(scripting::MacroHolder* holder,
                                      wchar_t** output_bstr) {
    log_if_debug("RequestCommand.\n");
    if (find_registered(holder) == nullptr || holder->macro() == nullptr) {
        return 0;
    }

    std::array<char, 0x4000> command_buffer{};
    holder->macro()->object_context.script_owner = host_.selected_creature();
    holder->reset_result_and_invoke_result_entry(command_buffer.data());
    host_.assign_output_bstr(
        std::string_view(command_buffer.data()), output_bstr);
    return holder->callback_result();
}

bool CSfcOLE::FireCommand(std::uint16_t execution_mode,
                          std::string_view command_text,
                          wchar_t** output_bstr) {
    scripting::MacroHolder* holder = CreateCommand(execution_mode);
    if (holder == nullptr) {
        return false;
    }

    LoadCommand(holder, command_text);
    const std::uint32_t request_result = RequestCommand(holder, output_bstr);
    if (!DestroyCommand(holder)) {
        return false;
    }
    return (request_result & 1u) != 0;
}

} // namespace creatures1::application
