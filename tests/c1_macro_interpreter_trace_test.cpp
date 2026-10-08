#include "scripting/macro.hpp"
#include "scripting/classifier_scripts.hpp"
#include "scripting/tables.hpp"
#include "objects/object.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace c1 = creatures1;
using c1::objects::Object;
using c1::scripting::CaosToken;
using c1::scripting::Macro;
using c1::scripting::MacroClassifierPattern;
using c1::scripting::MacroCommandHost;
using c1::scripting::MacroControlFlowResult;
using c1::scripting::MacroCreatureAssignment;
using c1::scripting::MacroCreatureValue;
using c1::scripting::MacroInterpreterFinalization;
using c1::scripting::MacroInterpreterTrace;
using c1::scripting::MacroRuntimeHost;
using c1::scripting::MacroSyntaxDiagnostic;
using c1::scripting::MacroViewportValue;

namespace {

// This is deliberately a test-only null adapter. It proves that the outer
// interpreter and trace seam execute without smuggling a fake application or
// scheduler implementation into the clean source lane.
class NullRuntime : public MacroRuntimeHost {
public:
    // The trace host has no world, so every object value is refused; the
    // trace scripts pass objects only through context fields.
    bool is_live_object(const Object*) const override { return false; }
    bool is_creature_object(const Object&) const override { return false; }
    bool is_simple_object(const Object&) const override { return false; }
    bool is_compound_object(const Object&) const override { return false; }
    bool is_vehicle_object(const Object&) const override { return false; }
    std::int32_t vehicle_movement_vector_x(const Object&) const override {
        return 0;
    }
    std::int32_t vehicle_movement_vector_y(const Object&) const override {
        return 0;
    }
    void set_vehicle_movement_vector_x(Object&, std::int32_t) override {}
    void set_vehicle_movement_vector_y(Object&, std::int32_t) override {}
    std::uint32_t creature_value(const Object&, MacroCreatureValue,
                                 std::uint32_t) const override {
        return 0;
    }
    std::uint32_t attention_record_index(const Object&) const override {
        return 0;
    }
    std::int32_t ground_height(std::uint32_t) const override { return 0; }
    std::uint32_t room_count() const override { return 0; }
    std::int32_t room_value(std::uint32_t, std::uint32_t) const override {
        return 0;
    }
    std::int32_t ambient_temperature_at(const Object&) const override {
        return 0;
    }
    std::int8_t ambient_wind() const override { return 0; }
    std::uint32_t score_value(std::uint32_t) const override { return 0; }
    std::uint32_t world_tick_count() const override { return 0; }
    std::uint32_t language_version() const override { return 0; }
    std::uint32_t sound_settings() const override { return 0; }
    std::uint32_t viewport_value(MacroViewportValue) const override {
        return 0;
    }
    Object* selected_creature() const override { return nullptr; }
    Object* pointer_tool() const override { return nullptr; }
    Object* edit_object() const override { return nullptr; }
    Object* topmost_pointer_object() const override { return nullptr; }
    std::uint32_t enabled_object_count_matching(
        std::uint32_t, std::uint32_t, std::uint32_t) const override {
        return 0;
    }
    std::size_t non_scenery_object_count() const override { return 0; }
    Object* non_scenery_object_at(std::size_t) const override {
        return nullptr;
    }
    Object* random_non_scenery_object(
        const MacroClassifierPattern&) const override {
        return nullptr;
    }
    std::uint32_t select_creature_target_pose_for_motion(
        Object&, bool) override {
        return 0;
    }
    void clear_creature_selected_decision_neuron(Object&) override {}
    void select_creature_walk_gait(Object&) override {}
    bool creature_approach_is_ready(const Object&) const override {
        return false;
    }
    void reset_creature_animation_sequence(Object&) override {}
    void set_selected_creature(Object*, bool) override {}
    void set_edit_object(Object*) override {}
    void set_viewport_value(MacroViewportValue, std::uint32_t) override {}
    void set_creature_value(Object&, MacroCreatureAssignment,
                            std::uint32_t) override {}
    void add_creature_chemical_moles(Object&, std::uint32_t,
                                     std::uint32_t) override {}
    void fire_creature_neuron(Object&, std::int32_t, std::int32_t,
                              std::int32_t) override {}
    void set_creature_brain_neuron_activation(Object&, std::uint32_t,
                                              std::uint32_t,
                                              std::uint8_t) override {}
    void set_object_image_index(Object&, std::uint8_t,
                                std::int32_t) override {}
    void configure_simple_object_behavior(Object&, std::uint32_t,
                                          std::uint32_t) override {}
    bool set_object_relative_image_index(Object&, std::uint32_t,
                                          std::int32_t) override {
        return true;
    }
    char* preload_object_image_sequence(Object&, char*, const char*,
                                        std::int32_t) override {
        return nullptr;
    }
    void teleport_target_and_refresh_selection(Object*, std::int32_t,
                                               std::int32_t) override {}
    void drive_presentation_for_bound_creatures(Object&) override {}
    void process_creature_insemination(Object&) override {}
    void die_creature(Object&) override {}
    void initialize_object_runtime_state(Object&) override {}
    void set_creature_sleep_indicator(Object&, std::uint32_t) override {}
    void set_creature_dream_countdown(Object&, std::uint32_t) override {}
    void notify_creature_dependents_on_removal(Object&, bool) override {}
    void remove_object_from_event_bar(Object&, bool) override {}
    void add_object_to_event_bar(Object*) override {}
    void stop_creature_involuntary_action(Object&) override {}
    void set_creature_action_activation_boost(Object&,
                                              std::uint32_t) override {}
    void set_creature_involuntary_action_cooldown(Object&, std::uint32_t,
                                                  std::uint8_t) override {}
    void update_creature_bacterium(Object&) override {}
    void initialize_creature_default_vocabulary(Object&) override {}
    void set_compound_part_bounds(Object&, std::uint32_t, std::int32_t,
                                  std::int32_t, std::int32_t,
                                  std::int32_t) override {}
    void set_compound_knob_function(Object&, std::uint32_t,
                                    std::uint32_t) override {}
    void set_vehicle_creature_event_bounds(Object&, std::int32_t,
                                           std::int32_t, std::int32_t,
                                           std::int32_t) override {}
    void grab_vehicle_passengers(Object&) override {}
    void set_object_bounds_mode(Object&, std::uint32_t) override {}
    void update_object_movement_bounds(Object&) override {}
    void note_egg_state_change() override {}
};

class NullDiagnostics final : public MacroCommandHost {
public:
    void report_syntax_error(Macro&, const MacroSyntaxDiagnostic&) override {}
};

class ThrowingDiagnostics final : public MacroCommandHost {
public:
    void report_syntax_error(Macro&, const MacroSyntaxDiagnostic&) override {
        throw std::runtime_error("test syntax exception");
    }
};

class RecordingExceptions final : public c1::scripting::MacroExceptionHost {
public:
    void report_execution_exception(
        Macro&, const c1::scripting::MacroExecutionExceptionDiagnostic& diagnostic)
        override {
        ++reports;
        last_token = diagnostic.offending_token;
    }

    std::size_t reports = 0;
    CaosToken last_token = 0;
};

class NullExecutionHost final : public c1::scripting::MacroExecutionHost {
public:
    Object* selected_creature() const override { return nullptr; }
    Object* initial_auxiliary_object() const override { return nullptr; }
    Object* resolve_it_object(Object*) const override { return nullptr; }
};

class TestInterpreterHost final : public c1::scripting::MacroInterpreterHost {
public:
    c1::scripting::MacroInterpreterBindings bindings;

    c1::scripting::MacroInterpreterBindings interpreter_bindings() override {
        ++binding_requests;
        return bindings;
    }
    void report_too_many_macros(Macro&, std::size_t) override {
        ++too_many_reports;
    }

    std::size_t binding_requests = 0;
    std::size_t too_many_reports = 0;
};

class Trace final : public MacroInterpreterTrace {
public:
    void command_fetched(Macro&, CaosToken token,
                         std::size_t command_start) override {
        ++fetches;
        last_token = token;
        last_command_start = command_start;
    }
    void command_result(Macro&, CaosToken token, MacroControlFlowResult result,
                        std::size_t cursor_after_dispatch) override {
        ++results;
        last_token = token;
        last_result = result;
        last_cursor = cursor_after_dispatch;
    }
    void iteration_finalized(Macro&, MacroInterpreterFinalization finalization,
                             std::size_t cursor_after_finalization) override {
        ++finalizations;
        last_finalization = finalization;
        last_cursor = cursor_after_finalization;
    }
    void execution_exception(Macro&) override { ++exceptions; }
    void scheduler_action(Macro&, MacroInterpreterFinalization action) override {
        ++scheduler_actions;
        last_scheduler_action = action;
    }

    std::size_t fetches = 0;
    std::size_t results = 0;
    std::size_t finalizations = 0;
    std::size_t exceptions = 0;
    std::size_t scheduler_actions = 0;
    CaosToken last_token = 0;
    std::size_t last_command_start = 0;
    std::size_t last_cursor = 0;
    MacroControlFlowResult last_result =
        MacroControlFlowResult::execution_terminated;
    MacroInterpreterFinalization last_finalization =
        MacroInterpreterFinalization::continue_dispatch;
    MacroInterpreterFinalization last_scheduler_action =
        MacroInterpreterFinalization::continue_dispatch;
};

void run_endm_trace_probe() {
    NullRuntime runtime;
    NullDiagnostics diagnostics;
    Trace trace;
    Macro macro;
    macro.capture_output_enabled = true;
    macro.load_script_text("endm");

    c1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = &runtime;
    bindings.diagnostics = &diagnostics;
    bindings.trace = &trace;
    const MacroControlFlowResult result = macro.execute_interpreter(bindings);

    assert(result == MacroControlFlowResult::scheduler_cleanup_required);
    assert(trace.fetches == 1);
    assert(trace.results == 1);
    assert(trace.finalizations == 0);
    assert(trace.exceptions == 0);
    assert(trace.scheduler_actions == 1);
    assert(trace.last_command_start == 0);
    assert(trace.last_result ==
           MacroControlFlowResult::scheduler_cleanup_required);
    assert(trace.last_scheduler_action ==
           MacroInterpreterFinalization::remove_from_scheduler_and_retain);
    std::printf("trace_probe case=endm fetches=%zu results=%zu finalizations=%zu exceptions=%zu scheduler_actions=%zu result=%u\n",
                trace.fetches, trace.results, trace.finalizations,
                trace.exceptions, trace.scheduler_actions,
                static_cast<unsigned>(result));
}

void run_wait_trace_probe() {
    NullRuntime runtime;
    NullDiagnostics diagnostics;
    Trace trace;
    Macro macro;
    macro.capture_output_enabled = true;

    // The clean source stores one five-byte command record at a time: four
    // command bytes followed by its separator/record byte.  Install the
    // exact packed bytes so this probe measures wait's native cursor rewind;
    // the existing endm probe separately covers scheduler termination.
    macro.script_buffer = "wait,2";
    macro.script_capacity_bytes =
        static_cast<std::uint32_t>(macro.script_buffer.size());

    c1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = &runtime;
    bindings.diagnostics = &diagnostics;
    bindings.trace = &trace;
    const MacroControlFlowResult result = macro.execute_interpreter(bindings);

    assert(result == MacroControlFlowResult::scheduler_cleanup_required);
    assert(trace.fetches == 2);
    assert(trace.results == 2);
    assert(trace.finalizations == 3);
    assert(trace.exceptions == 0);
    assert(trace.scheduler_actions == 1);
    assert(trace.last_command_start == 0);
    assert(trace.last_result ==
           MacroControlFlowResult::iteration_complete);
    assert(trace.last_finalization ==
           MacroInterpreterFinalization::remove_from_scheduler_and_retain);
    assert(trace.last_scheduler_action ==
           MacroInterpreterFinalization::remove_from_scheduler_and_retain);
    std::printf("trace_probe case=wait_rewind fetches=%zu results=%zu finalizations=%zu exceptions=%zu scheduler_actions=%zu result=%u\n",
                trace.fetches, trace.results, trace.finalizations,
                trace.exceptions, trace.scheduler_actions,
                static_cast<unsigned>(result));
}

void run_anim_scheduler_boundary_probe() {
    NullRuntime runtime;
    NullDiagnostics diagnostics;
    Trace trace;
    Macro macro;
    macro.load_script_text("anim [01R]");

    c1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = &runtime;
    bindings.diagnostics = &diagnostics;
    bindings.trace = &trace;
    const MacroControlFlowResult result = macro.execute_interpreter(bindings);

    // ANIM has a target-independent parser path even with the null target
    // used by this test. Native finalization returns to the world scheduler
    // after installing the sequence; it must not dispatch another command in
    // this same interpreter pass.
    assert(result == MacroControlFlowResult::interpreter_returned);
    assert(trace.fetches == 1);
    assert(trace.results == 1);
    assert(trace.last_result == MacroControlFlowResult::cursor_changed);
    assert(trace.finalizations == 1);
    assert(trace.last_finalization ==
           MacroInterpreterFinalization::return_to_caller);
    std::printf("trace_probe case=anim_scheduler_boundary fetches=%zu results=%zu finalizations=%zu result=%u\n",
                trace.fetches, trace.results, trace.finalizations,
                static_cast<unsigned>(result));
}

void run_exception_trace_probe() {
    NullRuntime runtime;
    ThrowingDiagnostics diagnostics;
    RecordingExceptions exceptions;
    Trace trace;
    Macro macro;
    macro.capture_output_enabled = true;
    macro.script_buffer = "xxxx";
    macro.script_capacity_bytes = 5;

    c1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = &runtime;
    bindings.diagnostics = &diagnostics;
    bindings.exceptions = &exceptions;
    bindings.trace = &trace;
    const MacroControlFlowResult result = macro.execute_interpreter(bindings);

    assert(result == MacroControlFlowResult::scheduler_cleanup_required);
    assert(trace.fetches == 1);
    assert(trace.results == 1);
    assert(trace.finalizations == 1);
    assert(trace.exceptions == 1);
    assert(trace.scheduler_actions == 1);
    assert(trace.last_result ==
           MacroControlFlowResult::execution_terminated);
    assert(trace.last_finalization ==
           MacroInterpreterFinalization::remove_from_scheduler_and_retain);
    assert(exceptions.reports == 1);
    assert(exceptions.last_token != 0);
    std::printf("trace_probe case=exception_cleanup fetches=%zu results=%zu finalizations=%zu exceptions=%zu scheduler_actions=%zu result=%u reports=%zu\n",
                trace.fetches, trace.results, trace.finalizations,
                trace.exceptions, trace.scheduler_actions,
                static_cast<unsigned>(result), exceptions.reports);
}

void run_scheduler_adapter_output_probe() {
    NullRuntime runtime;
    NullDiagnostics diagnostics;
    Trace trace;
    NullExecutionHost execution_host;
    TestInterpreterHost interpreter_host;
    interpreter_host.bindings.runtime = &runtime;
    interpreter_host.bindings.diagnostics = &diagnostics;
    interpreter_host.bindings.trace = &trace;
    c1::scripting::MacroSchedulerHostAdapter scheduler_host(
        execution_host, interpreter_host);

    Macro macro;
    macro.load_script_text("endm");
    std::string output = "stale output";
    const std::size_t output_length =
        macro.execute_to_output_buffer(scheduler_host, output);

    assert(output_length == 0);
    assert(output.empty());
    assert(interpreter_host.binding_requests == 1);
    assert(trace.fetches == 1);
    assert(trace.scheduler_actions == 1);
    std::printf("trace_probe case=scheduler_adapter_output fetches=%zu results=%zu finalizations=%zu exceptions=%zu scheduler_actions=%zu output_length=%zu\n",
                trace.fetches, trace.results, trace.finalizations,
                trace.exceptions, trace.scheduler_actions, output_length);
}

void run_scheduler_adapter_start_probe() {
    NullRuntime runtime;
    NullDiagnostics diagnostics;
    Trace trace;
    NullExecutionHost execution_host;
    TestInterpreterHost interpreter_host;
    interpreter_host.bindings.runtime = &runtime;
    interpreter_host.bindings.diagnostics = &diagnostics;
    interpreter_host.bindings.trace = &trace;
    c1::scripting::MacroSchedulerHostAdapter scheduler_host(
        execution_host, interpreter_host);

    Macro* macro = new Macro;
    macro->load_script_text("endm");
    const bool started = macro->start_execution(scheduler_host);

    assert(started);
    assert(c1::scripting::g_running_macros.empty());
    assert(interpreter_host.binding_requests == 1);
    assert(trace.fetches == 1);
    assert(trace.scheduler_actions == 1);
    std::printf("trace_probe case=scheduler_adapter_start fetches=%zu results=%zu finalizations=%zu exceptions=%zu scheduler_actions=%zu started=%u\n",
                trace.fetches, trace.results, trace.finalizations,
                trace.exceptions, trace.scheduler_actions,
                started ? 1u : 0u);
}

// A runtime whose one target object is a live Creature, counting how many
// times the interpreter hands it to insemination.
class InseminationRuntime final : public NullRuntime {
public:
    explicit InseminationRuntime(const Object& creature) : creature_(creature) {}
    bool is_live_object(const Object* object) const override {
        return object == &creature_;
    }
    bool is_creature_object(const Object& object) const override {
        return &object == &creature_;
    }
    void process_creature_insemination(Object& object) override {
        assert(&object == &creature_);
        ++inseminations;
    }
    std::size_t inseminations = 0;

private:
    const Object& creature_;
};

// MATE and its hidden F**K synonym share one native handler
// (ExecuteInterpreter @ 0x0041f0b6).  This goes through the full interpreter
// with the literal token, because F**K was once classified into a command
// family with no handler for it: every world mating script died at F**K
// before insemination, and a direct execute_mate_command() test could not
// see that.
void run_mate_synonym_dispatch_probe(const char* script_text) {
    Object creature;
    InseminationRuntime runtime(creature);
    NullDiagnostics diagnostics;
    Trace trace;
    Macro macro;
    macro.capture_output_enabled = true;
    macro.load_script_text(script_text);
    macro.object_context.target_object = &creature;

    c1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = &runtime;
    bindings.diagnostics = &diagnostics;
    bindings.trace = &trace;
    const MacroControlFlowResult result = macro.execute_interpreter(bindings);

    assert(result == MacroControlFlowResult::scheduler_cleanup_required);
    assert(runtime.inseminations == 1);
    std::printf("trace_probe case=mate_synonym script=%s inseminations=%zu result=%u\n",
                script_text, runtime.inseminations,
                static_cast<unsigned>(result));
}

// A runtime whose one target is a live Creature; records whether each
// `drop` asked for the empty-handed Disappointment fallback.
class DropRuntime final : public NullRuntime {
public:
    explicit DropRuntime(const Object& creature) : creature_(creature) {}
    bool is_live_object(const Object* object) const override {
        return object == &creature_;
    }
    bool is_creature_object(const Object& object) const override {
        return &object == &creature_;
    }
    void notify_creature_dependents_on_removal(Object& object,
                                               bool disappoint_when_empty) override {
        assert(&object == &creature_);
        disappointments.push_back(disappoint_when_empty);
    }
    std::vector<bool> disappointments;

private:
    const Object& creature_;
};

// Holds whatever Macro::serialize writes, in order, and plays it back.
class MemoryMacroArchive final : public c1::scripting::MacroArchive {
public:
    bool loading() const override { return loading_; }
    void start_loading() { loading_ = true; cursor_ = 0; }
    std::uint32_t read_uint32() override { return std::get<std::uint32_t>(values_[cursor_++]); }
    std::int32_t read_int32() override { return std::get<std::int32_t>(values_[cursor_++]); }
    std::string read_string() override { return std::get<std::string>(values_[cursor_++]); }
    Object* read_object() override { return std::get<Object*>(values_[cursor_++]); }
    void write_uint32(std::uint32_t value) override { values_.emplace_back(value); }
    void write_int32(std::int32_t value) override { values_.emplace_back(value); }
    void write_string(std::string_view value) override { values_.emplace_back(std::string(value)); }
    void write_object(Object* object) override { values_.emplace_back(object); }

private:
    bool loading_ = false;
    std::size_t cursor_ = 0;
    std::vector<std::variant<std::uint32_t, std::int32_t, std::string, Object*>> values_;
};

void set_scriptorium(std::initializer_list<std::pair<std::uint8_t, const char*>> scripts) {
    std::size_t index = 0;
    for (const auto& [family, text] : scripts) {
        c1::scripting::ScriptDefinitionEntry& entry =
            c1::scripting::g_script_definition_entries[index++];
        entry.script_text = text;
        entry.classifier_event = {};
        entry.classifier_event.family = family;
    }
    c1::scripting::g_script_definition_count = index;
}

// Runs `drop,endm` as a creature's macro, either straight through or after
// a save and load, and returns whether drop asked for Disappointment.  The
// started macro's provenance is what execute_script_for_classifier sets.
bool drop_disappoints(const char* text, bool started_as_object_script,
                      bool save_and_load) {
    Object creature;
    creature.set_classifier_base(0x04010000u);   // a norn
    DropRuntime runtime(creature);
    NullDiagnostics diagnostics;
    Trace trace;

    Macro started;
    started.load_script_text(text);
    started.object_context.script_owner = &creature;
    started.object_context.target_object = &creature;
    started.creature_runs_object_script = started_as_object_script;

    Macro loaded;
    Macro* running = &started;
    if (save_and_load) {
        MemoryMacroArchive archive;
        started.serialize(archive);
        archive.start_loading();
        loaded.serialize(archive);
        running = &loaded;
    }

    c1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = &runtime;
    bindings.diagnostics = &diagnostics;
    bindings.trace = &trace;
    running->execute_interpreter(bindings);
    assert(runtime.disappointments.size() == 1);
    return runtime.disappointments[0];
}

// An empty-handed creature's `drop` disappoints it only in its own scripts,
// not in the script another object gives it to push or pull that object --
// and a saved world must keep that distinction after it is loaded.
void run_drop_disappointment_save_load_probe() {
    set_scriptorium({{2, "drop,endm"}, {4, "drop,wait 1,endm"}});

    // Food's push script, run straight through and after a save/load.
    assert(!drop_disappoints("drop,endm", true, false));
    assert(!drop_disappoints("drop,endm", true, true));
    // The creature's own script disappoints, before and after.
    assert(drop_disappoints("drop,wait 1,endm", false, false));
    assert(drop_disappoints("drop,wait 1,endm", false, true));
    // CAOS from the pipe or the injector is in no scriptorium entry.
    assert(drop_disappoints("drop,stop,endm", false, true));
    // A running `appr` rewrites itself to `APPR`; case does not matter.
    set_scriptorium({{2, "drop,appr,endm"}});
    assert(!drop_disappoints("drop,APPR,endm", true, true));
    set_scriptorium({});
    std::printf("trace_probe case=drop_disappointment_save_load ok\n");
    std::fflush(stdout);
}

} // namespace

int main() {
    run_drop_disappointment_save_load_probe();
    run_mate_synonym_dispatch_probe("mate,endm");
    run_mate_synonym_dispatch_probe("f**k,endm");
    run_endm_trace_probe();
    run_wait_trace_probe();
    run_anim_scheduler_boundary_probe();
    run_exception_trace_probe();
    run_scheduler_adapter_output_probe();
    run_scheduler_adapter_start_probe();
    return 0;
}
