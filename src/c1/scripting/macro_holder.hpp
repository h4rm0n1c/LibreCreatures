#pragma once

#include <cstddef>
#include <cstdint>

#include "macro.hpp"

// The macro publish/report context is the automation VARIANT itself:
// C1MacroOutputPublishContextPrefix and C1BrainActivityReportCallbackContext
// are the same twelve-byte record, both carrying the output at offset 8.
namespace creatures1::application {
struct OleScriptVariant;
}

namespace creatures1::scripting {

class MacroHolder;

enum class MacroExecutionMode : std::uint32_t {
    start_execution = 0,
    execute_to_output = 1,
    format_brain_activity_report = 2,
    default_success_3 = 3,
    default_success_4 = 4,
};

class MacroHolderHost : public virtual MacroExecutionHost {
public:
    ~MacroHolderHost() override = default;

    virtual bool dispatch_start_macro_execution(MacroHolder& holder,
                                                 void* context) = 0;
    virtual bool dispatch_execute_to_output(MacroHolder& holder,
                                             void* context) = 0;
    virtual bool dispatch_default(MacroHolder& holder, void* context) = 0;

    // These are the two concrete operations used by the holder callbacks.
    // Scheduling/interpreter policy remains in the application host; the
    // callback selection and result carrier remain MacroHolder-owned.
    virtual bool start_macro(Macro& macro) = 0;
    virtual std::uint32_t execute_macro_to_output_buffer(
        Macro& macro, char* output_buffer) = 0;
    virtual void publish_macro_output(application::OleScriptVariant* context,
                                      const char* output,
                                      std::size_t output_length) = 0;

    virtual bool start_macro_execution(MacroHolder& holder,
                                       char* output_buffer) = 0;
    virtual bool execute_macro_to_output(MacroHolder& holder,
                                         char* output_buffer) = 0;
    virtual bool default_result(MacroHolder& holder,
                                char* output_buffer) = 0;
    virtual bool image_sequence_is_empty(MacroHolder& holder) = 0;

    // Returns the report buffer on success and nullptr on failure.  The
    // object-to-Brain conversion belongs to the concrete game host because
    // it depends on the recovered Creature layout, not base Object.
    virtual char* format_brain_activity_report(
        objects::Object* brain_object, char* output_buffer,
        std::uint32_t report_mode, std::uint32_t rule_index) = 0;
};

class MacroHolder {
public:
    MacroHolder(MacroExecutionMode execution_mode, MacroHolderHost& host);
    ~MacroHolder();

    bool invoke_dispatch_entry(void* callback_context);
    bool reset_result_and_invoke_result_entry(char* output_buffer);
    bool dispatch_start_macro_execution(void* callback_context);
    bool execute_and_publish_macro_output(void* callback_context);
    bool dispatch_format_brain_activity_report(
        void* callback_context);
    bool start_macro_execution(void* callback_context);
    bool execute_macro_to_output_buffer(char* output_buffer);
    bool format_brain_activity_report(char* output_buffer);
    bool set_zero_callback_result(void* callback_argument);

    // A started script that ends deletes its Macro; a kit that keeps its
    // holder then gets a fresh one, as native's reuse of the same object
    // effectively did (native kept a pointer to freed memory).
    Macro* macro() const {
        return macro_ != nullptr ? macro_
                                 : const_cast<MacroHolder*>(this)->adopt_new_macro();
    }

    // Called by ~Macro when a started script deletes itself.
    void forget_macro(const Macro& macro) {
        if (macro_ == &macro) {
            macro_ = nullptr;
        }
    }

    // For a caller whose holder dies straight after starting its script (the
    // pipe server): pass a still-running Macro to the scheduler, which then
    // deletes it when the script ends, instead of the holder's destructor
    // unscheduling and deleting a script it has only just started.
    void hand_started_macro_to_scheduler();
    std::uint32_t callback_result() const { return callback_result_; }

private:
    Macro* adopt_new_macro();

    MacroExecutionMode execution_mode_;
    MacroHolderHost& host_;
    Macro* macro_ = nullptr;
    std::uint32_t reserved_runtime_word_ = 0;
    std::uint32_t callback_result_ = 0;
};

} // namespace creatures1::scripting
