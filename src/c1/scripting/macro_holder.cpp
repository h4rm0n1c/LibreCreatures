#include "macro_holder.hpp"

#include "../application/sfc_ole.hpp"

#include <algorithm>
#include <array>

namespace creatures1::scripting {

MacroHolder::MacroHolder(MacroExecutionMode execution_mode,
                         MacroHolderHost& host)
    : execution_mode_(execution_mode), host_(host) {
    adopt_new_macro();
}

Macro* MacroHolder::adopt_new_macro() {
    macro_ = new Macro;
    macro_->owning_holder = this;
    macro_->object_context.script_owner = host_.selected_creature();
    macro_->object_context.from_object = nullptr;
    macro_->object_context.exec_object = host_.initial_auxiliary_object();
    macro_->reset_execution_state(host_);
    return macro_;
}

MacroHolder::~MacroHolder() {
    if (macro_ != nullptr) {
        macro_->owning_holder = nullptr;
        // Macro release may destroy the Macro or leave it scheduler-owned;
        // either way the holder no longer owns the pointer afterward.
        macro_->remove_from_running_scheduler_and_release();
        macro_ = nullptr;
    }
}

void MacroHolder::hand_started_macro_to_scheduler() {
    // LibreCreatures deviation.  Native CMacroHolder keeps its Macro after
    // StartExecution and its destructor releases it, which only works while
    // the holder outlives the script.  The pipe server's holder dies as soon
    // as the command returns, so a running Macro passes to the scheduler and
    // deletes itself when its script ends.
    if (macro_ == nullptr ||
        std::find(g_running_macros.begin(), g_running_macros.end(), macro_) ==
            g_running_macros.end()) {
        return;  // finished already, or never scheduled: the holder keeps it
    }
    macro_->owning_holder = nullptr;
    macro_->destroy_when_finished = true;
    macro_ = nullptr;
}

bool MacroHolder::invoke_dispatch_entry(void* callback_context) {
    if (macro() == nullptr) {
        return false;
    }

    macro()->object_context.script_owner = host_.selected_creature();
    switch (execution_mode_) {
    case MacroExecutionMode::start_execution:
        return dispatch_start_macro_execution(callback_context);
    case MacroExecutionMode::execute_to_output:
        return execute_and_publish_macro_output(callback_context);
    case MacroExecutionMode::format_brain_activity_report:
        return dispatch_format_brain_activity_report(callback_context);
    case MacroExecutionMode::default_success_3:
    case MacroExecutionMode::default_success_4:
        return host_.dispatch_default(*this, callback_context);
    }
    return false;
}

bool MacroHolder::reset_result_and_invoke_result_entry(char* output_buffer) {
    callback_result_ = 0;
    if (macro() == nullptr) {
        return false;
    }

    macro()->object_context.script_owner = host_.selected_creature();
    switch (execution_mode_) {
    case MacroExecutionMode::start_execution:
        return start_macro_execution(output_buffer);
    case MacroExecutionMode::execute_to_output:
        return execute_macro_to_output_buffer(output_buffer);
    case MacroExecutionMode::format_brain_activity_report:
        return format_brain_activity_report(output_buffer);
    case MacroExecutionMode::default_success_3:
    case MacroExecutionMode::default_success_4:
        return host_.default_result(*this, output_buffer);
    }
    return false;
}

bool MacroHolder::dispatch_start_macro_execution(void* callback_context) {
    (void)callback_context;
    // The native callback ignores its explicit argument and always reports
    // success after handing the owned Macro to the scheduler.
    host_.start_macro(*macro());
    return true;
}

bool MacroHolder::execute_and_publish_macro_output(void* callback_context) {
    if (macro() == nullptr) {
        return false;
    }

    std::array<char, 0x4000> output_buffer{};
    const std::uint32_t output_length =
        host_.execute_macro_to_output_buffer(*macro(), output_buffer.data());
    if (output_length != 0) {
        host_.publish_macro_output(
            static_cast<application::OleScriptVariant*>(callback_context),
            output_buffer.data(), output_length);
    }
    return true;
}

bool MacroHolder::dispatch_format_brain_activity_report(
    void* callback_context) {
    if (macro() == nullptr) {
        return false;
    }

    // CMacroHolder::DispatchFormatBrainActivityReport @ 0x00419400 writes the
    // report into the caller's own buffer -- report_context->output_buffer,
    // which is the VARIANT's value field.  Passing null here wrote the report
    // nowhere.
    auto* context =
        static_cast<application::OleScriptVariant*>(callback_context);
    char* const output_buffer =
        context == nullptr ? nullptr : context->bstr_value;
    run_report_script();
    char* result = host_.format_brain_activity_report(
        macro()->object_context.target_object, output_buffer,
        macro()->caos_work_values[0], macro()->caos_work_values[1]);
    return result != nullptr;
}

void MacroHolder::run_report_script() {
    // LibreCreatures deviation: the fix for a long-standing bug.  A brain
    // report measures what its Macro's first two work values say (firing
    // strength, activation, strongest weight, average target weight or
    // average dendrite state, and the dendrite rule), and the kits set them
    // with the script they load: the Science and Health Kits send
    // `inst,setv var0 1,endm` for activation.  Over DDE that worked, since a
    // conversation had one Macro and XTYP_EXECUTE ran its script before
    // XTYP_REQUEST took the report (DDEService::DdeCallback @ 0x0040fdb0).  Over SFC.OLE
    // nothing runs a report holder's script -- LoadMacro only stores it
    // (Macro::LoadScriptText @ 0x0041a280) and RequestMacro goes straight to
    // DispatchFormatBrainActivityReport @ 0x00419400 -- so the values stayed
    // 0 and every report was of firing strength.  Running the loaded script
    // first, to completion and unscheduled as a mode 1 holder's is, makes
    // them take.  A kit that loads nothing gets the native report.
    if (macro() == nullptr || macro()->script_buffer.empty()) {
        return;
    }
    host_.execute_macro_to_output_buffer(*macro(), nullptr);
}

bool MacroHolder::start_macro_execution(void* callback_context) {
    (void)callback_context;
    host_.start_macro(*macro());
    callback_result_ = 0;
    return true;
}

bool MacroHolder::execute_macro_to_output_buffer(char* output_buffer) {
    if (macro() == nullptr) {
        return false;
    }

    callback_result_ = host_.execute_macro_to_output_buffer(
        *macro(), output_buffer);
    return true;
}

bool MacroHolder::format_brain_activity_report(char* output_buffer) {
    if (macro() == nullptr) {
        return false;
    }

    run_report_script();
    char* result = host_.format_brain_activity_report(
        macro()->object_context.target_object, output_buffer,
        macro()->caos_work_values[0], macro()->caos_work_values[1]);
    // The report writer answers the end of what it wrote.  The result every
    // consumer wants is the length, so carry that rather than an address --
    // the pipe reports this value as its byte count, and a raw pointer there
    // reads as a nonsense length.
    callback_result_ =
        result == nullptr
            ? 0
            : static_cast<std::uint32_t>(result - output_buffer);
    return result != nullptr;
}

bool MacroHolder::set_zero_callback_result(void* callback_argument) {
    (void)callback_argument;
    callback_result_ = 0;
    return true;
}

} // namespace creatures1::scripting
