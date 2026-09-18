#include <cstdio>
#include <cstring>
#include "windows_macro_host.hpp"
#include <cstdlib>

#include "windows_object_event_host.hpp"

#include "windows_creature_hosts.hpp"

#include "../application/sfc_ole.hpp"
#include "../common/logging.hpp"
#include "../scripting/classifier_scripts.hpp"

namespace creatures1::platform {
namespace {

// The recovered exception report writes to the debug console under this
// category before it shows its modal box.
constexpr std::uint32_t kMacroDebugCategory = 0x2;

std::string format_classifier(
    const creatures1::scripting::ScriptClassifier& classifier) {
    char buffer[64] = {};
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%u, %u, %u",
                static_cast<unsigned>(classifier.family),
                static_cast<unsigned>(classifier.genus),
                static_cast<unsigned>(classifier.species));
    return buffer;
}

} // namespace

// --- MacroExecutionHost ----------------------------------------------------

creatures1::objects::Object* WindowsMacroHost::selected_creature() const {
    creatures1::creatures::Creature* creature = document_.selected_creature();
    return creature == nullptr
               ? nullptr
               : &document_.object_for_creature(*creature);
}

creatures1::objects::Object* WindowsMacroHost::initial_auxiliary_object()
    const {
    // The native seeds exec_object with the pointer tool.
    return document_.pointer_tool();
}

creatures1::objects::Object* WindowsMacroHost::resolve_it_object(
    creatures1::objects::Object* script_owner) const {
    // Macro::ResetExecutionState @ 0041a130: an owner whose family byte is 4
    // (a Creature) supplies IT from its motion link (+0x7f0), the object it is
    // attending to; every other owner gets null.  Creature action scripts
    // address the attended object as `_it_` (`mesg writ _it_ ...`), so a null
    // IT silently dropped every push/pull/get/eat message.
    if (script_owner == nullptr ||
        (script_owner->classifier_base() & 0xff000000U) != 0x04000000U) {
        return nullptr;
    }
    const creatures1::creatures::Creature* creature =
        document_.creature_for_object(*script_owner);
    return creature == nullptr ? nullptr : creature->skeleton().motion_link;
}

// --- MacroInterpreterHost --------------------------------------------------

creatures1::scripting::MacroInterpreterBindings
WindowsMacroHost::interpreter_bindings() {
    creatures1::scripting::MacroInterpreterBindings bindings;
    bindings.runtime = this;
    bindings.diagnostics = this;
    bindings.exceptions = this;
    bindings.dde = this;
    bindings.object_motion = this;
    // C1WindowsDocument already owns the script-definition table policy.
    bindings.scripts = &document_;
    bindings.object_events = this;
    bindings.message = this;
    bindings.stimulus = this;
    bindings.system = this;
    bindings.blackboard = this;
    bindings.new_object = &new_object_;
    bindings.debug = this;
    bindings.application = this;
    bindings.sound = this;
    bindings.sound_policy = this;
    bindings.speech = this;
    // Every other family stays null on purpose.  The interpreter checks each
    // pointer before dispatching, so an unbound family reports an unsupported
    // command instead of faulting, and the gap stays visible here rather than
    // being hidden behind a callback that silently does nothing.
    return bindings;
}

void WindowsMacroHost::report_too_many_macros(
    creatures1::scripting::Macro& macro, std::size_t maximum_macros) {
    static_cast<void>(macro);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, kMacroDebugCategory,
            "Macro: refusing to start, %u already running\n",
            static_cast<unsigned>(maximum_macros));
    }
}

// --- MacroCommandHost / MacroExceptionHost ---------------------------------

void WindowsMacroHost::report_syntax_error(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::MacroSyntaxDiagnostic& diagnostic) {
    static_cast<void>(macro);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console == nullptr) {
        return;
    }
    creatures1::common::debug_log(
        *console, kMacroDebugCategory,
        "Macro syntax error: expected %s near \"%s\"\n",
        diagnostic.expected_token_type.c_str(),
        diagnostic.script_excerpt.c_str());
}

void WindowsMacroHost::report_execution_exception(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::MacroExecutionExceptionDiagnostic&
        diagnostic) {
    static_cast<void>(macro);
    const std::string classifier =
        format_classifier(diagnostic.owning_classifier);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, kMacroDebugCategory,
            "Macro execution fault in script %s near \"%s\"\n",
            classifier.c_str(), diagnostic.script_excerpt.c_str());
    }
    // Held: the recovered report also stops timer 1, shows a modal
    // AfxMessageBox and re-arms the timer with a normalised [1,300] interval.
    // The modal box is deliberately not shown yet - it would block the world
    // update loop on every faulting script while the command families are
    // still unbound, turning a diagnostic into a hang.
}

// --- MacroHolderHost -------------------------------------------------------

bool WindowsMacroHost::start_macro(creatures1::scripting::Macro& macro) {
    creatures1::scripting::MacroSchedulerHostAdapter scheduler(*this, *this);
    return macro.start_execution(scheduler);
}

std::uint32_t WindowsMacroHost::execute_macro_to_output_buffer(
    creatures1::scripting::Macro& macro, char* output_buffer) {
    creatures1::scripting::MacroSchedulerHostAdapter scheduler(*this, *this);
    std::string output;
    const std::size_t written = macro.execute_to_output_buffer(scheduler,
                                                               output);
    if (output_buffer != nullptr) {
        // The native writes into a caller-owned fixed buffer; the recovered
        // pipe protocol caps a response at 0x1000 bytes.
        constexpr std::size_t kOutputCapacity = 0x1000;
        const std::size_t copied =
            (std::min)(output.size(), kOutputCapacity - 1);
        std::memcpy(output_buffer, output.data(), copied);
        output_buffer[copied] = '\0';
    }
    return static_cast<std::uint32_t>(written);
}

bool WindowsMacroHost::dispatch_start_macro_execution(
    creatures1::scripting::MacroHolder& holder, void* context) {
    static_cast<void>(context);
    return holder.macro() != nullptr && start_macro(*holder.macro());
}

bool WindowsMacroHost::dispatch_execute_to_output(
    creatures1::scripting::MacroHolder& holder, void* context) {
    return holder.execute_and_publish_macro_output(context);
}

bool WindowsMacroHost::dispatch_default(
    creatures1::scripting::MacroHolder& holder, void* context) {
    static_cast<void>(holder);
    static_cast<void>(context);
    // Execution modes 3 and 4 are recovered as unconditional success.
    return true;
}

void WindowsMacroHost::publish_macro_output(
    creatures1::application::OleScriptVariant* context, const char* output,
    std::size_t output_length) {
    // CMacroHolder::ExecuteAndPublishMacroOutput @ 0x00419340 frees whatever
    // BSTR the caller handed in and replaces it with a fresh byte-length one
    // holding exactly the bytes produced:
    //     SysFreeString(ctx->output_bstr);
    //     ctx->output_bstr = SysAllocStringByteLen(buffer, output_length);
    // A byte-length BSTR is what the caller allocated too -- CKitSheet
    // ::ConnectToCreatures builds its 0x1000-byte command buffer with
    // SysAllocStringByteLen -- so the length prefix stays valid and the
    // automation marshaller can carry the reply back across the process
    // boundary.  Writing the bytes anywhere else leaves the caller with the
    // script it sent, and treating this pointer as anything but the VARIANT it
    // is faults the server.
    if (context == nullptr || output == nullptr) {
        return;
    }
    ::SysFreeString(reinterpret_cast<BSTR>(context->bstr_value));
    context->bstr_value = reinterpret_cast<char*>(::SysAllocStringByteLen(
        output, static_cast<UINT>(output_length)));
}

bool WindowsMacroHost::start_macro_execution(
    creatures1::scripting::MacroHolder& holder, char* output_buffer) {
    static_cast<void>(output_buffer);
    return holder.macro() != nullptr && start_macro(*holder.macro());
}

bool WindowsMacroHost::execute_macro_to_output(
    creatures1::scripting::MacroHolder& holder, char* output_buffer) {
    return holder.macro() != nullptr &&
           execute_macro_to_output_buffer(*holder.macro(), output_buffer) != 0;
}

bool WindowsMacroHost::default_result(
    creatures1::scripting::MacroHolder& holder, char* output_buffer) {
    static_cast<void>(holder);
    static_cast<void>(output_buffer);
    return true;
}

bool WindowsMacroHost::image_sequence_is_empty(
    creatures1::scripting::MacroHolder& holder) {
    static_cast<void>(holder);
    // Held: the recovered check reads the owning object's parsed `anim`
    // sequence, which Entity owns and MacroHolder does not carry a route to.
    return true;
}

char* WindowsMacroHost::format_brain_activity_report(
    creatures1::objects::Object* brain_object, char* output_buffer,
    std::uint32_t report_mode, std::uint32_t rule_index) {
    // CreateBrainActivityData @ 0x004101e0 reaches the target object's CBrain
    // and calls FormatActivityReport with the macro's first two work values as
    // the report mode and the rule index.  Both halves are reachable now: the
    // object-to-creature conversion is the same one the rest of this host
    // uses, and Brain::format_activity_report is the report writer.
    if (brain_object == nullptr || output_buffer == nullptr) {
        return nullptr;
    }
    creatures1::creatures::Creature* creature = creature_of(*brain_object);
    if (creature == nullptr) {
        return nullptr;
    }
    creatures1::brain::Brain* brain = creature->brain();
    if (brain == nullptr) {
        return nullptr;
    }
    // The five recovered modes are firing strength, activation, maximum
    // current weight, average target weight and average dendrite state; a
    // value outside that set is not a mode the writer knows.
    constexpr std::uint32_t kHighestReportMode = 4;
    if (report_mode > kHighestReportMode) {
        return nullptr;
    }
    // Brain::format_activity_report answers the byte count it wrote,
    // terminator included; FormatActivityReport answers the end of what it
    // wrote, so callers can take the length as end - start.  Return the end.
    const std::size_t written = brain->format_activity_report(
        output_buffer,
        static_cast<creatures1::brain::ActivityReportMode>(report_mode),
        static_cast<int>(rule_index));
    return output_buffer + written;
}


// --- MacroRuntimeHost ------------------------------------------------------
//
// The interpreter checks every family pointer before dispatch, so publishing
// this host makes the runtime-owning commands reachable.  Methods that cannot
// yet reach their recovered policy say so and return the native failure value
// rather than pretending to succeed.

namespace {

// A held runtime command.  The native reports nothing here; the port records
// the gap on the debug console so an unimplemented CAOS command is visible
// while running rather than silently doing nothing.
void report_unbound_runtime_command(const char* command) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x2,
                                      "CAOS %s: runtime binding missing\n",
                                      command);
    }
}

} // namespace

creatures1::creatures::Creature* WindowsMacroHost::creature_of(
    creatures1::objects::Object& object) const {
    return document_.mutable_creature_for_object(object);
}

bool WindowsMacroHost::is_live_object(
    const creatures1::objects::Object* object) const {
    if (object == nullptr || document_.is_live_object(object)) {
        return object != nullptr;
    }
    // Loud on purpose.  A rejection here is either a script handing the
    // interpreter a bogus pointer -- which the original would have followed
    // into a fault -- or this check disagreeing with the world's registries,
    // which would be a defect in the check itself.  Either way it must be
    // visible rather than silently changing what a script does.
    if (FILE* log = std::fopen("Creatures.object.log", "a")) {
        std::fprintf(log, "rejected object value %p\n",
                     static_cast<const void*>(object));
        std::fclose(log);
    }
    return false;
}

bool WindowsMacroHost::is_creature_object(
    const creatures1::objects::Object& object) const {
    return document_.creature_for_object(object) != nullptr;
}

bool WindowsMacroHost::is_simple_object(
    const creatures1::objects::Object& object) const {
    return dynamic_cast<const creatures1::objects::SimpleObject*>(&object) !=
           nullptr;
}

bool WindowsMacroHost::is_compound_object(
    const creatures1::objects::Object& object) const {
    return dynamic_cast<const creatures1::objects::CompoundObject*>(&object) !=
           nullptr;
}

bool WindowsMacroHost::is_vehicle_object(
    const creatures1::objects::Object& object) const {
    return dynamic_cast<const creatures1::objects::Vehicle*>(&object) !=
           nullptr;
}

std::uint32_t WindowsMacroHost::creature_value(
    const creatures1::objects::Object& object,
    creatures1::scripting::MacroCreatureValue value,
    std::uint32_t index) const {
    const creatures1::creatures::Creature* creature =
        document_.creature_for_object(object);
    if (creature == nullptr) {
        return 0;
    }
    switch (value) {
    case creatures1::scripting::MacroCreatureValue::chemical_concentration:
        return creature->chemical_concentration(index);
    case creatures1::scripting::MacroCreatureValue::gender:
        // Native ParseRValue `gend` @0041afa6 returns the raw byte at
        // Creature+0x2cc1: 1 = male, 2 = female (0 for a non-creature owner,
        // handled by the caller).  Returning 0/1 made the Science Kit's
        // `dde: putv gend` report males as female, and broke world scripts
        // that test `gend eq 2`.
        return creature->gender() ==
                       creatures1::creatures::CreatureGender::female
                   ? 2u
                   : 1u;
    case creatures1::scripting::MacroCreatureValue::death_state:
        return creature->life_state() ==
                       creatures1::creatures::CreatureLifeState::dead
                   ? 1u
                   : 0u;
    case creatures1::scripting::MacroCreatureValue::drive_level: {
        // GoalDriveLevels is indexable and sized; the instinct runtime state
        // exposes it directly.
        auto& drives = const_cast<creatures1::creatures::Creature&>(*creature)
                           .control_state()
                           .goal_direction_drive_levels;
        return index < drives.size() ? drives[index] : 0u;
    }
    case creatures1::scripting::MacroCreatureValue::winning_drive: {
        // The winning drive is the highest-valued slot; ties keep the first,
        // which is the native scan order.
        auto& drives = const_cast<creatures1::creatures::Creature&>(*creature)
                           .control_state()
                           .goal_direction_drive_levels;
        std::uint32_t winner = 0;
        std::uint8_t best = 0;
        for (std::size_t slot = 0; slot < drives.size(); ++slot) {
            if (drives[slot] > best) {
                best = drives[slot];
                winner = static_cast<std::uint32_t>(slot);
            }
        }
        return winner;
    }
    }
    return 0;
}

std::uint32_t WindowsMacroHost::attention_record_index(
    const creatures1::objects::Object& object) const {
    // get_attention_record_index maps the classifier family/genus onto the
    // fixed C1 attention slot; the policy was already translated.
    WindowsStimulusSourceHost classifier(document_);
    return creatures1::creatures::get_attention_record_index(
        classifier.classify(object));
}

std::int32_t WindowsMacroHost::ground_height(std::uint32_t x_block) const {
    return document_.map_ground_height(x_block);
}

std::uint32_t WindowsMacroHost::room_count() const {
    return document_.map_room_count();
}

std::int32_t WindowsMacroHost::room_value(std::uint32_t room_index,
                                          std::uint32_t field) const {
    return document_.map_room_value(room_index, field);
}

std::int32_t WindowsMacroHost::ambient_temperature_at(
    const creatures1::objects::Object& object) const {
    // world::get_ambient_temperature_at_point is already translated, and
    // WindowsCreatureEnvironmentHost::sample_at already drives it with the
    // room table and ambient record set.
    WindowsCreatureEnvironmentHost environment(document_);
    return environment
        .sample_at(object.sound_source_x(), object.sound_source_y())
        .temperature_delta_raw;
}

std::int8_t WindowsMacroHost::ambient_wind() const {
    // The selected ambient environment record carries the world wind delta.
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return 0;
    }
    const std::uint32_t index =
        runtime->map_data().ambient_environment_index_value();
    if (index >= creatures1::world::kAmbientEnvironmentRecords.size()) {
        return 0;
    }
    return creatures1::world::kAmbientEnvironmentRecords[index].wind_delta;
}

std::uint32_t WindowsMacroHost::score_value(std::uint32_t index) const {
    return document_.document_score_value(index);
}

std::uint32_t WindowsMacroHost::world_tick_count() const {
    return document_.world_tick_count();
}

std::uint32_t WindowsMacroHost::language_version() const {
    // The recovered build reports language version 1.
    return 1;
}

std::uint32_t WindowsMacroHost::sound_settings() const {
    return document_.sounds_muted() ? 0u : 1u;
}

std::uint32_t WindowsMacroHost::viewport_value(
    creatures1::scripting::MacroViewportValue value) const {
    switch (value) {
    case creatures1::scripting::MacroViewportValue::width:
        return static_cast<std::uint32_t>(document_.renderer_viewport_width());
    case creatures1::scripting::MacroViewportValue::height:
        return static_cast<std::uint32_t>(document_.renderer_viewport_height());
    case creatures1::scripting::MacroViewportValue::center_x:
        return static_cast<std::uint32_t>(
            document_.renderer_viewport_left() +
            document_.renderer_viewport_width() / 2);
    case creatures1::scripting::MacroViewportValue::center_y:
        return static_cast<std::uint32_t>(
            document_.renderer_viewport_top() +
            document_.renderer_viewport_height() / 2);
    }
    return 0;
}

creatures1::objects::Object* WindowsMacroHost::pointer_tool() const {
    return document_.pointer_tool();
}

creatures1::objects::Object* WindowsMacroHost::edit_object() const {
    return document_.edit_object();
}

creatures1::objects::Object* WindowsMacroHost::topmost_pointer_object() const {
    // CAOS `hots`: the frontmost non-scenery object whose bounds contain the
    // pointer, excluding the pointer tool itself.  Render plane is the native
    // front-to-back order, so the highest plane wins.
    const int pointer_x = document_.mouse_world_x();
    const int pointer_y = document_.mouse_world_y();
    creatures1::objects::Object* best = nullptr;
    int best_plane = 0;
    for (std::size_t index = 0; index < document_.non_scenery_object_count();
         ++index) {
        creatures1::objects::Object* candidate =
            document_.non_scenery_object_at(index);
        if (candidate == nullptr || candidate == document_.pointer_tool()) {
            continue;
        }
        creatures1::world::WorldRect bounds{};
        candidate->get_bounds(&bounds);
        if (pointer_x < bounds.min_x || pointer_x >= bounds.max_x ||
            pointer_y < bounds.min_y || pointer_y >= bounds.max_y) {
            continue;
        }
        const auto* simple =
            dynamic_cast<const creatures1::objects::SimpleObject*>(candidate);
        const int plane = simple != nullptr && simple->entity() != nullptr
                              ? simple->entity()->render_plane()
                              : 0;
        if (best == nullptr || plane >= best_plane) {
            best = candidate;
            best_plane = plane;
        }
    }
    return best;
}

std::size_t WindowsMacroHost::non_scenery_object_count() const {
    return document_.non_scenery_object_count();
}

creatures1::objects::Object* WindowsMacroHost::non_scenery_object_at(
    std::size_t index) const {
    return document_.non_scenery_object_at(index);
}

std::uint32_t WindowsMacroHost::enabled_object_count_matching(
    std::uint32_t family, std::uint32_t genus, std::uint32_t species) const {
    std::uint32_t matches = 0;
    for (std::size_t index = 0; index < non_scenery_object_count(); ++index) {
        const creatures1::objects::Object* object =
            document_.non_scenery_object_at(index);
        if (object == nullptr) {
            continue;
        }
        const std::uint32_t classifier = object->classifier_base();
        // Macro::ParseRValue `totl` @ 0x0041b131 also requires the byte at
        // Object+0x4c (tick enabled) to be set.  InitializeRuntimeState
        // (kill) clears it @ 0x0042cdce, so killed objects -- parked below
        // the world until the next save -- are not counted.
        if ((family == 0 || ((classifier >> 24) & 0xff) == family) &&
            (genus == 0 || ((classifier >> 16) & 0xff) == genus) &&
            (species == 0 || ((classifier >> 8) & 0xff) == species) &&
            object->tick_enabled()) {
            ++matches;
        }
    }
    return matches;
}

creatures1::objects::Object* WindowsMacroHost::random_non_scenery_object(
    const creatures1::scripting::MacroClassifierPattern& pattern) const {
    std::vector<creatures1::objects::Object*> matches;
    for (std::size_t index = 0; index < non_scenery_object_count(); ++index) {
        creatures1::objects::Object* object =
            document_.non_scenery_object_at(index);
        if (object == nullptr) {
            continue;
        }
        const std::uint32_t classifier = object->classifier_base();
        if ((pattern.family == 0 || ((classifier >> 24) & 0xff) == pattern.family) &&
            (pattern.genus == 0 || ((classifier >> 16) & 0xff) == pattern.genus) &&
            (pattern.species == 0 || ((classifier >> 8) & 0xff) == pattern.species)) {
            matches.push_back(object);
        }
    }
    if (matches.empty()) {
        return nullptr;
    }
    return matches[static_cast<std::size_t>(rand()) % matches.size()];
}

void WindowsMacroHost::set_selected_creature(
    creatures1::objects::Object* creature, bool notify) {
    static_cast<void>(notify);
    if (creature == nullptr) {
        document_.set_selected_creature(nullptr);
        return;
    }
    document_.set_selected_creature(creature_of(*creature));
}

void WindowsMacroHost::set_edit_object(creatures1::objects::Object* object) {
    document_.set_edit_object(object);
}

void WindowsMacroHost::die_creature(creatures1::objects::Object& object) {
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureDeathHost death_host(document_);
    creature->die(death_host);
}

void WindowsMacroHost::set_creature_dream_countdown(
    creatures1::objects::Object& object, std::uint32_t countdown) {
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->instinct_runtime_state().dream_countdown = countdown;
    }
}

void WindowsMacroHost::stop_creature_involuntary_action(
    creatures1::objects::Object& object) {
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->stop_current_involuntary_action();
    }
}

void WindowsMacroHost::set_object_bounds_mode(
    creatures1::objects::Object& object, std::uint32_t mode) {
    object.set_bounds_mode(mode, document_.renderables());
}

void WindowsMacroHost::update_object_movement_bounds(
    creatures1::objects::Object& object) {
    object.update_movement_bounds(document_);
}

void WindowsMacroHost::initialize_object_runtime_state(
    creatures1::objects::Object& object) {
    // `kill` calls vtable slot 16.  Creature overrides it with the permanent
    // delete (0040e9f0), and Bubble overrides it with DestroyAndRedraw
    // (00429f90). Other classes retain Object's parking operation.
    if (creatures1::creatures::Creature* creature = creature_of(object)) {
        document_.delete_creature(*creature);
        return;
    }
    if (auto* bubble = dynamic_cast<creatures1::objects::Bubble*>(&object)) {
        bubble->destroy_and_redraw(document_);
        return;
    }
    object.initialize_runtime_state(document_);
}

void WindowsMacroHost::remove_object_from_event_bar(
    creatures1::objects::Object& object, bool record_auxiliary_state) {
    document_.remove_from_event_bar(object, record_auxiliary_state);
}

void WindowsMacroHost::note_egg_state_change() {
    // The native only marks the world dirty; the document already tracks that
    // through its own modified flag.
    document_.SetModifiedFlag(TRUE);
}

std::uint32_t WindowsMacroHost::select_creature_target_pose_for_motion(
    creatures1::objects::Object& object, bool force_interaction_pose) {
    // CAOS `poin`: the guarded selector is already translated on Skeleton.
    // Its 0, 1 and 0xffffffff results stay meaningful to Macro, which rewinds
    // the command token only on zero.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return 0;
    }
    const std::uint32_t pose_result =
        creature->skeleton().select_target_pose_for_motion_guarded(
            force_interaction_pose);
    return pose_result;
}

void WindowsMacroHost::clear_creature_selected_decision_neuron(
    creatures1::objects::Object& object) {
    // CAOS `touc`: the -1 path clears only the decision-lobe neuron lanes.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->clear_selected_decision_neuron();
    }
}

void WindowsMacroHost::select_creature_walk_gait(
    creatures1::objects::Object& object) {
    // CAOS `appr`: the gait choice is Creature-owned and needs no host.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->select_walk_gait();
    }
}

bool WindowsMacroHost::creature_approach_is_ready(
    const creatures1::objects::Object& object) const {
    // CAOS `APPR` continuation, native ExecuteInterpreter @0041e1e9:
    //   MOV EAX,[creature+0x7c]; SUB EAX,[creature+0x7f4]; abs;
    //   CMP EAX,0x36; JG not_ready
    // i.e. ready when |down_foot_x - motion_target_x| <= 0x36.  motion_target
    // is the aimed part's centre, refreshed by Creature::Update; the creature's
    // gait steers toward it.  Comparing against the link's left edge instead
    // left wide targets permanently "not arrived", so push/get scripts stalled
    // on APPR and never reached TOUC.
    const creatures1::creatures::Creature* creature =
        document_.creature_for_object(object);
    if (creature == nullptr) {
        return false;
    }
    const creatures1::creatures::Skeleton& skeleton = creature->skeleton();
    const int distance = skeleton.down_foot_x - skeleton.motion_target_x;
    const bool ready = (distance < 0 ? -distance : distance) <= 0x36;
    if (ready) {
        static const char* setting = std::getenv("C1_TRACE_CREATURE");
        static unsigned rows = 0;
        const bool disabled = setting != nullptr &&
            (std::strcmp(setting, "0") == 0 || std::strcmp(setting, "off") == 0);
        if (!disabled && rows < 400) {
            ++rows;
            if (FILE* log = std::fopen("Creatures.place.log", "a")) {
                std::fprintf(log,
                             "appr-ready moniker=%08x action=%u foot=%d target=%d "
                             "part=%d\n",
                             skeleton.genome_source_filename,
                             creature->selected_action_id(),
                             skeleton.down_foot_x, skeleton.motion_target_x,
                             skeleton.motion_target_part_index);
                std::fclose(log);
            }
        }
    }
    return ready;
}

void WindowsMacroHost::reset_creature_animation_sequence(
    creatures1::objects::Object& object) {
    // CAOS `APPR` clears the sequence and its cursor once the approach lands.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    creatures1::creatures::Skeleton& skeleton = creature->skeleton();
    skeleton.animation_sequence.fill('\0');
    skeleton.animation_cursor = 0;
}

void WindowsMacroHost::set_viewport_value(
    creatures1::scripting::MacroViewportValue value,
    std::uint32_t new_value) {
    // CAOS `cmra` moves the viewport origin; width and height are renderer
    // geometry the command cannot resize.
    const int target = static_cast<int>(new_value);
    switch (value) {
    case creatures1::scripting::MacroViewportValue::center_x:
        document_.set_renderer_viewport_origin(
            target - document_.renderer_viewport_width() / 2,
            document_.renderer_viewport_top());
        return;
    case creatures1::scripting::MacroViewportValue::center_y:
        document_.set_renderer_viewport_origin(
            document_.renderer_viewport_left(),
            target - document_.renderer_viewport_height() / 2);
        return;
    case creatures1::scripting::MacroViewportValue::width:
    case creatures1::scripting::MacroViewportValue::height:
        report_unbound_runtime_command("cmra (viewport resize)");
        return;
    }
}

void WindowsMacroHost::set_creature_value(
    creatures1::objects::Object& object,
    creatures1::scripting::MacroCreatureAssignment field,
    std::uint32_t value) {
    // CAOS `baby` is the only assignment in this family: it writes the lower
    // edge of the target's movement bounds, which the matching rvalue reads.
    switch (field) {
    case creatures1::scripting::MacroCreatureAssignment::egg_movement_limit:
        object.set_movement_bounds_max_y(static_cast<int>(value));
        return;
    }
}

void WindowsMacroHost::add_creature_chemical_moles(
    creatures1::objects::Object& object, std::uint32_t chemical_index,
    std::uint32_t moles) {
    // CAOS `chem`: Biochemistry owns saturation and the optional log.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr || creature->biochemistry() == nullptr) {
        return;
    }
    creature->biochemistry()->add_chemical_moles(
        static_cast<int>(chemical_index), moles, active_debug_console());
}

void WindowsMacroHost::fire_creature_neuron(
    creatures1::objects::Object& object, std::int32_t global_x,
    std::int32_t global_y, std::int32_t activation) {
    // CAOS `fire`: Brain owns the lobe scan and the byte clamp.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr || creature->brain() == nullptr) {
        return;
    }
    creature->brain()->fire_neuron_at_global_position(global_x, global_y,
                                                      activation);
}

void WindowsMacroHost::set_creature_brain_neuron_activation(
    creatures1::objects::Object& object, std::uint32_t lobe_index,
    std::uint32_t neuron_index, std::uint8_t activation) {
    // CAOS `trig`: Brain/Lobe own the two byte lanes.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr || creature->brain() == nullptr) {
        return;
    }
    creature->brain()->set_lobe_neuron_activation(lobe_index, neuron_index,
                                                   activation);
}

void WindowsMacroHost::set_object_image_index(
    creatures1::objects::Object& object, std::uint8_t image_index,
    std::int32_t part_index) {
    // CAOS `base` is the native Object virtual at slot 40.  CompoundObject
    // takes a part index; SimpleObject owns a single Entity.
    WindowsEntityImageSequenceRenderHost redraw_host(document_);
    if (auto* compound =
            dynamic_cast<creatures1::objects::CompoundObject*>(&object)) {
        compound->set_image_index(image_index, static_cast<int>(part_index),
                                  redraw_host);
        return;
    }
    if (auto* simple =
            dynamic_cast<creatures1::objects::SimpleObject*>(&object)) {
        simple->set_image_index(image_index, redraw_host);
        return;
    }
    report_unbound_runtime_command("base (plain Object target)");
}

void WindowsMacroHost::configure_simple_object_behavior(
    creatures1::objects::Object& object, std::uint32_t behavior_index,
    std::uint32_t interaction_flags) {
    // CAOS `bhvr`: SimpleObject owns the five selector records.
    auto* simple = dynamic_cast<creatures1::objects::SimpleObject*>(&object);
    if (simple != nullptr) {
        simple->configure_interaction_behavior(behavior_index,
                                               interaction_flags);
    }
}

bool WindowsMacroHost::set_object_relative_image_index(
    creatures1::objects::Object& object, std::uint32_t relative_index,
    std::int32_t part_index) {
    // CAOS `pose` is the native Object virtual at slot 37.  A false result
    // rewinds the consumed command so the scheduler retries it next tick.
    // SimpleObject and CompoundObject carry the render host on their concrete
    // implementations; calling the base virtual here would accept the pose
    // without changing the displayed image.
    WindowsEntityImageSequenceRenderHost redraw_host(document_);
    if (auto* compound =
            dynamic_cast<creatures1::objects::CompoundObject*>(&object)) {
        return compound->set_relative_image_index(
            static_cast<creatures1::objects::CaosValue>(relative_index),
            static_cast<int>(part_index), redraw_host);
    }
    if (auto* simple =
            dynamic_cast<creatures1::objects::SimpleObject*>(&object)) {
        return simple->set_relative_image_index(
            static_cast<creatures1::objects::CaosValue>(relative_index),
            static_cast<int>(part_index), redraw_host);
    }
    return object.set_relative_image_index(
        static_cast<creatures1::objects::CaosValue>(relative_index),
        static_cast<int>(part_index));
}

char* WindowsMacroHost::preload_object_image_sequence(
    creatures1::objects::Object& object, char* sequence_text,
    std::int32_t part_index) {
    // CAOS `prld` is the native Object virtual at slot 39; the returned
    // pointer is the interpreter's next script cursor.
    if (auto* compound =
            dynamic_cast<creatures1::objects::CompoundObject*>(&object)) {
        return compound->preload_image_sequence(
            sequence_text, static_cast<int>(part_index), document_);
    }
    if (auto* simple =
            dynamic_cast<creatures1::objects::SimpleObject*>(&object)) {
        return simple->preload_image_sequence(
            sequence_text, static_cast<int>(part_index), document_);
    }
    return object.preload_image_sequence(sequence_text,
                                         static_cast<int>(part_index));
}

void WindowsMacroHost::teleport_target_and_refresh_selection(
    creatures1::objects::Object* target, std::int32_t world_x,
    std::int32_t world_y) {
    // CAOS `tele` moves the Creatures anchored to the target, not the target
    // itself: the native scans the Creature registry for every entry whose
    // bounds reference is the target, releases that reference, and teleports
    // it.  The move runs through a temporary UNBOUNDED_2 transition so the
    // creature's current room cannot clamp the destination, and DEFAULT_WORLD
    // is restored afterwards whatever the entry started in.
    for (std::size_t index = 0; index < document_.creature_count(); ++index) {
        auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
            document_.creature_at(index));
        if (creature == nullptr) {
            continue;
        }
        creatures1::creatures::Skeleton& skeleton = creature->skeleton();
        if (skeleton.bounds_reference_object() != target) {
            continue;
        }

        skeleton.set_bounds_reference_object(nullptr);
        skeleton.set_bounds_mode(
            static_cast<std::uint32_t>(
                creatures1::objects::Object::BoundsMode::unbounded_2),
            document_.renderables());
        skeleton.update_movement_bounds(document_);
        // Skeleton's MoveToAndRedraw is vtable slot 23, which for a Creature
        // is SetDownFootPositionAndInvalidateBounds @0043c2f0 -- a creature is
        // positioned by the foot it stands on, and the redraw comes from the
        // two dirty rectangles it queues, not from a layout recompute.
        class SkeletonMoveHost final
            : public creatures1::creatures::SkeletonWorldHost {
        public:
            explicit SkeletonMoveHost(C1WindowsDocument& document)
                : document_(document) {}
            void move_by(creatures1::creatures::Skeleton& skeleton,
                         int delta_x, int delta_y) override {
                skeleton.translate_by(delta_x, delta_y);
            }
            void queue_dirty_world_rect(
                const creatures1::world::WorldRect& bounds) override {
                document_.queue_renderer_dirty_world_rect(bounds);
            }

        private:
            C1WindowsDocument& document_;
        };
        SkeletonMoveHost move_host(document_);
        skeleton.set_down_foot_position_and_invalidate_bounds(
            static_cast<int>(world_x), static_cast<int>(world_y), move_host,
            document_);
        skeleton.set_bounds_mode(
            static_cast<std::uint32_t>(
                creatures1::objects::Object::BoundsMode::default_world),
            document_.renderables());
        skeleton.update_movement_bounds(document_);

        if (document_.is_selected_creature(skeleton)) {
            document_.request_viewport_origin_for_selected_creature();
            // The eye view follows the creature it was showing; both calls are
            // no-ops when no eye view is open, matching the native null guard.
            document_.update_eye_view_title();
            document_.invalidate_eye_view_follow_position();
        }
    }
}

void WindowsMacroHost::drive_presentation_for_bound_creatures(
    creatures1::objects::Object& target) {
    // CAOS `dpas`: queue event 5 for every creature whose movement bounds are
    // anchored to the target.  The native admits only the family-3 target.
    if (((target.classifier_base() >> 24) & 0xff) != 3) {
        return;
    }
    for (std::size_t index = 0; index < document_.creature_count(); ++index) {
        auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
            document_.creature_at(index));
        if (creature == nullptr) {
            continue;
        }
        creatures1::objects::Object& creature_object =
            document_.object_for_creature(*creature);
        if (creature_object.bounds_reference_object() == &target) {
            document_.queue_immediate_object_event(
                target, creature_object,
                creatures1::objects::ObjectEventId::event_5, 0);
        }
    }
}

void WindowsMacroHost::process_creature_insemination(
    creatures1::objects::Object& object) {
    // CAOS `mate`: Creature owns the conception-probability decision and the
    // fertility state transition.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureInseminationHost host(document_);
    creature->process_insemination(host);
}

void WindowsMacroHost::set_creature_sleep_indicator(
    creatures1::objects::Object& object, std::uint32_t enabled) {
    // CAOS `aslp`: Creature owns the state transition and event choice.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureAttentionHost attention(document_);
    creature->set_sleep_indicator(enabled != 0, attention);
}

void WindowsMacroHost::notify_creature_dependents_on_removal(
    creatures1::objects::Object& object) {
    // CAOS `drop`: Creature owns the recovered notification order.  Every
    // host it needs already exists -- the world runtime is the object
    // registry, the document is the identity host, and the stimulus source
    // and immediate-event queue are the small adapters below.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    WindowsImmediateEventQueueHost immediate_events(document_);
    WindowsStimulusSourceHost source_host(document_);
    creature->notify_dependents_on_removal(
        document_.object_registry(), immediate_events, document_,
        source_host, nullptr, active_debug_console());
}

void WindowsMacroHost::add_object_to_event_bar(
    creatures1::objects::Object* object) {
    // CAOS `evnt`: the event bar owns duplicate suppression and eviction, and
    // the recovered add accepts a null pointer.
    document_.add_to_event_bar(object);
}

void WindowsMacroHost::set_creature_action_activation_boost(
    creatures1::objects::Object& object, std::uint32_t value) {
    // CAOS `impt`: only the low byte is stored.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->set_action_activation_boost(
            static_cast<std::uint8_t>(value & 0xffu));
    }
}

void WindowsMacroHost::set_creature_involuntary_action_cooldown(
    creatures1::objects::Object& object, std::uint32_t action_index,
    std::uint8_t cooldown_ticks) {
    // CAOS `ltcy`: Creature owns the eight two-byte involuntary-action
    // records; the instinct runtime state exposes them directly.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    auto& actions = creature->control_state().involuntary_actions;
    if (action_index < actions.size()) {
        actions[action_index].cooldown_ticks = cooldown_ticks;
    }
}

void WindowsMacroHost::update_creature_bacterium(
    creatures1::objects::Object& object) {
    // CAOS `snez`: Creature owns the embedded bacterium update.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureBacteriumEnvironmentHost host(document_);
    creature->update_bacterium_and_environment(host, active_debug_console());
}

void WindowsMacroHost::initialize_creature_default_vocabulary(
    creatures1::objects::Object& object) {
    // CAOS `vocb`: Creature owns the stock vocabulary tables outright.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->initialize_default_vocabulary();
    }
}

void WindowsMacroHost::set_compound_part_bounds(
    creatures1::objects::Object& object, std::uint32_t part_bounds_index,
    std::int32_t min_x, std::int32_t min_y, std::int32_t max_x,
    std::int32_t max_y) {
    // CAOS `spot`: CompoundObject owns the six signed rectangles.
    auto* compound =
        dynamic_cast<creatures1::objects::CompoundObject*>(&object);
    if (compound == nullptr ||
        part_bounds_index >=
            creatures1::objects::CompoundObject::kPartBoundsCount) {
        return;
    }
    compound->set_part_bounds(static_cast<std::size_t>(part_bounds_index),
                              creatures1::world::WorldRect{min_x, min_y,
                                                           max_x, max_y});
}

void WindowsMacroHost::set_compound_knob_function(
    creatures1::objects::Object& object, std::uint32_t function_index,
    std::uint32_t hotspot_index) {
    // CAOS `knob`: CompoundObject owns the creature-event mapping.
    auto* compound =
        dynamic_cast<creatures1::objects::CompoundObject*>(&object);
    if (compound == nullptr) {
        return;
    }
    compound->set_knob_function(function_index,
                                static_cast<int>(hotspot_index));
}

void WindowsMacroHost::set_vehicle_creature_event_bounds(
    creatures1::objects::Object& object, std::int32_t min_x,
    std::int32_t min_y, std::int32_t max_x, std::int32_t max_y) {
    // CAOS `cabn`: Vehicle owns the local creature-event rectangle.
    auto* vehicle = dynamic_cast<creatures1::objects::Vehicle*>(&object);
    if (vehicle != nullptr) {
        vehicle->set_creature_event_bounds_local(
            creatures1::world::WorldRect{min_x, min_y, max_x, max_y});
    }
}

void WindowsMacroHost::grab_vehicle_passengers(
    creatures1::objects::Object& object) {
    // CAOS `gpas`: Vehicle owns the local event production policy.
    auto* vehicle = dynamic_cast<creatures1::objects::Vehicle*>(&object);
    if (vehicle == nullptr) {
        return;
    }
    WindowsVehicleEventHost event_host(document_);
    vehicle->queue_event_4_for_creatures_in_local_bounds(event_host);
}



// --- Small host adapters for the recovered CAOS delegates -----------------

creatures1::creatures::AttentionClassifier
WindowsStimulusSourceHost::classify(
    const creatures1::objects::Object& object) const {
    // The attention family is the classifier's high byte; the genus is the
    // next one, exactly as get_attention_record_index expects.
    const std::uint32_t classifier = object.classifier_base();
    creatures1::creatures::AttentionClassifier result;
    switch ((classifier >> 24) & 0xff) {
    case 1:
        result.family = creatures1::creatures::AttentionObjectFamily::scenery;
        break;
    case 2:
        result.family =
            creatures1::creatures::AttentionObjectFamily::simple_object;
        break;
    case 3:
        result.family =
            creatures1::creatures::AttentionObjectFamily::compound_object;
        break;
    case 4:
        result.family = creatures1::creatures::AttentionObjectFamily::creature;
        break;
    default:
        result.family = creatures1::creatures::AttentionObjectFamily::unknown;
        break;
    }
    result.genus = static_cast<std::uint8_t>((classifier >> 16) & 0xff);
    return result;
}

int WindowsStimulusSourceHost::sound_source_x(
    const creatures1::objects::Object& object) const {
    return object.sound_source_x();
}

int WindowsStimulusSourceHost::sound_source_y(
    const creatures1::objects::Object& object) const {
    return object.sound_source_y();
}

bool WindowsStimulusSourceHost::is_this_creature(
    const creatures1::objects::Object& object,
    const creatures1::creatures::Creature& creature) const {
    return document_.creature_for_object(object) == &creature;
}

void WindowsEntityImageSequenceRenderHost::redraw_image_sequence_change(
    creatures1::objects::Entity& entity,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    static_cast<void>(entity);
    // This used to invalidate the whole main window on every animation frame
    // change, which forced a full-viewport repaint many times per tick.
    document_.queue_renderer_dirty_world_rect(old_bounds);
    document_.queue_renderer_dirty_world_rect(new_bounds);
}

std::size_t WindowsVehicleEventHost::creature_count() const {
    return document_.creature_count();
}

creatures1::objects::Object* WindowsVehicleEventHost::creature_at(
    std::size_t index) const {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
    return creature == nullptr ? nullptr
                               : &document_.object_for_creature(*creature);
}

void WindowsVehicleEventHost::report_invalid_creature_index() const {
    throw std::out_of_range("C1 vehicle creature registry index");
}

bool WindowsVehicleEventHost::object_is_bound_to_vehicle(
    const creatures1::objects::Object& creature,
    const creatures1::objects::Vehicle& vehicle) const {
    // The recovered predicate is bounds-reference identity: a passenger is a
    // creature whose movement bounds are anchored to this vehicle.
    return creature.bounds_reference_object() == &vehicle;
}

void WindowsVehicleEventHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

creatures1::creatures::CreatureEnvironmentHost&
WindowsCreatureBacteriumEnvironmentHost::environment_host() {
    return environment_;
}

creatures1::creatures::CreatureGoalDirectionHost&
WindowsCreatureBacteriumEnvironmentHost::goal_direction_host() {
    return *this;
}

bool WindowsCreatureBacteriumEnvironmentHost::is_selected_creature(
    const creatures1::creatures::Creature& creature) const {
    return document_.selected_creature() == &creature;
}

std::int32_t WindowsCreatureBacteriumEnvironmentHost::room_index_at(
    int world_x, int world_y) const {
    return document_.map_room_index_at(world_x, world_y);
}

std::int32_t WindowsCreatureBacteriumEnvironmentHost::room_class_at(
    int world_x, int world_y) const {
    const std::int32_t index = document_.map_room_index_at(world_x, world_y);
    return index < 0 ? -1 : document_.map_room_value(
                                static_cast<std::uint32_t>(index), 4);
}

std::uint32_t
WindowsCreatureBacteriumEnvironmentHost::current_interaction_event_id(
    const creatures1::objects::Object& object) const {
    return object.current_interaction_event_id_for_script();
}

std::int32_t WindowsCreatureBacteriumEnvironmentHost::attention_record_index(
    const creatures1::objects::Object& object) const {
    WindowsStimulusSourceHost classifier(document_);
    return static_cast<std::int32_t>(
        creatures1::creatures::get_attention_record_index(
            classifier.classify(object)));
}

void WindowsImmediateEventQueueHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}


// --- CreatureAttentionHost -------------------------------------------------

creatures1::objects::Object&
WindowsCreatureAttentionHost::object_for_creature(
    creatures1::creatures::Creature& creature) const {
    return document_.object_for_creature(creature);
}

creatures1::objects::Object*
WindowsCreatureAttentionHost::create_sleep_indicator(
    creatures1::creatures::Creature& creature, int render_plane) {
    // SetSleepIndicator @ 0040da80 constructs the indicator as a SimpleObject
    // with sprite file 0x7a7a7a7a, ten images, classifier family 2 genus 3
    // species 0xd, bounds flags 0x10, click selector 0xff, positioned at the
    // creature's sound source minus 0x14 in y.
    const creatures1::objects::Object& owner =
        document_.object_for_creature(creature);
    auto indicator = std::make_unique<creatures1::objects::SimpleObject>(
        0x7a7a7a7au, 0, 10u, false, owner.sound_source_x(),
        owner.sound_source_y() - 0x14, render_plane,
        static_cast<std::uint8_t>(0x10),
        static_cast<std::uint8_t>(
            creatures1::scripting::ScriptEvent::deactivate),
        static_cast<std::uint8_t>(0x0d), static_cast<std::uint8_t>(3),
        static_cast<std::uint8_t>(2), static_cast<std::uint8_t>(0xff), 0u, 0u,
        static_cast<std::uint8_t>(0), document_);
    auto* raw = indicator.get();
    document_.adopt_sleep_indicator(std::move(indicator));
    return raw;
}

bool WindowsCreatureAttentionHost::action_event_target_is_live(
    const creatures1::objects::Object& target) const {
    // A target is live while it is still registered as a world object.
    for (std::size_t index = 0;
         index < document_.non_scenery_object_count(); ++index) {
        if (document_.non_scenery_object_at(index) == &target) {
            return true;
        }
    }
    return false;
}

std::uint32_t WindowsCreatureAttentionHost::classifier_base(
    const creatures1::objects::Object& object) const {
    return object.classifier_base();
}

creatures1::creatures::ActionTargetRequirement
WindowsCreatureAttentionHost::action_target_requirement(
    std::size_t action_index) const {
    // g_action_target_requirements[16] @ 00454450, one entry per Decision-lobe
    // neuron index, which is also the C1CreatureAction value.  Actions 12-15
    // are reserved headroom the shipped game never uses: the table marks them
    // not_selectable, and no family-4 script in a populated World.sfc carries
    // their event numbers.
    using Requirement = creatures1::creatures::ActionTargetRequirement;
    static constexpr Requirement kActionTargetRequirements[16] = {
        Requirement::always_eligible,     // 0  quiescent
        Requirement::requires_target,     // 1  activate 1
        Requirement::requires_target,     // 2  activate 2
        Requirement::requires_target,     // 3  deactivate
        Requirement::requires_target,     // 4  approach
        Requirement::requires_target,     // 5  retreat
        Requirement::requires_target,     // 6  get
        Requirement::always_eligible,     // 7  drop
        Requirement::always_eligible,     // 8  express need
        Requirement::requires_no_target,  // 9  rest
        Requirement::always_eligible,     // 10 travel west
        Requirement::always_eligible,     // 11 travel east
        Requirement::not_selectable,      // 12 reserved
        Requirement::not_selectable,      // 13 reserved
        Requirement::not_selectable,      // 14 reserved
        Requirement::not_selectable,      // 15 reserved
    };
    return action_index < std::size(kActionTargetRequirements)
               ? kActionTargetRequirements[action_index]
               : Requirement::not_selectable;
}

creatures1::creatures::AttentionClassifier
WindowsCreatureAttentionHost::classify_object(
    const creatures1::objects::Object& object) const {
    WindowsStimulusSourceHost classifier(document_);
    return classifier.classify(object);
}

void WindowsCreatureAttentionHost::dispatch_sleep_indicator_event(
    creatures1::objects::Object& indicator,
    creatures1::objects::ObjectEventId event_id,
    creatures1::objects::Object* target, std::uint32_t /*argument*/) {
    // SetSleepIndicator @ 0040da80 calls Object::DispatchScriptEvent (vtable
    // +0x88) directly, so the indicator's event script (1/2: `sndl zzzz` /
    // `sndl gsnr`, 0: `fade`) runs before InitializeRuntimeState purges the
    // object's macros.  Queuing it let the purge win: `fade` never ran and
    // every sleep left its snore loop playing.
    WindowsObjectScriptDispatchHost scripts(document_);
    indicator.dispatch_script_event(event_id, target, false, scripts);
}

void WindowsCreatureAttentionHost::initialize_sleep_indicator(
    creatures1::objects::Object& indicator) {
    indicator.initialize_runtime_state(document_);
}

bool WindowsCreatureAttentionHost::debug_console_visible() const {
    return active_debug_console() != nullptr;
}

bool WindowsCreatureAttentionHost::is_selected_creature(
    const creatures1::creatures::Creature& creature) const {
    return document_.selected_creature() == &creature;
}

void WindowsCreatureAttentionHost::log_attention_shift(
    const creatures1::creatures::Creature& creature,
    const creatures1::objects::Object* target) {
    static_cast<void>(creature);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "Attention shifted to %s\n",
            target == nullptr ? "nothing" : "object");
    }
}

void WindowsCreatureAttentionHost::log_override_action_script_selected(
    const creatures1::creatures::Creature& creature,
    std::uint32_t classifier) {
    static_cast<void>(creature);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "Override action script selected: %08x\n",
            classifier);
    }
}

void WindowsCreatureAttentionHost::log_no_action_script(
    const creatures1::creatures::Creature& creature,
    std::uint32_t classifier) {
    static_cast<void>(creature);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x1,
                                      "No action script for %08x\n",
                                      classifier);
    }
}

void WindowsCreatureAttentionHost::log_involuntary_action(
    const creatures1::creatures::Creature& creature, int action_index) {
    static_cast<void>(creature);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x1,
                                      "Involuntary action %d\n",
                                      action_index);
    }
}

void WindowsCreatureAttentionHost::log_action_selection(
    const creatures1::creatures::Creature& creature,
    const creatures1::objects::Object* motion_link,
    std::uint32_t action_index) {
    static_cast<void>(creature);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "Action %u selected, motion link %s\n",
            action_index, motion_link == nullptr ? "none" : "set");
    }
}


void WindowsCreatureAttentionHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

std::uint32_t WindowsCreatureAttentionHost::classifier_base(
    const creatures1::creatures::Creature& creature) const {
    return document_
        .object_for_creature(
            const_cast<creatures1::creatures::Creature&>(creature))
        .classifier_base();
}

int WindowsCreatureAttentionHost::execute_script_for_classifier(
    creatures1::creatures::Creature& creature,
    creatures1::objects::Object* source, std::uint32_t classifier,
    bool force_restart) {
    // The classifier resolver in scripting/classifier_scripts.cpp owns the
    // lookup and invocation state transitions; the macro chain supplies its
    // storage and scheduling.
    WindowsMacroHost macro_host(document_);
    WindowsScriptExecutionHost runtime(document_, macro_host);
    creatures1::scripting::ScriptClassifier packed;
    packed.family = static_cast<std::uint8_t>((classifier >> 24) & 0xff);
    packed.genus = static_cast<std::uint8_t>((classifier >> 16) & 0xff);
    packed.species = static_cast<std::uint8_t>((classifier >> 8) & 0xff);
    packed.event = static_cast<creatures1::scripting::ScriptEvent>(
        classifier & 0xff);
    return creatures1::scripting::execute_script_for_classifier(
               &document_.object_for_creature(creature), source, packed,
               force_restart, runtime)
               ? 1
               : 0;
}

// --- WindowsScriptExecutionHost --------------------------------------------

void WindowsScriptExecutionHost::report_script_counts_if_changed(
    std::size_t stored_count, std::size_t running_count) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "Scripts: %u stored, %u running\n",
            static_cast<unsigned>(stored_count),
            static_cast<unsigned>(running_count));
    }
}

creatures1::scripting::Macro*
WindowsScriptExecutionHost::find_running_macro_for_owner(
    creatures1::objects::Object* script_owner) const {
    for (creatures1::scripting::Macro* macro :
         creatures1::scripting::g_running_macros) {
        if (macro != nullptr &&
            macro->object_context.script_owner == script_owner) {
            return macro;
        }
    }
    return nullptr;
}

creatures1::scripting::Macro*
WindowsScriptExecutionHost::create_initialized_macro() {
    // The scheduler owns a started macro's lifetime, matching the native,
    // where the running-macro list is the owner.
    auto* macro = new (std::nothrow) creatures1::scripting::Macro();
    if (macro != nullptr) {
        macro->reset_execution_state(macro_host_);
    }
    return macro;
}

creatures1::objects::Object*
WindowsScriptExecutionHost::selected_creature() const {
    creatures1::creatures::Creature* creature = document_.selected_creature();
    return creature == nullptr ? nullptr
                               : &document_.object_for_creature(*creature);
}

void WindowsScriptExecutionHost::start_macro_execution(
    creatures1::scripting::Macro& macro) {
    macro_host_.start_macro(macro);
}

void WindowsScriptExecutionHost::report_missing_script(
    creatures1::scripting::ScriptClassifier classifier) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "No script for %u %u %u event %u\n",
            static_cast<unsigned>(classifier.family),
            static_cast<unsigned>(classifier.genus),
            static_cast<unsigned>(classifier.species),
            static_cast<unsigned>(classifier.event));
    }
}


// --- CreatureInseminationHost ----------------------------------------------

creatures1::creatures::Creature*
WindowsCreatureInseminationHost::recipient_for_insemination(
    creatures1::creatures::Creature& source) const {
    // The recovered partner is the creature this one is motion-linked to.
    const creatures1::objects::Object* link = source.skeleton().motion_link;
    if (link == nullptr) {
        return nullptr;
    }
    creatures1::creatures::Creature* recipient =
        document_.mutable_creature_for_object(
            *const_cast<creatures1::objects::Object*>(link));
    return recipient == &source ? nullptr : recipient;
}

creatures1::creatures::GenomeFilenameId
WindowsCreatureInseminationHost::generate_offspring_genome_file(
    creatures1::creatures::GenomeFilenameId maternal_source_filename,
    creatures1::creatures::GenomeFilenameId paternal_source_filename) {
    // The crossover/mutation policy is already translated; this supplies the
    // file store the document owns, the CRT random source, and the next free
    // genome filename.
    // The unique-filename policy is translated too: four random letters and a
    // digit, retried until the registry reports the id unused.
    creatures1::creatures::Genome placeholder;
    creatures1::creatures::generate_unique_genome_filename(placeholder, *this,
                                                           *this);
    const creatures1::creatures::GenomeFilenameId generated =
        placeholder.source_filename();
    return creatures1::creatures::generate_offspring_genome_file(
        maternal_source_filename, paternal_source_filename,
        document_.genome_files(), *this, generated);
}

bool WindowsCreatureInseminationHost::debug_console_visible() const {
    return active_debug_console() != nullptr;
}

bool WindowsCreatureInseminationHost::is_selected_creature(
    const creatures1::creatures::Creature& creature) const {
    return document_.selected_creature() == &creature;
}

void WindowsCreatureInseminationHost::log_insemination(
    creatures1::creatures::InseminationLogEvent event,
    const creatures1::creatures::Creature& source,
    const creatures1::creatures::Creature* recipient,
    creatures1::creatures::GenomeFilenameId paternal_source_filename) {
    static_cast<void>(source);
    static_cast<void>(recipient);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "Insemination event %u, paternal genome %u\n",
            static_cast<unsigned>(event),
            static_cast<unsigned>(paternal_source_filename));
    }
}

bool WindowsCreatureInseminationHost::creature_uses_filename(
    creatures1::creatures::GenomeFilenameId id) const {
    for (std::size_t index = 0; index < document_.creature_count(); ++index) {
        const auto* creature =
            dynamic_cast<const creatures1::creatures::Creature*>(
                document_.creature_at(index));
        if (creature != nullptr &&
            (creature->child_genome_source_filename() == id ||
             creature->gamete_genome_source_filename() == id)) {
            return true;
        }
    }
    return false;
}

bool WindowsCreatureInseminationHost::family_four_object_uses_filename(
    creatures1::creatures::GenomeFilenameId id) const {
    // Family four is the creature classifier family; an egg carries the
    // genome filename it will hatch.
    static_cast<void>(id);
    // Held: eggs store their genome filename in object state the document
    // does not expose, so only the creature registry is consulted.  A
    // collision with an unhatched egg remains possible.
    return false;
}

std::uint32_t WindowsCreatureInseminationHost::next() {
    return static_cast<std::uint32_t>(rand());
}

void WindowsCreatureInseminationHost::log_mutation(std::uint8_t old_value,
                                                   std::uint8_t new_value,
                                                   std::uint8_t bit_distance) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1, "Mutation %02x -> %02x, distance %u\n",
            static_cast<unsigned>(old_value),
            static_cast<unsigned>(new_value),
            static_cast<unsigned>(bit_distance));
    }
}

bool WindowsCreatureInseminationHost::debug_logging_enabled() const {
    return active_debug_console() != nullptr;
}

void WindowsCreatureInseminationHost::log_conception(
    std::uint32_t crossovers, std::uint32_t duplications,
    std::uint32_t omissions, std::uint32_t mutations) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1,
            "Conception: %u crossovers, %u duplications, %u omissions, "
            "%u mutations\n",
            crossovers, duplications, omissions, mutations);
    }
}

int WindowsObjectScriptDispatchHost::execute_script_for_classifier(
    creatures1::objects::Object& object,
    creatures1::objects::Object* from_object, std::uint32_t classifier,
    bool force_restart) {
    WindowsMacroHost macro_host(document_);
    WindowsScriptExecutionHost runtime(document_, macro_host);
    creatures1::scripting::ScriptClassifier packed;
    packed.family = static_cast<std::uint8_t>((classifier >> 24) & 0xff);
    packed.genus = static_cast<std::uint8_t>((classifier >> 16) & 0xff);
    packed.species = static_cast<std::uint8_t>((classifier >> 8) & 0xff);
    packed.event =
        static_cast<creatures1::scripting::ScriptEvent>(classifier & 0xff);
    return creatures1::scripting::execute_script_for_classifier(
               &object, from_object, packed, force_restart, runtime)
               ? 1
               : 0;
}


// --- WindowsCreatureConstructionHost ---------------------------------------

creatures1::creatures::Creature::InitializationHost&
WindowsCreatureConstructionHost::initialization_host() {
    return document_;
}

creatures1::creatures::Creature::GenomeInitializationHost&
WindowsCreatureConstructionHost::genome_initialization_host() {
    return *this;
}

std::size_t WindowsCreatureConstructionHost::living_creature_count() const {
    return document_.creature_count();
}

const creatures1::creatures::Creature*
WindowsCreatureConstructionHost::living_creature_at(
    std::size_t index) const {
    return dynamic_cast<const creatures1::creatures::Creature*>(
        document_.creature_at(index));
}

void WindowsCreatureConstructionHost::report_invalid_living_creature_index()
    const {
    throw std::out_of_range("C1 living creature registry index");
}

creatures1::objects::ObjectRenderableSetHost&
WindowsCreatureConstructionHost::renderable_set() {
    return document_.renderables();
}

creatures1::objects::ObjectMovementBoundsHost&
WindowsCreatureConstructionHost::movement_bounds() {
    return document_;
}

creatures1::objects::ObjectSoundPlaybackHost&
WindowsCreatureConstructionHost::sound_playback() {
    return document_;
}

const creatures1::creatures::MultibyteTextApi&
WindowsCreatureConstructionHost::text_api() const {
    return document_;
}

creatures1::creatures::CreatureEnvironmentHost&
WindowsCreatureConstructionHost::environment() {
    return environment_;
}

void WindowsCreatureConstructionHost::append_to_creature_registry(
    creatures1::creatures::Creature& creature) {
    static_cast<void>(creature);
    // Deliberately empty.  The recovered constructor asks the host to register
    // the creature, but in the port WorldRuntime::adopt_creature performs both
    // the registry append and the ownership transfer in one step, and the
    // caller runs it immediately after construction.  Registering here as well
    // would list the creature twice.
}

void WindowsCreatureConstructionHost::rebuild_creature_selection_menu() {
    document_.rebuild_creature_selection_menu();
}

creatures1::creatures::GenomeFileStore&
WindowsCreatureConstructionHost::genome_files() {
    return document_.genome_files();
}

const creatures1::creatures::SkeletonSpriteBuildServices&
WindowsCreatureConstructionHost::skeleton_services() const {
    if (!skeleton_services_) {
        skeleton_services_.emplace(document_.skeleton_services(document_));
    }
    return *skeleton_services_;
}

creatures1::creatures::SkeletonRenderPlaneHost&
WindowsCreatureConstructionHost::render_plane_host() {
    return render_plane_;
}

creatures1::objects::ObjectSoundPlaybackHost&
WindowsCreatureConstructionHost::sound_host() {
    return document_;
}

creatures1::biochemistry::BiochemistryLocusHost&
WindowsCreatureConstructionHost::biochemistry_locus_host(
    creatures1::biochemistry::Biochemistry& value) {
    // Resolution runs through `value.owner()`, so this must be the creature's
    // own biochemistry.  It used to wrap a freshly constructed, ownerless
    // Biochemistry instead, which made every locus lookup throw "no owning
    // Brain for locus resolution" and killed creature construction outright.
    locus_host_.emplace(value);
    return *locus_host_;
}

creatures1::creatures::VoiceFileStore&
WindowsCreatureConstructionHost::voice_files() {
    return document_.voice_files();
}

// --- WindowsGeneratedCreatureHost ------------------------------------------

creatures1::creatures::GenomeFilenameId
WindowsGeneratedCreatureHost::generate_test_offspring_genome() {
    // The Testing command breeds from the shipped "test" genome with no
    // paternal parent.  Both native handlers call
    // GenerateOffspringGenomeFile(0x74736574, 0), and 0x74736574 is 'test'
    // little-endian -- TEST.GEN in the Genetics directory.
    //
    // Passing 0 as the maternal id instead made `Genome`'s constructor bail
    // out (it treats id 0 as "no file"), so the offspring payload was copied
    // from an empty genome and every generated creature was written a
    // zero-byte .gen file.  Such a creature came up with no brain lobes, no
    // instincts and therefore never dreamed.
    constexpr creatures1::creatures::GenomeFilenameId kTestGenomeFilename =
        0x74736574u;
    WindowsCreatureInseminationHost genomes(document_);
    return genomes.generate_offspring_genome_file(kTestGenomeFilename, 0);
}

creatures1::creatures::Creature*
WindowsGeneratedCreatureHost::create_generated_creature(
    creatures1::creatures::GenomeFilenameId genome_source_filename,
    creatures1::creatures::CreatureConstructionSex construction_sex) {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return nullptr;
    }
    WindowsCreatureConstructionHost host(document_);
    auto creature = std::make_unique<creatures1::creatures::Creature>(
        genome_source_filename, construction_sex, host);
    creatures1::creatures::Creature* const adopted =
        &runtime->adopt_creature(std::move(creature));
    // Creature's recovered constructor asks its construction host to rebuild
    // the menu, but adoption happens immediately after construction in this
    // port.  Refresh once more after adoption so the new creature is present
    // in the selection registry before the menu is rebuilt.
    document_.rebuild_creature_selection_menu();
    return adopted;
}

void WindowsGeneratedCreatureHost::set_selected_creature(
    creatures1::creatures::Creature* creature) {
    document_.set_selected_creature(creature);
}

void WindowsGeneratedCreatureHost::set_edit_object(
    creatures1::creatures::Creature* creature) {
    document_.set_edit_object(
        creature == nullptr ? nullptr
                            : &document_.object_for_creature(*creature));
}


// --- MacroObjectMotionHost --------------------------------------------------

void WindowsMacroHost::move_to_and_redraw(creatures1::objects::Object& object,
                                          int world_x, int world_y) {
    document_.move_to_and_redraw(object, world_x, world_y);
}

void WindowsMacroHost::move_by_and_redraw(creatures1::objects::Object& object,
                                          int delta_x, int delta_y) {
    document_.move_by_and_redraw(object, delta_x, delta_y);
}

void WindowsMacroHost::set_motion_target_part_index(
    creatures1::objects::Object& object, std::uint32_t part_index) {
    // Macro has already established that the target is a creature; the
    // skeleton owns the motion-target part the walk aims at.
    creatures1::creatures::Creature* creature = creature_of(object);
    if (creature != nullptr) {
        creature->skeleton().motion_target_part_index =
            static_cast<std::int32_t>(part_index);
    }
}

std::int32_t WindowsMacroHost::vehicle_movement_vector_x(
    const creatures1::objects::Object& object) const {
    const auto* vehicle =
        dynamic_cast<const creatures1::objects::Vehicle*>(&object);
    return vehicle == nullptr ? 0 : vehicle->velocity_x_8_8;
}

std::int32_t WindowsMacroHost::vehicle_movement_vector_y(
    const creatures1::objects::Object& object) const {
    const auto* vehicle =
        dynamic_cast<const creatures1::objects::Vehicle*>(&object);
    return vehicle == nullptr ? 0 : vehicle->velocity_y_8_8;
}

void WindowsMacroHost::set_vehicle_movement_vector_x(
    creatures1::objects::Object& object, std::int32_t value) {
    auto* vehicle = dynamic_cast<creatures1::objects::Vehicle*>(&object);
    if (vehicle != nullptr) {
        vehicle->velocity_x_8_8 = value;
    }
}

void WindowsMacroHost::set_vehicle_movement_vector_y(
    creatures1::objects::Object& object, std::int32_t value) {
    auto* vehicle = dynamic_cast<creatures1::objects::Vehicle*>(&object);
    if (vehicle != nullptr) {
        vehicle->velocity_y_8_8 = value;
    }
}

// --- MacroObjectEventHost / MacroDebugHost / MacroApplicationHost ----------

void WindowsMacroHost::queue_immediate_object_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id) {
    // The three-argument form the vehicle commands use; the runtime's own
    // four-argument overload carries the extra event argument.
    document_.queue_immediate_object_event(source, target, event_id, 0);
}

void WindowsMacroHost::log_debug_message(creatures1::scripting::Macro& macro,
                                         std::string_view text) {
    static_cast<void>(macro);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, kMacroDebugCategory, "%s\n",
                                      std::string(text).c_str());
    }
}

void WindowsMacroHost::log_debug_value(creatures1::scripting::Macro& macro,
                                       std::uint32_t value) {
    static_cast<void>(macro);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, kMacroDebugCategory,
                                      "Debug Value: %d\n",
                                      static_cast<int>(value));
    }
}

void WindowsMacroHost::log_debug_command_value(
    creatures1::scripting::Macro& macro, std::uint32_t value) {
    static_cast<void>(macro);
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, kMacroDebugCategory,
                                      "Debug: %d\n",
                                      static_cast<int>(value));
    }
}

void WindowsMacroHost::shutdown_embedded_kit_tool(std::uint32_t tool_index) {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        // The frame declares this as a platform override, so reach it through
        // the interface that owns the operation rather than the concrete type.
        static_cast<creatures1::application::MainFrameLifecyclePlatform*>(frame)
            ->shutdown_embedded_kit_tool(static_cast<std::size_t>(tool_index));
    }
}

void WindowsMacroHost::register_embedded_kit_tool(
    creatures1::scripting::Macro& macro, std::string_view registry_name,
    std::string_view display_name, std::string_view launch_command,
    std::uint8_t raw_type_code) {
    static_cast<void>(macro);
    creatures1::application::EmbeddedKitToolRegistration registration;
    registration.registry_name = std::string(registry_name);
    registration.display_name = std::string(display_name);
    registration.launch_command = std::string(launch_command);
    registration.raw_type_code = raw_type_code;
    register_embedded_kit_tool_for_frame(active_main_frame(), registration);
}

// --- MacroSpeechHost -------------------------------------------------------

void WindowsMacroHost::speak_text(creatures1::objects::Object& creature_object,
                                  std::string_view text) {
    // say$ is Creature::Speak with the bracketed literal; Macro has already
    // established that the target is a creature.
    creatures1::creatures::Creature* creature = creature_of(creature_object);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureSpeechHost speech(document_);
    creature->speak(text, speech);
}

void WindowsMacroHost::speak_learned_word(
    creatures1::objects::Object& creature_object,
    std::uint32_t learned_word_index) {
    // Macro `say#` @ 0x00420... speaks learned_word_records[n].response_word
    // through SpeakTextWithVoice and then queues EVENT_6 to every creature in
    // speech range, carrying the word index as the event argument and the
    // Voice delay as the schedule.  Macro has already proven the target is a
    // creature-family object.
    creatures1::creatures::Creature* creature = creature_of(creature_object);
    if (creature == nullptr) {
        return;
    }
    if (learned_word_index >=
        creatures1::creatures::Creature::kLearnedWordRecordCount) {
        // Native indexes learned_word_records unchecked and would read past
        // the fixed 0x50-entry table.  The port refuses and says so rather
        // than reproducing the overread.
        OutputDebugStringA("'say#' learned-word index out of range\n");
        return;
    }

    WindowsCreatureSpeechPhraseHost phrase_host(document_);
    std::int32_t delay_world_ticks = 0;
    creature->speak_text_with_voice(
        creature->learned_word_record(learned_word_index).response_word,
        delay_world_ticks, phrase_host);

    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_events_in_speech_range(
        creature_object, creatures1::objects::ObjectEventId::event_6,
        learned_word_index, delay_world_ticks, fanout);
}

void WindowsMacroHost::speak_dominant_drive_phrase(
    creatures1::objects::Object& creature_object) {
    creatures1::creatures::Creature* creature = creature_of(creature_object);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureSpeechPhraseHost phrase_host(document_);
    WindowsCreatureSpeechHost speech_output(document_);
    creature->speak_dominant_drive_phrase(document_, phrase_host,
                                          speech_output);
}

// --- WindowsNewObjectHost --------------------------------------------------

creatures1::objects::Object* WindowsNewObjectHost::adopt(
    std::unique_ptr<creatures1::objects::Object> object) {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (object == nullptr || runtime == nullptr) {
        return nullptr;
    }
    return &runtime->adopt_non_scenery_object(std::move(object));
}

creatures1::display::Gallery* WindowsNewObjectHost::acquire_gallery(
    std::uint32_t sprite_file_id, int header_record_index,
    std::uint32_t image_count, bool cache_protected) {
    return document_.acquire_gallery(sprite_file_id, header_record_index,
                                     image_count, cache_protected);
}

creatures1::objects::EntityRegistryHost&
WindowsNewObjectHost::entity_registry() {
    return document_.entity_registry();
}

void WindowsNewObjectHost::update_movement_bounds(
    creatures1::objects::SimpleObject& object) {
    document_.update_movement_bounds(object);
}

void WindowsNewObjectHost::remove_from_renderable_set(
    creatures1::objects::CompoundObject& object) {
    document_.renderables().erase(object);
}

void WindowsNewObjectHost::stop_continuous_sound(int sound_handle) {
    if (document_.sound_manager_available()) {
        document_.sound_manager().stop_continuous_sound(
            static_cast<std::uint32_t>(sound_handle), false);
    }
}

void WindowsNewObjectHost::release_gallery(
    creatures1::display::Gallery& gallery) {
    if (creatures1::world::WorldRuntime* runtime = document_.world_runtime()) {
        runtime->release_gallery(gallery);
    }
}

void WindowsNewObjectHost::unregister_from_object_registry(
    creatures1::objects::CompoundObject& object) {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return;
    }
    // Same bug as WindowsSkeletonLifetimeHost::unregister_from_object_
    // registry: this searched world_objects_ by index, then deleted that
    // index from creatures_ (WorldRuntime::remove_at) -- an unrelated
    // container. Every Vehicle/Lift/Blackboard/CompoundObject destroyed
    // this way leaked a dangling Object* in objects_ forever, corrupting
    // every later totl/enum count. Mirror the already-correct
    // Object::unregister_from_non_scenery_object_registry pattern for
    // objects_, then clean up world_objects_ via its real removal API.
    const std::size_t count = runtime->object_count();
    for (std::size_t index = 0; index < count; ++index) {
        if (runtime->object_at(index) == &object) {
            runtime->remove_object_at(index);
            break;
        }
    }
    runtime->remove_world_object(object);
}

void WindowsNewObjectHost::report_syntax_error(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::MacroSyntaxDiagnostic& diagnostic) {
    WindowsMacroHost host(document_);
    host.report_syntax_error(macro, diagnostic);
}

creatures1::objects::Object* WindowsNewObjectHost::create_call_button(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewCallButtonRequest& request) {
    static_cast<void>(macro);
    // The native adopts whatever Lift is currently active (g_Lift).
    auto button = std::make_unique<creatures1::objects::CallButton>(
        request.object_file_id,
        static_cast<int>(request.header_record_index), request.image_count,
        static_cast<int>(request.render_plane), document_.active_lift(),
        *this);
    return adopt(std::move(button));
}

creatures1::objects::Object* WindowsNewObjectHost::create_simple_object(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewSimpleObjectRequest& request) {
    static_cast<void>(macro);
    // Literal arguments recovered from the native call: origin position, no
    // bounds flags, classifier 0x02000000 (family 2 only), click selector
    // 0xff and no interaction flags.
    auto object = std::make_unique<creatures1::objects::SimpleObject>(
        request.object_file_id,
        static_cast<int>(request.header_record_index), request.image_count,
        request.cache_protected, 0, 0,
        static_cast<int>(request.render_plane),
        /*bounds_flags=*/0, /*classifier_event=*/0, /*classifier_species=*/0,
        /*classifier_genus=*/0, /*classifier_family=*/2,
        /*click_event_selector_0=*/0xff, /*reserved_word_0=*/0,
        /*reserved_word_1=*/0, /*interaction_event_flags=*/0, *this);
    return adopt(std::move(object));
}

creatures1::objects::Object* WindowsNewObjectHost::create_compound_object(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewCompoundObjectRequest& request) {
    static_cast<void>(macro);
    auto object = std::make_unique<creatures1::objects::CompoundObject>(
        request.sprite_file_id,
        static_cast<int>(request.header_record_index), request.image_count,
        request.cache_protected, *this);
    object->set_lifetime_host(&document_);
    return adopt(std::move(object));
}

creatures1::objects::Object* WindowsNewObjectHost::create_vehicle(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewVehicleRequest& request) {
    static_cast<void>(macro);
    auto vehicle = std::make_unique<creatures1::objects::Vehicle>(
        request.sprite_file_id,
        static_cast<int>(request.header_record_index), request.image_count,
        *this);
    vehicle->set_lifetime_host(&document_);
    return adopt(std::move(vehicle));
}

creatures1::objects::Object* WindowsNewObjectHost::create_blackboard(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewBlackboardRequest& request) {
    static_cast<void>(macro);
    auto blackboard = std::make_unique<creatures1::brain::Blackboard>(
        request.object_type,
        static_cast<int>(request.header_record_index), request.image_count,
        static_cast<std::uint8_t>(request.fill_palette_index),
        static_cast<std::uint8_t>(request.text_render_config_1),
        static_cast<std::uint8_t>(request.text_render_config_2),
        static_cast<std::uint8_t>(request.tile_x),
        static_cast<std::uint8_t>(request.tile_y), *this);
    blackboard->set_lifetime_host(&document_);
    return adopt(std::move(blackboard));
}

creatures1::objects::Object* WindowsNewObjectHost::create_scenery(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewSceneryRequest& request) {
    static_cast<void>(macro);
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return nullptr;
    }
    // The gallery is acquired with header record 0 and no cache protection,
    // which is the literal native call.
    creatures1::display::Gallery* gallery = acquire_gallery(
        request.construction_type, 0, request.image_count, false);
    auto scenery = std::make_unique<creatures1::objects::Scenery>(
        gallery, request.image_index,
        static_cast<int>(request.render_plane), entity_registry(), document_,
        document_.object_registry());
    // Scenery lives in its own registry, not the non-scenery one.
    return &runtime->adopt_scenery_object(std::move(scenery));
}

creatures1::objects::Object* WindowsNewObjectHost::create_lift(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewLiftRequest& request) {
    static_cast<void>(macro);
    auto lift = std::make_unique<creatures1::objects::Lift>(
        static_cast<std::uint32_t>(request.object_file_id),
        static_cast<int>(request.header_record_index), request.image_count,
        *this);
    lift->set_lifetime_host(&document_);
    creatures1::objects::Lift* raw = lift.get();
    creatures1::objects::Object* adopted = adopt(std::move(lift));
    if (adopted != nullptr) {
        WindowsCallButtonRuntimeHost runtime(document_);
        raw->initialize_state(runtime);
    }
    return adopted;
}

creatures1::objects::Object* WindowsNewObjectHost::create_creature(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewCreatureRequest& request) {
    static_cast<void>(macro);
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return nullptr;
    }

    WindowsCreatureConstructionHost construction(document_);
    auto creature = std::make_unique<creatures1::creatures::Creature>(
        request.genome_source_filename, request.construction_sex,
        construction);
    creatures1::creatures::Creature& adopted =
        runtime->adopt_creature(std::move(creature));
    // The constructor's native-order menu refresh precedes adopt_creature in
    // this composition.  Repeat it after adoption so NEW: CREA exposes the
    // newborn through the normal creature-selection path.
    document_.rebuild_creature_selection_menu();
    return &adopted.skeleton();
}

std::uint32_t WindowsNewObjectHost::generate_offspring_genome_file(
    creatures1::scripting::Macro& macro, std::uint32_t first_parent,
    std::uint32_t second_parent) {
    static_cast<void>(macro);
    // The crossover, mutation and unique-filename policies are already
    // translated and already composed for CAOS insemination; `new: gene` is
    // the same operation reached from a different verb.
    WindowsCreatureInseminationHost insemination(document_);
    return insemination.generate_offspring_genome_file(first_parent,
                                                       second_parent);
}

void WindowsNewObjectHost::create_part(
    creatures1::scripting::Macro& macro,
    const creatures1::scripting::NewPartRequest& request) {
    // ExecuteNewCommand @ 0041d130 reads Object::gallery_ptr, creates the
    // Entity at the origin, stores the requested offsets in CompoundPart,
    // and extends part_count. Movement applies the offsets later.
    auto* target = dynamic_cast<creatures1::objects::CompoundObject*>(
        macro.object_context.target_object);
    if (target == nullptr ||
        request.part_index >= creatures1::objects::CompoundObject::kPartCapacity) {
        return;
    }

    auto entity = std::make_unique<creatures1::objects::Entity>(
        &entity_registry());
    entity->set_render_plane(static_cast<int>(request.render_plane));
    entity->move_to(0, 0);
    entity->set_gallery(target->gallery());
    entity->set_image_index(static_cast<std::uint8_t>(request.image_index));
    entity->set_image_index_base(
        static_cast<std::uint8_t>(request.image_index));
    target->install_part(static_cast<std::size_t>(request.part_index),
                         std::move(entity),
                         static_cast<int>(request.local_x_offset),
                         static_cast<int>(request.local_y_offset));
}

// --- WindowsBlackboardHost -------------------------------------------------

creatures1::world::ViewportBounds
WindowsBlackboardHost::sound_viewport() const {
    return document_.sound_viewport();
}

creatures1::objects::Object* WindowsBlackboardHost::pointer_tool() const {
    return document_.pointer_tool();
}

void WindowsBlackboardHost::clear_text_input() {
    // g_text_input_buffer[0] = 0, g_text_input_length = 0 (SetEditMode,
    // FinalizeTextInput, Tick) -- the typed buffer, not the key ring.
    document_.clear_text_input_buffer();
}

void WindowsBlackboardHost::configure_text_input(
    creatures1::objects::Object* target, std::uint32_t maximum_length,
    std::uint32_t allowed_characters) {
    document_.configure_text_input(target, maximum_length, allowed_characters);
}

void WindowsBlackboardHost::update_text_input(
    creatures1::objects::Object& target, std::string_view text) {
    // Only the blackboard editor reaches this; the pointer tool has its own
    // republish path in update_text_input_target.
    auto* blackboard = dynamic_cast<creatures1::brain::Blackboard*>(&target);
    if (blackboard != nullptr) {
        blackboard->set_current_word_text(text, *this);
    }
}

creatures1::objects::EntityRasterHost&
WindowsBlackboardHost::raster_host() {
    return document_;
}

creatures1::objects::CompoundObjectMoveRedrawHost&
WindowsBlackboardHost::redraw_host() {
    return document_;
}

// --- MacroBlackboardHost ---------------------------------------------------

namespace {

// ExecuteBlackboardCAOSCommand @ 0041d8a0 admits the macro target when its
// classifier FAMILY is 3 and then treats it as a Blackboard outright.  The
// port additionally requires it to really be one: a family-3 object that is
// not a Blackboard would have the native reading another class's storage as
// word banks, which is not behaviour worth reproducing.
creatures1::brain::Blackboard* blackboard_target(
    const creatures1::scripting::Macro& macro) {
    creatures1::objects::Object* target = macro.object_context.target_object;
    if (target == nullptr ||
        ((target->classifier_base() >> 24) & 0xffu) != 3u) {
        return nullptr;
    }
    return dynamic_cast<creatures1::brain::Blackboard*>(target);
}

} // namespace

bool WindowsMacroHost::has_blackboard_target(
    const creatures1::scripting::Macro& macro) const {
    return blackboard_target(macro) != nullptr;
}

std::uint32_t WindowsMacroHost::current_word_index(
    const creatures1::scripting::Macro& macro) const {
    creatures1::brain::Blackboard* blackboard = blackboard_target(macro);
    return blackboard == nullptr ? 0u : blackboard->object_variable_0();
}

std::string WindowsMacroHost::current_word_text(
    const creatures1::scripting::Macro& macro) const {
    creatures1::brain::Blackboard* blackboard = blackboard_target(macro);
    if (blackboard == nullptr) {
        return {};
    }
    const std::uint32_t index = blackboard->object_variable_0();
    if (index >= creatures1::brain::Blackboard::kWordCount) {
        return {};
    }
    // The native tests only text[0] for the emit guard; the slot is a
    // NUL-terminated eleven-byte record.
    const auto& slot = blackboard->word(index).text;
    return std::string(slot.data(),
                       ::strnlen(slot.data(), slot.size()));
}

void WindowsMacroHost::announce_blackboard_word(
    creatures1::scripting::Macro& macro, bool spoken,
    std::uint32_t word_index, std::string_view word_text) {
    static_cast<void>(word_index);
    creatures1::brain::Blackboard* blackboard = blackboard_target(macro);
    if (blackboard == nullptr) {
        return;
    }

    WindowsCreatureFanoutHost fanout(document_);
    if (!spoken) {
        // `bbd: emit 0` only signals the creatures that can see the board.
        creatures1::creatures::queue_events_for_perceiving_creatures(
            *blackboard, creatures1::objects::ObjectEventId::event_7, fanout);
    } else {
        // `bbd: emit 1` shouts instead, and shows the word as a bubble.  The
        // native passes event argument 0 here -- the word offset visible in
        // the decompile sits in the unused ABI slot, not the argument.
        creatures1::creatures::queue_events_in_speech_range(
            *blackboard, creatures1::objects::ObjectEventId::event_7, 0, 0,
            fanout);
        create_speech_bubble(document_, *blackboard, word_text, 0x14);
    }

    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x4000,
                                      "A blackboard word has been read\n");
    }
}

void WindowsMacroHost::write_blackboard_word(
    creatures1::scripting::Macro& macro, std::uint32_t word_index,
    std::uint32_t value, std::string_view word_text) {
    creatures1::brain::Blackboard* blackboard = blackboard_target(macro);
    if (blackboard == nullptr ||
        word_index >= creatures1::brain::Blackboard::kWordCount) {
        return;
    }
    // Macro has already clamped the text to the ten source characters that
    // strcpy_s(dest, 0xb, ...) admits.
    auto& slot = blackboard->word(word_index).text;
    slot.fill('\0');
    const std::size_t length =
        std::min<std::size_t>(word_text.size(), slot.size() - 1);
    std::memcpy(slot.data(), word_text.data(), length);
    blackboard->set_word_value(word_index, value);
}

void WindowsMacroHost::set_blackboard_edit_mode(
    creatures1::scripting::Macro& macro, std::uint32_t enabled) {
    creatures1::brain::Blackboard* blackboard = blackboard_target(macro);
    if (blackboard == nullptr) {
        return;
    }
    WindowsBlackboardHost host(document_);
    blackboard->set_edit_mode(enabled != 0, host, host);
}

void WindowsMacroHost::redraw_blackboard(creatures1::scripting::Macro& macro,
                                          std::uint32_t mode) {
    creatures1::brain::Blackboard* blackboard = blackboard_target(macro);
    if (blackboard == nullptr) {
        return;
    }
    WindowsBlackboardHost host(document_);
    blackboard->redraw_display(mode != 0, host);
}

// --- MacroSystemHost -------------------------------------------------------

void WindowsMacroHost::move_main_window(int left, int top, int right,
                                        int bottom) {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr || frame->GetSafeHwnd() == nullptr) {
        return;
    }
    // Macro already converted the native width/height operands into a right
    // and bottom edge, which is the rectangle MoveWindow takes.
    frame->MoveWindow(left, top, right - left, bottom - top, TRUE);
}

void WindowsMacroHost::send_main_frame_command(std::uint32_t command) {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr || frame->GetSafeHwnd() == nullptr) {
        return;
    }
    frame->PostMessageA(WM_COMMAND, static_cast<WPARAM>(command), 0);
}

void WindowsMacroHost::enable_viewport_navigation() {
    // SetViewportNavigationMode(1) is the manual mode the document already
    // models as "navigation not disabled".
    document_.request_event_bar_viewport_origin(document_.viewport_left(),
                                                document_.renderer_viewport_top());
}

void WindowsMacroHost::set_viewport_origin(int x, int y) {

    document_.set_renderer_viewport_origin(x, y);
}

void WindowsMacroHost::open_world_file(std::string_view path) {
    if (path.empty()) {
        return;
    }
    document_.OnOpenDocument(CStringA(std::string(path).c_str()));
}

void WindowsMacroHost::set_ground_height(std::uint32_t x_block,
                                         std::uint32_t height) {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return;
    }
    // Macro has already applied the native `x_block <= 0x104` bound.
    runtime->map_data().ground_height_at(x_block) =
        static_cast<std::int32_t>(height);
}

void WindowsMacroHost::set_room_definition(std::uint32_t room_index, int left,
                                           int top, int right, int bottom,
                                           std::uint32_t room_type) {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return;
    }
    creatures1::world::MapData& map = runtime->map_data();
    if (room_index >= creatures1::world::MapData::kRoomCapacity) {
        return;
    }
    creatures1::world::MapRoom& room =
        map.room_at(static_cast<std::size_t>(room_index));
    room.bounds = {left, top, right, bottom};
    room.room_type = room_type;
    // The native `room` command grows the active count so a script can define
    // rooms past the end of the loaded table.
    if (static_cast<std::int32_t>(room_index) >= map.room_count()) {
        map.room_count() = static_cast<std::int32_t>(room_index) + 1;
    }
}

void WindowsMacroHost::bring_main_window_to_front() {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr && frame->GetSafeHwnd() != nullptr) {
        ::SetForegroundWindow(frame->GetSafeHwnd());
    }
}

void WindowsMacroHost::report_unsupported_language_version(
    creatures1::scripting::Macro& macro, std::uint32_t supported_version,
    std::uint32_t requested_version) {
    static_cast<void>(macro);
    // The native text names the running version twice and the required
    // version once, kills main-frame timer 1, shows a modal warning, and then
    // disables the world update timer.  Macro owns the termination itself.
    char message[512];
    std::snprintf(
        message, sizeof(message),
        "This version of Creatures (version %u.%u) needs to be updated to "
        "version %u.%u\n to run this macro script."
        "Contact the CyberLife website for Creatures updates.\n"
        "This script will be aborted and removed.\n",
        supported_version, 0u, requested_version, 0u);

    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 2, "%s", message);
    }
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr && frame->GetSafeHwnd() != nullptr) {
        ::KillTimer(frame->GetSafeHwnd(), 1);
    }
    AfxMessageBox(message, MB_ICONEXCLAMATION, 0);
    document_.set_world_update_timer_interval_ms(0);
}

void WindowsMacroHost::follow_macro_target(
    creatures1::scripting::Macro& macro) {
    // `sys: camt` centres the viewport on the macro's target, but only when
    // that target's sound source lies inside the navigation world bounds.
    creatures1::objects::Object* target = macro.object_context.target_object;
    if (target == nullptr) {
        // Native clears the renderer's followed creature and returns; the
        // port's renderer has no separate follow slot, so the manual mode set
        // by the caller is the whole effect.
        return;
    }
    // The renderer owns the centring and the clamp; request_viewport_origin
    // is the same entry the event bar and the creature-follow path use, so
    // the navigation-bounds test the native performs inline lives there.
    document_.request_renderer_origin(target->sound_source_x(),
                                      target->sound_source_y());
}

void WindowsMacroHost::set_dirty_world_rect(int left, int top, int right,
                                            int bottom) {
    // `sys: edit` stores the rectangle as the renderer's debug highlight and
    // dirties the viewport so it is outlined; it does not dirty the rectangle
    // itself, which is what this used to do.
    document_.set_renderer_debug_highlight_rect(left, top, right, bottom);
}

void WindowsMacroHost::remove_script_definition(
    creatures1::scripting::ScriptClassifier classifier) {
    creatures1::scripting::remove_script_definition_for_classifier(classifier);
}

// --- MacroStimulusHost -----------------------------------------------------

void WindowsMacroHost::queue_sign_stimulus(
    creatures1::objects::Object& source, std::int32_t stimulus_index) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_sign_stimulus_for_perceiving_creatures(
        source, stimulus_index, fanout);
}

void WindowsMacroHost::queue_tact_stimulus(
    creatures1::objects::Object& source, std::int32_t stimulus_index) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_tact_stimulus_for_overlapping_creatures(
        source, stimulus_index, fanout);
}

void WindowsMacroHost::queue_speech_range_stimulus(
    creatures1::objects::Object& source, std::int32_t stimulus_index) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_built_in_stimulus_in_speech_range(
        source, stimulus_index, fanout);
}

void WindowsMacroHost::queue_built_in_stimulus(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    std::int32_t stimulus_index) {
    creatures1::creatures::Creature* creature = creature_of(target);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_built_in_stimulus_for_creature(
        source, *creature, stimulus_index, fanout);
}

void WindowsMacroHost::queue_stimulus_for_perceiving(
    creatures1::objects::Object& source,
    creatures1::creatures::StimulusContext& stimulus) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_stimulus_for_perceiving_creatures(
        stimulus, source, fanout);
}

void WindowsMacroHost::queue_stimulus_for_overlapping(
    creatures1::objects::Object& source,
    creatures1::creatures::StimulusContext& stimulus) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_stimulus_for_overlapping_creatures(
        stimulus, source, fanout);
}

void WindowsMacroHost::queue_stimulus_in_speech_range(
    creatures1::objects::Object& source,
    creatures1::creatures::StimulusContext& stimulus) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_stimulus_in_speech_range(stimulus, source,
                                                          fanout);
}

void WindowsMacroHost::queue_direct_stimulus(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::creatures::StimulusContext& stimulus) {
    creatures1::creatures::Creature* creature = creature_of(target);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_stimulus_for_creature(stimulus, source,
                                                       *creature, fanout);
}

void WindowsMacroHost::report_invalid_stimulus_index(
    std::int32_t stimulus_index) const {
    WindowsCreatureFanoutHost fanout(document_);
    fanout.report_invalid_stimulus_index(stimulus_index);
}

// --- MacroMessageHost ------------------------------------------------------

void WindowsMacroHost::queue_perception_message(
    creatures1::objects::Object& source,
    creatures1::objects::ObjectEventId event_id) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_events_for_perceiving_creatures(
        source, event_id, fanout);
}

void WindowsMacroHost::queue_tactile_message(
    creatures1::objects::Object& source,
    creatures1::objects::ObjectEventId event_id) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_tact_events_for_overlapping_creatures(
        source, event_id, fanout);
}

void WindowsMacroHost::queue_speech_range_message(
    creatures1::objects::Object& source,
    creatures1::objects::ObjectEventId event_id) {
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_events_in_speech_range(source, event_id, 0, 0,
                                                        fanout);
}

// --- MacroSoundHost / MacroSoundPolicyHost ---------------------------------

void WindowsMacroHost::play_sound_effect(creatures1::objects::Object& object,
                                         creatures1::sound::SoundId sound_id,
                                         int queue_delay_ticks) {
    // `force_during_archive` is false for a CAOS-driven effect: only the
    // archive load path plays through a suspended world.
    object.play_sound_effect(sound_id, queue_delay_ticks, false, document_);
}

void WindowsMacroHost::set_continuous_sound(
    creatures1::objects::Object& object, creatures1::sound::SoundId sound_id,
    bool persist_when_out_of_range) {
    object.set_continuous_sound(sound_id, persist_when_out_of_range,
                                document_);
}

void WindowsMacroHost::fade_continuous_sound(
    creatures1::objects::Object& object) {
    object.fade_continuous_sound(document_);
}

void WindowsMacroHost::stop_continuous_sound(
    creatures1::objects::Object& object) {
    object.stop_continuous_sound(document_);
}

void WindowsMacroHost::load_sound_cache_if_audible(
    creatures1::objects::Object& object, creatures1::sound::SoundId sound_id) {
    // The native preloads only when the object is audible and the mixer is
    // actually able to play: audibility state, then not suspended, then the
    // backend ready, then FindOrLoadCacheEntry.  This was held on the belief
    // that no load-without-play entry existed -- SoundManager has had
    // find_or_load_cache_entry all along.
    if (object.sound_audibility_state(document_) ==
        creatures1::objects::SoundAudibilityState::out_of_range) {
        return;
    }
    creatures1::sound::SoundManager& manager = document_.sound_manager();
    if (manager.mixer_suspended() || !manager.backend_ready()) {
        return;
    }
    manager.find_or_load_cache_entry(sound_id);
}

void WindowsMacroHost::set_sound_foreground_policy() {
    document_.apply_view_sound_policy(
        creatures1::ui::SfcViewSoundPolicy::foreground_only);
}

void WindowsMacroHost::enable_sound_and_restore_if_ready() {
    document_.apply_view_sound_policy(
        creatures1::ui::SfcViewSoundPolicy::enable);
}

void WindowsMacroHost::disable_sound_and_suspend_if_ready() {
    document_.apply_view_sound_policy(
        creatures1::ui::SfcViewSoundPolicy::disable);
}

void WindowsMacroHost::set_sound_conservative_policy() {
    document_.apply_view_sound_policy(
        creatures1::ui::SfcViewSoundPolicy::plays_unfocused);
}

// --- MacroDdeHost -----------------------------------------------------------
//
// Macro::ExecuteDDECommand is the interface the Creatures kits drive the game
// through, so this family is bound.  Each query below either reaches recovered
// policy or reports the gap and returns the native failure value; none of them
// silently succeeds.

namespace {

void report_unrecovered_dde_query(const char* query) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x2, "CAOS dde: %s: rendering not recovered yet\n",
            query);
    }
}

// NotifyDDEScoreChanged crosses into the embedded kits through record 8's
// IDispatch; the document owns that broadcast.
class DocumentScoreNotification final
    : public creatures1::scripting::DdeScoreNotificationHost {
public:
    explicit DocumentScoreNotification(C1WindowsDocument& document)
        : document_(document) {}

    bool score_notification_endpoint_available() const override {
        return true;
    }
    void notify_score_changed() override {
        document_.broadcast_embedded_control_state(8);
    }

private:
    C1WindowsDocument& document_;
};

} // namespace

void WindowsMacroHost::adjust_score(
    creatures1::scripting::DdeScoreCounter counter, std::int32_t delta) {
    document_.adjust_document_score(counter, delta);
}

void WindowsMacroHost::notify_score_changed() {
    DocumentScoreNotification notification(document_);
    creatures1::scripting::notify_dde_score_changed(notification);
}

void WindowsMacroHost::pan_view_to_selected_creature() {
    document_.pan_view_to_selected_creature();
}

namespace {

// Every getb/putb subcommand works on the macro's CURRENT TARGET and admits
// only classifier family 4, the Creature family per ClassifierNames.txt.  The
// selected creature is a different thing entirely and is not what these read.
creatures1::creatures::Creature* dde_creature_target(
    creatures1::scripting::Macro& macro, C1WindowsDocument& document) {
    creatures1::objects::Object* target = macro.object_context.target_object;
    if (target == nullptr ||
        ((target->classifier_base() >> 24) & 0xffu) != 4u) {
        return nullptr;
    }
    return document.mutable_creature_for_object(*target);
}

} // namespace

std::optional<std::string> WindowsMacroHost::query_getb(
    creatures1::scripting::Macro& macro,
    creatures1::scripting::DdeGetBQuery query) {
    creatures1::creatures::Creature* creature =
        dde_creature_target(macro, document_);
    // Every getb query reads the macro's creature target except `ovvd`, which
    // native answers from the selection array whatever TARG is.
    if (creature == nullptr &&
        query != creatures1::scripting::DdeGetBQuery::selected_creature_status) {
        return std::nullopt;
    }
    switch (query) {
    case creatures1::scripting::DdeGetBQuery::creature_name:
        return creature->register_state().history().display_name;
    case creatures1::scripting::DdeGetBQuery::genome_source: {
        // `%lx` of the skeleton's genome filename id, not the history string.
        char formatted[32] = {0};
        std::snprintf(formatted, sizeof(formatted), "%lx",
                      static_cast<unsigned long>(
                          creature->skeleton().genome_source_filename));
        return std::string(formatted);
    }
    case creatures1::scripting::DdeGetBQuery::creature_history: {
        // Ten fields, each TERMINATED by a bar rather than separated by one.
        std::string record;
        for (const std::string* field :
             creature->register_state().history_entries()) {
            record += *field;
            record.push_back('|');
        }
        return record;
    }
    case creatures1::scripting::DdeGetBQuery::creature_age: {
        // age_ticks/600 is minutes; the native prints "%2d:%2d" and then
        // replaces a leading space in the minutes field with a zero.
        const std::uint32_t minutes =
            creature->register_state().age_ticks() / 600u;
        char formatted[32] = {0};
        const int written = std::snprintf(formatted, sizeof(formatted),
                                          "%2u:%2u", minutes / 60u,
                                          minutes % 60u);
        if (written >= 2 && formatted[written - 2] == ' ') {
            formatted[written - 2] = '0';
        }
        return std::string(formatted);
    }
    case creatures1::scripting::DdeGetBQuery::selected_creature_status: {
        // `getb ovvd` walks the creature SELECTION array -- not the registry --
        // and joins one status record per entry with '&'.  Unlike every other
        // getb query it ignores the macro target entirely.
        // The five status strings come straight from the string table, the
        // same way every other platform load_string in this tree does it.
        const auto resource_string = [](std::uint32_t id) {
            CStringA text;
            return text.LoadStringA(static_cast<UINT>(id))
                       ? std::string(text.GetString())
                       : std::string{};
        };
        creatures1::creatures::Creature::StatusStrings strings;
        strings.sick = resource_string(0xef2b);
        strings.healthy = resource_string(0xef2c);
        strings.dead = resource_string(0xef2d);
        strings.not_pregnant = resource_string(0xef2f);
        strings.male = resource_string(0xef30);

        creatures1::world::MapRoomTable rooms{};
        if (creatures1::world::WorldRuntime* runtime =
                document_.world_runtime()) {
            rooms = runtime->map_data().room_table();
        }

        std::string joined;
        for (std::size_t index = 0; index < document_.selection_count();
             ++index) {
            auto* selected = static_cast<creatures1::creatures::Creature*>(
                document_.selection_at(index));
            if (selected == nullptr) {
                continue;
            }
            if (!joined.empty()) {
                joined.push_back('&');
            }
            joined += selected->format_status_for_external_query(rooms,
                                                                 strings);
        }
        return joined;
    }
    }
    return std::nullopt;
}

bool WindowsMacroHost::update_putb(
    creatures1::scripting::Macro& macro,
    creatures1::scripting::DdePutBCommand command, std::string_view text) {
    creatures1::creatures::Creature* creature =
        dde_creature_target(macro, document_);
    if (creature == nullptr) {
        return false;
    }
    if (command == creatures1::scripting::DdePutBCommand::creature_name) {
        creature->register_state().history().display_name = std::string(text);
        return true;
    }

    // `putb data` splits the argument on bars into the ten history fields,
    // stopping at the first field with no terminator.
    const std::array<std::string*, 10> fields =
        creature->register_state().history_entries();
    std::string_view remaining(text);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const std::size_t separator = remaining.find('|');
        if (separator == std::string_view::npos) {
            break;
        }
        *fields[index] = std::string(remaining.substr(0, separator));
        remaining.remove_prefix(separator + 1);
    }

    // The name the creature now carries becomes the word it answers to: the
    // native copies it into learned word slot 0x10 as both the recognised and
    // the spoken form, at full reinforcement.
    document_.rebuild_creature_selection_menu();
    const std::string& display_name =
        creature->register_state().history().display_name;
    creatures1::creatures::LearnedWordRecord& record =
        creature->learned_word_record(0x10);
    const std::size_t copied = (std::min)(
        display_name.size(),
        creatures1::creatures::kLearnedWordTextCapacity - 1);
    std::memcpy(record.recognized_word, display_name.data(), copied);
    record.recognized_word[copied] = '\0';
    std::memcpy(record.response_word, display_name.data(), copied);
    record.response_word[copied] = '\0';
    record.reinforcement = 0xff;

    if (document_.is_selected_creature(creature->skeleton())) {
        document_.broadcast_embedded_control_state(7);
        document_.update_eye_view_title();
    }
    return true;
}

std::string WindowsMacroHost::render_learned_words(
    creatures1::scripting::Macro& macro) {
    static_cast<void>(macro);
    report_unrecovered_dde_query("getb/putv learned words");
    return {};
}

std::string WindowsMacroHost::render_brain_lobe(
    creatures1::scripting::Macro& macro) {
    // ExecuteDDECommand's `lobe` branch is a BINARY payload, not text: a
    // count byte, then five bytes per lobe, then 0xff, with the length set
    // to count * 5 + 2.  The five come from CLobe offsets 4, 8, 12, 16 and
    // 34 -- grid x, grid y, width, height and the winner-take-all flags,
    // which is exactly what a brain viewer needs to lay the lobe grid out.
    // CBrain::lobes starts at offset 8, which is what makes the native
    // cursor (brain + 0x10, striding 200) land on lobe+4.
    //
    // The trailing 0xff is deliberate: ExecuteToOutputBuffer overwrites the
    // last byte with the NUL, so 0xff is the sacrificial separator here and
    // this payload must NOT also go through append_pipe_field.
    creatures1::creatures::Creature* creature =
        dde_creature_target(macro, document_);
    if (creature == nullptr) {
        return {};
    }
    creatures1::brain::Brain* brain = creature->brain();
    if (brain == nullptr) {
        return {};
    }
    const std::uint32_t count = brain->lobe_count();
    std::string payload;
    payload.reserve(count * 5u + 2u);
    payload.push_back(static_cast<char>(static_cast<std::uint8_t>(count)));
    for (std::uint32_t index = 0; index < count; ++index) {
        const creatures1::brain::Lobe& lobe = brain->lobe(index);
        payload.push_back(
            static_cast<char>(static_cast<std::uint8_t>(lobe.grid_x_offset_value())));
        payload.push_back(
            static_cast<char>(static_cast<std::uint8_t>(lobe.grid_y_offset_value())));
        payload.push_back(
            static_cast<char>(static_cast<std::uint8_t>(lobe.grid_width_value())));
        payload.push_back(
            static_cast<char>(static_cast<std::uint8_t>(lobe.grid_height_value())));
        payload.push_back(
            static_cast<char>(lobe.winner_take_all_flags_value()));
    }
    payload.push_back(static_cast<char>(0xff));
    return payload;
}

std::string WindowsMacroHost::render_gene_counts(
    creatures1::scripting::Macro& macro) {
    // ExecuteDDECommand's `gene` branch reads g_selected_creature -- the
    // SELECTED creature, not the macro's target -- rebuilds a temporary
    // CGenome from its genome filename, gender and life stage, and prints
    // fourteen "%d|" fields straight out of the resulting count report.  The
    // policy is already in build_current_creature_genome_gene_counts; only
    // the selection and the file store come from here.
    static_cast<void>(macro);
    creatures1::creatures::Creature* creature = document_.selected_creature();
    if (creature == nullptr) {
        return {};
    }
    creatures1::creatures::GenomeGeneCountSelection selection;
    selection.source_filename = creature->skeleton().genome_source_filename;
    selection.sex = creature->genome_sex();
    selection.life_stage =
        static_cast<creatures1::creatures::GenomeLifeStage>(
            creature->genome_life_stage());
    const creatures1::creatures::GenomeGeneCountReport report =
        creatures1::creatures::build_current_creature_genome_gene_counts(
            selection, document_.genome_files());

    // The report's fourteen counts in declaration order; native walks them as
    // a flat array from &brain_lobe_gene_count.
    const creatures1::creatures::GenomeGeneCount counts[14] = {
        report.brain_lobe_gene_count,
        report.biochemistry_receptor_gene_count,
        report.biochemistry_emitter_gene_count,
        report.biochemistry_reaction_gene_count,
        report.biochemistry_half_life_gene_count,
        report.biochemistry_initial_chemical_gene_count,
        report.creature_stimulus_gene_count,
        report.creature_genus_gene_count,
        report.creature_appearance_gene_count,
        report.creature_pose_gene_count,
        report.creature_gait_gene_count,
        report.creature_instinct_gene_count,
        report.creature_life_stage_gene_count,
        report.total_creature_life_stage_advance_loci,
    };
    std::string rendered;
    for (const creatures1::creatures::GenomeGeneCount count : counts) {
        rendered += std::to_string(static_cast<int>(count));
        rendered += '|';
    }
    return rendered;
}


std::string WindowsMacroHost::render_cell_values(
    creatures1::scripting::Macro& macro, std::uint32_t cell,
    std::uint32_t variable, std::uint32_t field) {
    // ExecuteDDECommand's `cell` branch.  The three operands are a lobe
    // index, a neuron index and a dendrite-rule index, whatever the
    // parameter names inherited from the CAOS spelling suggest:
    //
    //   brain + 0xa4 + lobe * 200   is CLobe::neurons (offset 156)
    //   + neuron * 0x10             is &neurons[n]    (16-byte neuron)
    //   neuron + 0xc + rule         is that rule's dendrite count
    //   neuron + 4 + rule * 4       is that rule's dendrite list
    //
    // then it walks the dendrites summing four bytes at connection offsets
    // 6, 7, 8 and 9 -- current_weight, target_weight, baseline_weight and
    // dendrite_state -- and prints seven "%d|" fields.  The trailing bar is
    // the sacrificial byte the reply terminator overwrites, so the caller
    // appends this verbatim.
    const std::uint32_t lobe_index = cell;
    const std::uint32_t neuron_index = variable;
    const std::uint32_t rule_index = field;
    creatures1::creatures::Creature* creature =
        dde_creature_target(macro, document_);
    if (creature == nullptr) {
        return {};
    }
    creatures1::brain::Brain* brain = creature->brain();
    if (brain == nullptr || lobe_index >= brain->lobe_count() ||
        rule_index > 1u) {
        return {};
    }
    const creatures1::brain::Lobe& lobe = brain->lobe(lobe_index);
    if (neuron_index >= lobe.neuron_count_value()) {
        return {};
    }
    const creatures1::brain::LobeNeuron& neuron = lobe.neuron(neuron_index);

    const std::uint8_t dendrite_count =
        rule_index == 0 ? neuron.rule0_connection_count
                        : neuron.rule1_connection_count;
    const creatures1::brain::LobeConnection* dendrites =
        rule_index == 0 ? neuron.rule0_connections_begin
                        : neuron.rule1_connections_begin;

    std::uint32_t current_weight_sum = 0;
    std::uint32_t target_weight_sum = 0;
    std::uint32_t baseline_weight_sum = 0;
    std::uint32_t dendrite_state_sum = 0;
    for (std::uint8_t index = 0; dendrites != nullptr && index < dendrite_count;
         ++index) {
        current_weight_sum += dendrites[index].current_weight;
        target_weight_sum += dendrites[index].target_weight;
        baseline_weight_sum += dendrites[index].baseline_weight;
        dendrite_state_sum += dendrites[index].dendrite_state;
    }

    char formatted[128] = {0};
    std::snprintf(formatted, sizeof(formatted), "%d|%d|%d|%d|%d|%d|%d|",
                  static_cast<int>(neuron.firing_strength),
                  static_cast<int>(neuron.activation),
                  static_cast<int>(dendrite_count),
                  static_cast<int>(current_weight_sum),
                  static_cast<int>(target_weight_sum),
                  static_cast<int>(baseline_weight_sum),
                  static_cast<int>(dendrite_state_sum));
    return std::string(formatted);
}


bool WindowsMacroHost::capture_picture(creatures1::scripting::Macro& macro,
                                       std::string& output_path) {
    static_cast<void>(macro);
    static_cast<void>(output_path);
    report_unrecovered_dde_query("pict (screen capture)");
    return false;
}

} // namespace creatures1::platform
