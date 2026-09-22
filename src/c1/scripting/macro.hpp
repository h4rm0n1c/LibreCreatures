#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tables.hpp"
#include "../sound/sound.hpp"

namespace creatures1::objects {
class Object;
enum class ObjectEventId : std::uint32_t;
}

namespace creatures1::creatures {
enum class CreatureConstructionSex : std::uint32_t;
struct StimulusContext;
}

namespace creatures1::scripting {

class Macro;
class ScriptDefinitionInstallHost;
struct MacroInterpreterBindings;

using CaosToken = std::uint32_t;

// CAOS stores four-character words as little-endian 32-bit tokens in the
// script stream.  Keeping the spelling here makes the interpreter's command
// inventory readable and gives later handler code a typed vocabulary instead
// of scattering packed hexadecimal constants through Macro.cpp.
constexpr CaosToken make_caos_token(std::string_view spelling) {
    return spelling.size() == 4
               ? static_cast<CaosToken>(
                     static_cast<unsigned char>(spelling[0])) |
                     (static_cast<CaosToken>(
                          static_cast<unsigned char>(spelling[1]))
                      << 8) |
                     (static_cast<CaosToken>(
                          static_cast<unsigned char>(spelling[2]))
                      << 16) |
                     (static_cast<CaosToken>(
                          static_cast<unsigned char>(spelling[3]))
                      << 24)
               : 0;
}

// Four-byte CAOS vocabulary recovered from Macro::ExecuteInterpreter
// (0x0041dc40).  Prefix-style families and their nested subcommands are kept in
// MacroPrefixCommand/MacroPrefixSubcommand below; this enum is deliberately
// an inventory first, so command ownership can be translated one family at a
// time without reintroducing a monolithic decompiler-shaped dispatcher.
enum class MacroCommand : CaosToken {
    do_if = make_caos_token("doif"),
    else_ = make_caos_token("else"),
    end_if = make_caos_token("endi"),
    loop = make_caos_token("loop"),
    ever = make_caos_token("ever"),
    until = make_caos_token("untl"),
    repeat = make_caos_token("repe"),
    repeat_counted = make_caos_token("reps"),
    next = make_caos_token("next"),
    global_subroutine = make_caos_token("gsub"),
    subroutine = make_caos_token("subr"),
    return_from_subroutine = make_caos_token("retn"),
    stop = make_caos_token("stop"),
    kill = make_caos_token("kill"),
    set_variable = make_caos_token("setv"),
    add_variable = make_caos_token("addv"),
    subtract_variable = make_caos_token("subv"),
    multiply_variable = make_caos_token("mulv"),
    divide_variable = make_caos_token("divv"),
    modulo_variable = make_caos_token("modv"),
    bitwise_and_variable = make_caos_token("andv"),
    bitwise_or_variable = make_caos_token("orrv"),
    negate_variable = make_caos_token("negv"),
    randomize_variable = make_caos_token("rndv"),
    move_by = make_caos_token("mvby"),
    move_to = make_caos_token("mvto"),
    instantiate = make_caos_token("inst"),
    script = make_caos_token("scrp"),
    script_extended = make_caos_token("scrx"),
    endm = make_caos_token("endm"),
    mate = make_caos_token("mate"),
    delete_creature = make_caos_token("cdie"),
    dream = make_caos_token("drea"),
    sleep = make_caos_token("aslp"),
    drop = make_caos_token("drop"),
    touch = make_caos_token("touc"),
    part = make_caos_token("part"),
    execute = make_caos_token("exec"),
    quit = make_caos_token("quit"),
    over = make_caos_token("over"),
    done = make_caos_token("done"),
    edit = make_caos_token("edit"),
    base = make_caos_token("base"),
    pose = make_caos_token("pose"),
    behavior = make_caos_token("bhvr"),
    spot = make_caos_token("spot"),
    sound_start = make_caos_token("snde"),
    sound_change = make_caos_token("sndc"),
    sound_loop = make_caos_token("sndl"),
    sound_fade = make_caos_token("sndf"),
    sound_query = make_caos_token("sndq"),
    sound_voice = make_caos_token("sndv"),
    sound_stop_channel = make_caos_token("stpc"),
    fade = make_caos_token("fade"),
    tele = make_caos_token("tele"),
    say_hash = make_caos_token("say#"),
    say_dollar = make_caos_token("say$"),
    say_name = make_caos_token("sayn"),
    message = make_caos_token("mesg"),
    target = make_caos_token("targ"),
    from_object = make_caos_token("from"),
    stimulus = make_caos_token("stim"),
    speech_range_stimulus = make_caos_token("shou"),
    sign_stimulus = make_caos_token("sign"),
    tact_stimulus = make_caos_token("tact"),
    event = make_caos_token("evnt"),
    remove_event = make_caos_token("rmev"),
    trig = make_caos_token("trig"),
    chem = make_caos_token("chem"),
    debug_message = make_caos_token("dbgm"),
    debug_value = make_caos_token("dbgv"),
    dbug = make_caos_token("dbug"),
    vocabulary = make_caos_token("vocb"),
    version = make_caos_token("vrsn"),
    room = make_caos_token("room"),
    sound_pause = make_caos_token("plds"),
    vehicle_grab_passengers = make_caos_token("gpas"),
    vehicle_set_passenger = make_caos_token("spas"),
    cabinet = make_caos_token("cabn"),
    knob = make_caos_token("knob"),
    slim = make_caos_token("slim"),
    tick = make_caos_token("tick"),
    enumerate_objects = make_caos_token("enum"),
    random_target = make_caos_token("rtar"),
    drive_presentation = make_caos_token("dpas"),
    mcrt = make_caos_token("mcrt"),
    tool = make_caos_token("tool"),
    walk = make_caos_token("walk"),
    pointer = make_caos_token("poin"),
    wait = make_caos_token("wait"),
    approach = make_caos_token("appr"),
    uppercase_approach = make_caos_token("APPR"),
    anim = make_caos_token("anim"),
    preload_image_sequence = make_caos_token("prld"),
    set_creature_action_activation_boost = make_caos_token("impt"),
    fire = make_caos_token("fire"),
    stm_hash = make_caos_token("stm#"),
    hidden_fertilize = make_caos_token("f**k"),
    less_than_creature = make_caos_token("ltcy"),
    bacterium_update = make_caos_token("snez"),
};

// The direct equality inventory above is supplemented by these prefix-style
// families in the native dispatcher.  The application and aim prefixes are
// retained as distinct boundaries even though their handlers are still held;
// they must not be silently folded into sys:/dde: or ordinary commands.
enum class MacroPrefixCommand : CaosToken {
    application = make_caos_token("app:"),
    aim = make_caos_token("aim:"),
    blackboard = make_caos_token("bbd:"),
    dde = make_caos_token("dde:"),
    system = make_caos_token("sys:"),
    new_object = make_caos_token("new:"),
};

enum class MacroPrefixSubcommand : CaosToken {
    write = make_caos_token("writ"),
    sign = make_caos_token("sign"),
    tact = make_caos_token("tact"),
    speech_range = make_caos_token("shou"),
    sound_foreground = make_caos_token("fore"),
    sound_on = make_caos_token("on__"),
    sound_off = make_caos_token("off_"),
    sound_conservative = make_caos_token("cons"),
};

enum class MacroApplicationSubcommand : CaosToken {
    quit = make_caos_token("quit"),
};

enum class MacroStimulusSubcommand : CaosToken {
    write = make_caos_token("writ"),
    from = make_caos_token("from"),
    sign = make_caos_token("sign"),
    tact = make_caos_token("tact"),
    speech_range = make_caos_token("shou"),
};

// `mesg` has the same four nested spellings as `stm#`, but its payload is an
// ObjectEventId rather than a built-in stimulus index.  Keep a separate enum
// so a future interpreter dispatcher cannot accidentally route message event
// ids through the stimulus-context path.
enum class MacroMessageSubcommand : CaosToken {
    write = make_caos_token("writ"),
    sign = make_caos_token("sign"),
    tact = make_caos_token("tact"),
    speech_range = make_caos_token("shou"),
};

// This is a routing classification, not an execution result.  It keeps the
// outer VM dispatcher readable and gives each recovered command family a
// source owner before its remaining native contracts are translated.
enum class MacroCommandFamily : std::uint8_t {
    control_flow,
    arithmetic_and_lvalue,
    object_motion,
    object_appearance,
    object_and_script_lifecycle,
    compound_geometry,
    creature_motor,
    creature_runtime,
    sound,
    speech_and_messaging,
    stimulus_and_events,
    debug_system_and_misc,
    vehicle_interaction,
    prefixed,
    unknown,
};

// The comparison word following `doif`/`untl` is a two-byte CAOS operator,
// not a four-byte command token.  These values are kept separate from the
// command inventory so the interpreter cannot accidentally treat a parser
// operator as a dispatch opcode.
enum class MacroComparisonOperator : std::uint16_t {
    equal = 0x7165,              // eq
    not_equal = 0x656e,          // ne
    greater_than = 0x7467,       // gt
    less_than = 0x746c,          // lt
    greater_or_equal = 0x6567,   // ge
    less_or_equal = 0x656c,      // le
    bit_set = 0x7462,             // bt
    bit_clear = 0x6662,           // fb
};

enum class MacroControlFlowResult : std::uint8_t {
    not_control_flow,
    iteration_complete,
    cursor_changed,
    execution_terminated,
    interpreter_returned,
    scheduler_cleanup_required,
    // The handler has already removed and destroyed the Macro.  Native `touc`
    // calls RemoveFromRunningSchedulerAndDestroy and then returns straight out
    // of ExecuteInterpreter; the interpreter must do the same and touch no
    // member afterwards.
    macro_destroyed,
};

// Result of the native ExecuteInterpreter post-command boundary.  This is
// deliberately separate from MacroControlFlowResult: command handlers report
// what happened to the CAOS cursor, while the outer interpreter reports what
// the scheduler must do with the Macro instance.
enum class MacroInterpreterFinalization : std::uint8_t {
    continue_dispatch,
    return_to_caller,
    remove_from_scheduler_and_retain,
    remove_from_scheduler_and_destroy,
};

// Optional runtime instrumentation for the recovered ExecuteInterpreter
// outer loop.  This is deliberately an observer, not an execution callback:
// command ownership and scheduler policy remain in Macro and its typed host
// bindings.  A concrete scheduler can use the events to build a per-token
// execution ledger and compare it with the static native-site inventory.
class MacroInterpreterTrace {
public:
    virtual ~MacroInterpreterTrace() = default;

    virtual void command_fetched(Macro& macro, CaosToken token,
                                 std::size_t command_start) = 0;
    virtual void command_result(Macro& macro, CaosToken token,
                                MacroControlFlowResult result,
                                std::size_t cursor_after_dispatch) = 0;
    virtual void iteration_finalized(
        Macro& macro, MacroInterpreterFinalization finalization,
        std::size_t cursor_after_finalization) = 0;
    virtual void execution_exception(Macro& macro) = 0;
    virtual void scheduler_action(Macro& macro,
                                  MacroInterpreterFinalization action) = 0;
};

struct MacroSyntaxDiagnostic {
    std::string expected_token_type;
    CaosToken offending_token = 0;
    std::string script_excerpt;
};

// `enum`, `next`, and `rtar` all use the native three-component classifier
// query. Keep the query readable at the scripting boundary; packing it into
// the Object classifier word remains an Object/runtime concern.
struct MacroClassifierPattern {
    std::uint32_t family = 0;
    std::uint32_t genus = 0;
    std::uint32_t species = 0;
};

struct MacroExecutionExceptionDiagnostic {
    CaosToken offending_token = 0;
    std::string script_excerpt;
    // Native HandleMacroScriptExecutionException snapshots the owning
    // object's packed classifier before it reports the fault.  Keep that
    // evidence in the semantic diagnostic; the application host formats the
    // classifier and owns the debug-console/message-box/timer boundary.
    ScriptClassifier owning_classifier{};
};

class MacroCommandHost {
public:
    virtual ~MacroCommandHost() = default;
    virtual void report_syntax_error(Macro& macro,
                                     const MacroSyntaxDiagnostic& diagnostic) = 0;
};

enum class MacroCreatureValue : std::uint8_t {
    chemical_concentration,
    gender,
    death_state,
    asleep,
    winning_drive,
    drive_level,
    life_stage,
    child_genome_source_filename,
};

enum class MacroViewportValue : std::uint8_t {
    width,
    height,
    center_x,
    center_y,
};

enum class MacroCreatureAssignment : std::uint8_t {
    child_genome_source_filename,
};

// ParseRValue and AssignLValue are Macro-owned CAOS language operations. The
// values they read or mutate outside Macro are supplied through this typed
// runtime boundary; no command host is allowed to masquerade as the parser.
class MacroRuntimeHost {
public:
    virtual ~MacroRuntimeHost() = default;

    // A CAOS value is an untyped 32-bit word, so a script can hand any
    // integer to a command that expects an object -- `setv var0 12345,targ
    // var0` is legal CAOS.  The original dereferences whatever it is given
    // and faults; the port asks the world whether the pointer is one of its
    // live objects before using it.  The adapter must answer from the real
    // registries: a false negative would drop a legitimate object and break
    // the script instead.
    virtual bool is_live_object(const objects::Object* object) const = 0;

    // Invalid object values are recoverable CAOS faults. Application hosts
    // report them through the normal game logger; small test/runtime adapters
    // may keep the default no-op.
    virtual void report_invalid_object_reference(
        std::uint32_t value) const {
        static_cast<void>(value);
    }

    virtual bool is_creature_object(const objects::Object& object) const = 0;
    virtual bool is_simple_object(const objects::Object& object) const = 0;
    virtual bool is_compound_object(const objects::Object& object) const = 0;
    virtual bool is_vehicle_object(const objects::Object& object) const = 0;
    // XVEC/YVEC read the vehicle's movement vector in 1/256ths of a pixel.
    // Only a Vehicle carries one, so the adapter owns the ownership check.
    virtual std::int32_t vehicle_movement_vector_x(
        const objects::Object& object) const = 0;
    virtual std::int32_t vehicle_movement_vector_y(
        const objects::Object& object) const = 0;
    // The write side of the same pseudo-variables: a plain, unscaled store
    // into the vehicle's own velocity field (native AssignLValue @
    // 0x0041b9f0, confirmed via disassembly -- MOV [target+0x144/0x148],EAX,
    // nothing else touched).
    virtual void set_vehicle_movement_vector_x(
        objects::Object& object, std::int32_t value) = 0;
    virtual void set_vehicle_movement_vector_y(
        objects::Object& object, std::int32_t value) = 0;
    virtual std::uint32_t creature_value(
        const objects::Object& object, MacroCreatureValue value,
        std::uint32_t index) const = 0;
    virtual std::uint32_t attention_record_index(
        const objects::Object& object) const = 0;
    virtual std::int32_t ground_height(std::uint32_t x_block) const = 0;
    virtual std::uint32_t room_count() const = 0;
    virtual std::int32_t room_value(std::uint32_t room_index,
                                    std::uint32_t field) const = 0;
    virtual std::int32_t ambient_temperature_at(
        const objects::Object& object) const = 0;
    virtual std::int8_t ambient_wind() const = 0;
    virtual std::uint32_t score_value(std::uint32_t index) const = 0;
    virtual std::uint32_t world_tick_count() const = 0;
    virtual std::uint32_t language_version() const = 0;
    virtual std::uint32_t sound_settings() const = 0;
    virtual std::uint32_t viewport_value(MacroViewportValue value) const = 0;
    virtual objects::Object* selected_creature() const = 0;
    virtual objects::Object* pointer_tool() const = 0;
    virtual objects::Object* edit_object() const = 0;
    virtual objects::Object* topmost_pointer_object() const = 0;
    virtual std::uint32_t enabled_object_count_matching(
        std::uint32_t family, std::uint32_t genus,
        std::uint32_t species) const = 0;
    // `enum`/`next` walk the runtime non-scenery pointer registry.  The
    // registry is application-owned and populated at runtime; Macro owns
    // only the classifier mask, continuation tuple, and CAOS cursor rules.
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual objects::Object* non_scenery_object_at(
        std::size_t index) const = 0;
    // Native `rtar` scans the same registry and applies the same
    // IsSoundSourceBelowWorldY exclusion as `enum`, then chooses one match
    // through the process CRT random source. The registry, predicate, and
    // random-source boundary stay outside Macro.
    virtual objects::Object* random_non_scenery_object(
        const MacroClassifierPattern& pattern) const = 0;
    // Native `poin` is a Creature-only dispatch to the inherited Skeleton
    // guarded pose selector.  The return values 0, 1, and 0xffffffff remain
    // meaningful to Macro because only zero rewinds the command token.
    virtual std::uint32_t select_creature_target_pose_for_motion(
        objects::Object& object, bool force_interaction_pose) = 0;
    // Native `touc` uses the same guarded Skeleton pose selector with
    // force=false. A -1 result then clears only the selected decision neuron;
    // Macro itself owns the command cursor and scheduler transition.
    virtual void clear_creature_selected_decision_neuron(
        objects::Object& object) = 0;
    // `appr` selects the Creature-owned walking gait and rewrites its
    // consumed command token to the native uppercase continuation.  The
    // continuation (`APPR`) asks the Creature/Skeleton adapter whether the
    // lead foot is within the native 0x36 horizontal threshold and, when it
    // is, clears the animation sequence and cursor.  Macro owns only the
    // five-byte token rewrite and retry cursor movement.
    virtual void select_creature_walk_gait(objects::Object& object) = 0;
    virtual bool creature_approach_is_ready(
        const objects::Object& object) const = 0;
    virtual void reset_creature_animation_sequence(
        objects::Object& object) = 0;

    virtual void set_selected_creature(objects::Object* creature,
                                       bool notify) = 0;
    // Top-level `edit` publishes the current target as the application's
    // edit object.  The application owns the global/selection lifetime;
    // Macro owns only the null-target guard and command routing.
    virtual void set_edit_object(objects::Object* object) = 0;
    virtual void set_viewport_value(MacroViewportValue value,
                                    std::uint32_t new_value) = 0;
    virtual void set_creature_value(objects::Object& object,
                                    MacroCreatureAssignment field,
                                    std::uint32_t value) = 0;
    // `chem` mutates the target Creature's byte-sized concentration table.
    // The Creature/Biochemistry adapter owns saturation and the optional
    // category-0x10 diagnostic log; Macro owns only operand order and target
    // selection.
    virtual void add_creature_chemical_moles(
        objects::Object& object, std::uint32_t chemical_index,
        std::uint32_t moles) = 0;
    // `fire` targets the first Creature-brain neuron whose lobe-local
    // coordinate plus lobe offset equals the two parsed global coordinates.
    // The Creature/Brain adapter owns the scan and byte clamp; Macro owns
    // only parsing and target classification.
    virtual void fire_creature_neuron(objects::Object& object,
                                      std::int32_t global_x,
                                      std::int32_t global_y,
                                      std::int32_t activation) = 0;
    // `trig` replaces both native byte lanes for one Creature-brain neuron.
    // Brain/Lobe own the record layout; Macro owns only operand order and
    // target classification.
    virtual void set_creature_brain_neuron_activation(
        objects::Object& object, std::uint32_t lobe_index,
        std::uint32_t neuron_index, std::uint8_t activation) = 0;
    // `base` is the native Object virtual at vtable slot 40.  The concrete
    // SimpleObject/CompoundObject implementations own image storage and
    // redraw; this adapter preserves that dispatch without putting renderer
    // ownership or a fake base implementation in Macro.
    virtual void set_object_image_index(
        objects::Object& object, std::uint8_t image_index,
        std::int32_t part_index) = 0;
    // `bhvr` is a SimpleObject interaction-state operation.  The object
    // adapter owns the five native selector records and the byte-sized flag;
    // Macro owns only the two-rvalue parse and the family-2 target guard.
    virtual void configure_simple_object_behavior(
        objects::Object& object, std::uint32_t behavior_index,
        std::uint32_t interaction_flags) = 0;
    // `pose` is the native Object virtual at vtable slot 37.  The boolean
    // result is part of the interpreter contract: false rewinds the consumed
    // five-byte command record so the scheduler retries it next tick.
    virtual bool set_object_relative_image_index(
        objects::Object& object, std::uint32_t relative_index,
        std::int32_t part_index) = 0;
    // `prld` is the native Object virtual at vtable slot 39.  The returned
    // pointer is the interpreter's next script cursor; the application
    // adapter owns the image-cache/display services used by concrete objects.
    virtual char* preload_object_image_sequence(
        objects::Object& object, char* sequence_text,
        const char* sequence_end, std::int32_t part_index) = 0;
    // `tele` is a Creature-registry operation, not a direct Object movement
    // command.  The application adapter owns the registry scan, the native
    // temporary bounds transition, and the selected-Creature viewport/title
    // refresh.  Macro owns only the two-rvalue parse and command routing.
    // The target is passed by pointer because the native command does not
    // null-check it: with no target, the registry scan matches every Creature
    // whose bounds reference is also null.
    virtual void teleport_target_and_refresh_selection(
        objects::Object* target, std::int32_t world_x,
        std::int32_t world_y) = 0;
    // `dpas` admits only the native family-3 target (the clean runtime maps
    // this classifier family to Vehicle), scans the Creature registry for
    // creatures bound to that target, and queues ObjectEventId::event_5.
    // The application adapter owns registry iteration, bounds-reference
    // matching, and immediate-ring capacity/wrap; Macro owns only routing.
    virtual void drive_presentation_for_bound_creatures(
        objects::Object& target) = 0;
    // `mate` admits only a Creature target and invokes the Creature-owned
    // insemination policy.  The runtime supplies the object-to-Creature and
    // world/genome host boundary; Macro owns only command routing.
    virtual void process_creature_insemination(
        objects::Object& object) = 0;
    // `cdie` delegates the already recovered Creature::die policy to the
    // application adapter, which supplies CreatureDeathHost services.
    virtual void die_creature(objects::Object& object) = 0;
    // `kill` invokes Object's runtime-state reset virtual after parsing an
    // explicit object rvalue. Object/runtime owns deregistration, bounds,
    // selection, and re-registration; Macro owns only the operand and the
    // self-owner termination decision.
    virtual void initialize_object_runtime_state(objects::Object& object) = 0;
    // `aslp` passes its parsed CAOS value to Creature::set_sleep_indicator;
    // the adapter converts the Object target and supplies CreatureAttentionHost.
    virtual void set_creature_sleep_indicator(
        objects::Object& object, std::uint32_t enabled) = 0;
    // `drea` stores its one parsed CAOS value in the Creature instinct runtime
    // state's dream countdown.  Creature owns the countdown and dream-step
    // policy; Macro owns only operand order and target classification.
    virtual void set_creature_dream_countdown(
        objects::Object& object, std::uint32_t countdown) = 0;
    // `drop` delegates Creature::notify_dependents_on_removal; the adapter
    // supplies the registry, immediate-event, identity, stimulus, and debug
    // hosts required by that typed policy.
    virtual void notify_creature_dependents_on_removal(
        objects::Object& object) = 0;
    // `rmev` parses an Object reference and removes that object from the
    // process-wide EventBar display list with the native auxiliary-state
    // recording flag set.  The UI/event-bar adapter owns list storage and
    // funeral-state policy; Macro owns only operand parsing and routing.
    virtual void remove_object_from_event_bar(objects::Object& object,
                                              bool record_auxiliary_state) = 0;
    // `evnt` parses one Object reference and appends the resulting pointer to
    // the process-wide EventBar display list.  The native add routine accepts
    // the raw pointer (including null) and owns duplicate suppression,
    // capacity eviction, and pane refresh; Macro owns only parsing and
    // routing.
    virtual void add_object_to_event_bar(objects::Object* object) = 0;
    // `done` delegates the Creature motor-state transition: cancel the
    // active involuntary action, clear the selected decision neuron, and
    // reset the action-activation boost.
    virtual void stop_creature_involuntary_action(
        objects::Object& object) = 0;
    // `impt` parses one value, admits only a Creature target, and stores the
    // low byte as that Creature's action-activation boost.  The byte-sized
    // state remains Creature-owned; Macro supplies only operand order and
    // target classification.
    virtual void set_creature_action_activation_boost(
        objects::Object& object, std::uint32_t value) = 0;
    // `ltcy` writes the randomized cooldown byte for one of the eight
    // Creature involuntary-action records. Creature owns the two-byte record
    // layout; Macro owns operand order, native range arithmetic, and target
    // admission.
    virtual void set_creature_involuntary_action_cooldown(
        objects::Object& object, std::uint32_t action_index,
        std::uint8_t cooldown_ticks) = 0;
    // `snez` invokes the embedded CBacterium::Update policy. The application
    // adapter supplies the Creature/Bacterium world host; Macro owns only the
    // native target-family guard and command routing.
    virtual void update_creature_bacterium(objects::Object& object) = 0;
    // `vocb` restores the shipped default learned-word records. The command
    // owns only the Creature-family admission; vocabulary tables and record
    // storage remain Creature-owned.
    virtual void initialize_creature_default_vocabulary(
        objects::Object& object) = 0;
    // `spot` writes one of CompoundObject's six signed WorldRect records.
    // The CompoundObject adapter owns the +0xcc layout and slot boundary;
    // Macro owns only the five-rvalue order and target-family admission.
    virtual void set_compound_part_bounds(
        objects::Object& object, std::uint32_t part_bounds_index,
        std::int32_t min_x, std::int32_t min_y, std::int32_t max_x,
        std::int32_t max_y) = 0;
    // `knob` assigns one of CompoundObject's six creature-event mappings.
    // The CompoundObject adapter owns the mapping storage and any signed
    // representation details; Macro owns only operand order and the
    // family-3/index admission checks.
    virtual void set_compound_knob_function(
        objects::Object& object, std::uint32_t function_index,
        std::uint32_t hotspot_index) = 0;
    // `cabn` stores four signed words in Vehicle::creature_event_bounds_local
    // (+0x154).  Vehicle owns that rectangle; Macro owns only operand order
    // and the family-3 admission check.
    virtual void set_vehicle_creature_event_bounds(
        objects::Object& object, std::int32_t min_x, std::int32_t min_y,
        std::int32_t max_x, std::int32_t max_y) = 0;
    // `gpas` delegates the Vehicle-local event production policy. The
    // adapter resolves the Object to Vehicle and supplies VehicleEventHost.
    virtual void grab_vehicle_passengers(objects::Object& object) = 0;
    virtual void set_object_bounds_mode(objects::Object& object,
                                        std::uint32_t mode) = 0;
    virtual void update_object_movement_bounds(objects::Object& object) = 0;
    virtual void note_egg_state_change() = 0;
};

// `mvto` and `mvby` are Macro grammar operations whose rendering and object
// geometry are owned by the Object hierarchy.  Keeping this boundary
// separate prevents the interpreter from acquiring a counterfeit global
// movement implementation or a platform renderer dependency.
class MacroObjectMotionHost : public virtual MacroCommandHost {
public:
    ~MacroObjectMotionHost() override = default;
    virtual void move_to_and_redraw(objects::Object& object, int world_x,
                                    int world_y) = 0;
    virtual void move_by_and_redraw(objects::Object& object, int delta_x,
                                    int delta_y) = 0;
    // `aim:` targets a creature's Skeleton motion target part.  The adapter
    // performs the Object-to-Skeleton ownership check; Macro owns only the
    // token and rvalue order.
    virtual void set_motion_target_part_index(
        objects::Object& object, std::uint32_t part_index) = 0;
};

// `app:` is an application-owned prefix.  Its recovered `quit` subcommand
// releases an embedded kit slot; Macro must not acquire OLE, process-handle,
// or toolbar dependencies merely to parse the command.
class MacroApplicationHost : public virtual MacroCommandHost {
public:
    ~MacroApplicationHost() override = default;
    virtual void shutdown_embedded_kit_tool(std::uint32_t tool_index) = 0;
    // `tool` owns only CAOS parsing.  Registry serialization, the fixed
    // embedded-kit table, menu rebuild, and availability publication remain
    // application/platform work behind this typed seam.
    virtual void register_embedded_kit_tool(
        Macro& macro, std::string_view registry_name,
        std::string_view display_name, std::string_view launch_command,
        std::uint8_t raw_type_code) = 0;
};

// `stm#` is a Creature event-queue command. Macro owns only the nested-token
// grammar and rvalue order; the Creature adapter owns built-in stimulus
// storage, queue capacity, and the queued-record copy operation.
class MacroStimulusHost : public virtual MacroCommandHost {
public:
    ~MacroStimulusHost() override = default;

    // The native `stm# sign` and `stm# tact` branches delegate the
    // perception/overlap walk to separate Creature-owned helpers.  Keep the
    // signed parser result at this boundary: the native helpers perform the
    // built-in-index policy, registry walk, and queue submission.
    virtual void queue_sign_stimulus(objects::Object& source,
                                     std::int32_t stimulus_index) = 0;
    virtual void queue_tact_stimulus(objects::Object& source,
                                     std::int32_t stimulus_index) = 0;
    virtual void queue_speech_range_stimulus(
        objects::Object& source, std::int32_t stimulus_index) = 0;
    virtual void queue_built_in_stimulus(objects::Object& source,
                                         objects::Object& target,
                                         std::int32_t stimulus_index) = 0;
    // The top-level `stim` command carries a complete descriptor and four
    // chemical pairs. Macro owns the nested grammar and operand order;
    // Creature owns the registry walk, spatial predicate, and queue ring.
    virtual void queue_stimulus_for_perceiving(
        objects::Object& source,
        creatures1::creatures::StimulusContext& stimulus) = 0;
    virtual void queue_stimulus_for_overlapping(
        objects::Object& source,
        creatures1::creatures::StimulusContext& stimulus) = 0;
    virtual void queue_stimulus_in_speech_range(
        objects::Object& source,
        creatures1::creatures::StimulusContext& stimulus) = 0;
    virtual void queue_direct_stimulus(
        objects::Object& source, objects::Object& target,
        creatures1::creatures::StimulusContext& stimulus) = 0;
    virtual void report_invalid_stimulus_index(
        std::int32_t stimulus_index) const = 0;
};

// Macro-owned commands that submit an immediate ObjectEvent record share this
// narrow queue boundary.  The scheduler owns ring storage, capacity, and
// ordering; Macro owns token grammar and operand order.
class MacroObjectEventHost : public virtual MacroCommandHost {
public:
    ~MacroObjectEventHost() override = default;

    virtual void queue_immediate_object_event(
        objects::Object& source, objects::Object& target,
        objects::ObjectEventId event_id) = 0;
};

// `mesg` is the object-event counterpart to `stm#`.  Its additional fan-out
// forms remain here, while its direct record uses the shared object-event
// boundary above.  `spas` uses the same boundary with two explicit rvalues.
class MacroMessageHost : public MacroObjectEventHost {
public:
    ~MacroMessageHost() override = default;

    virtual void queue_perception_message(objects::Object& source,
                                          objects::ObjectEventId event_id) = 0;
    virtual void queue_tactile_message(objects::Object& source,
                                       objects::ObjectEventId event_id) = 0;
    virtual void queue_speech_range_message(
        objects::Object& source, objects::ObjectEventId event_id) = 0;
};

// `dbgm` and `dbgv` are Macro-owned debug commands.  `dbgm` consumes the
// bracketed text form; `dbgv` consumes one rvalue.  Formatting, console
// presence, and logging policy remain in the application/common boundary.
class MacroDebugHost : public virtual MacroCommandHost {
public:
    ~MacroDebugHost() override = default;
    virtual void log_debug_message(Macro& macro, std::string_view text) = 0;
    virtual void log_debug_value(Macro& macro, std::uint32_t value) = 0;
    // `dbug` uses the native "Debug: %d\n" format and then enables the
    // Macro output-capture lifetime flag.  Keep it distinct from `dbgv`,
    // whose native branch logs "Debug Value: %d\n" without that write.
    virtual void log_debug_command_value(Macro& macro,
                                          std::uint32_t value) = 0;
};

// Sound commands consume literal sound descriptors from the bytecode. The
// object owns audibility and continuous-sound state, while the application
// host owns the SoundManager and mixer readiness checks.
class MacroSoundHost : public virtual MacroCommandHost {
public:
    ~MacroSoundHost() override = default;
    virtual void play_sound_effect(objects::Object& object,
                                   sound::SoundId sound_id,
                                   int queue_delay_ticks) = 0;
    virtual void set_continuous_sound(objects::Object& object,
                                      sound::SoundId sound_id,
                                      bool persist_when_out_of_range) = 0;
    virtual void fade_continuous_sound(objects::Object& object) = 0;
    virtual void stop_continuous_sound(objects::Object& object) = 0;
    virtual void load_sound_cache_if_audible(objects::Object& object,
                                             sound::SoundId sound_id) = 0;
};

// `sndf` has no object operand. It changes the SFC view's global sound
// policy and conditionally coordinates the process-wide mixer. Keeping these
// operations atomic at the host boundary preserves that ownership split:
// Macro parses the CAOS subcommand, while the view/application adapter owns
// focus state, mixer readiness, logging, and SoundManager flags.
class MacroSoundPolicyHost : public virtual MacroCommandHost {
public:
    ~MacroSoundPolicyHost() override = default;
    virtual void set_sound_foreground_policy() = 0;
    virtual void enable_sound_and_restore_if_ready() = 0;
    virtual void disable_sound_and_suspend_if_ready() = 0;
    virtual void set_sound_conservative_policy() = 0;
};

// Speech commands cross from the Macro bytecode into Creature's learned-word
// and Voice policy. The host keeps Creature's Voice/Object/event wiring out
// of the interpreter while exposing operations at the recovered semantic
// boundary.
class MacroSpeechHost : public virtual MacroCommandHost {
public:
    ~MacroSpeechHost() override = default;
    virtual void speak_learned_word(objects::Object& creature,
                                    std::uint32_t learned_word_index) = 0;
    virtual void speak_text(objects::Object& creature,
                            std::string_view text) = 0;
    virtual void speak_dominant_drive_phrase(objects::Object& creature) = 0;
};

class MacroExceptionHost {
public:
    virtual ~MacroExceptionHost() = default;
    // This host operation includes the native report sequence: optional
    // DebugLog, modal AfxMessageBox, timer 1 shutdown before the dialog, and
    // [1,300] interval normalization/re-arm after it.  Those are application
    // and Windows/MFC boundaries, not Macro-owned language semantics.
    virtual void report_execution_exception(
        Macro& macro, const MacroExecutionExceptionDiagnostic& diagnostic) = 0;
};

class MacroInterpreterHost {
public:
    virtual ~MacroInterpreterHost() = default;
    // The scheduler supplies the concrete owners for each recovered command
    // family.  ExecuteInterpreter remains Macro-owned; the host no longer
    // hides the whole interpreter behind an opaque callback.
    virtual MacroInterpreterBindings interpreter_bindings() = 0;
    virtual void report_too_many_macros(Macro& macro,
                                        std::size_t maximum_macros) = 0;
};

class MacroExecutionHost {
public:
    virtual ~MacroExecutionHost() = default;

    virtual objects::Object* selected_creature() const = 0;
    virtual objects::Object* initial_auxiliary_object() const = 0;

    // The host validates the C1 creature classifier and resolves its IT
    // object.  The classifier/layout check is not a property of base Object.
    virtual objects::Object* resolve_it_object(
        objects::Object* script_owner) const = 0;
};

class MacroArchive {
public:
    virtual ~MacroArchive() = default;
    virtual bool loading() const = 0;
    virtual std::uint32_t read_uint32() = 0;
    virtual std::int32_t read_int32() = 0;
    virtual std::string read_string() = 0;
    virtual void write_uint32(std::uint32_t value) = 0;
    virtual void write_int32(std::int32_t value) = 0;
    virtual void write_string(std::string_view value) = 0;
    virtual objects::Object* read_object() = 0;
    virtual void write_object(objects::Object* object) = 0;
};

enum class MacroObjectSlot : std::size_t {
    script_owner = 0,
    from_object = 1,
    exec_object = 2,
    target_object = 3,
    it_object = 4,
};

struct MacroObjectContext {
    objects::Object* script_owner = nullptr;
    objects::Object* from_object = nullptr;
    objects::Object* exec_object = nullptr;
    objects::Object* target_object = nullptr;
    objects::Object* it_object = nullptr;
};

enum class DdeScoreCounter {
    living_norns,
    dead_norns,
    natural_eggs_laid,
    hatchery_eggs_used,
};

enum class DdeGetBQuery {
    genome_source,
    creature_history,
    selected_creature_status,
    creature_name,
    creature_age,
};

enum class DdePutBCommand {
    creature_name,
    creature_history,
};

class MacroDdeHost : public virtual MacroCommandHost {
public:
    ~MacroDdeHost() override = default;

    virtual void adjust_score(DdeScoreCounter counter, std::int32_t delta) = 0;
    virtual void notify_score_changed() = 0;
    virtual void pan_view_to_selected_creature() = 0;

    virtual std::optional<std::string> query_getb(Macro& macro,
                                                  DdeGetBQuery query) = 0;
    virtual bool update_putb(Macro& macro, DdePutBCommand command,
                             std::string_view text) = 0;
    virtual std::string render_learned_words(Macro& macro,
                                             std::uint32_t word_index) = 0;
    virtual std::string render_brain_lobe(Macro& macro) = 0;
    virtual std::string render_gene_counts(Macro& macro) = 0;
    virtual std::string render_cell_values(Macro& macro,
                                            std::uint32_t cell,
                                            std::uint32_t variable,
                                            std::uint32_t field) = 0;
    virtual bool capture_picture(Macro& macro, std::uint8_t width,
                                 std::uint8_t height,
                                 std::string& output_path) = 0;
};

class MacroSystemHost : public virtual MacroCommandHost {
public:
    ~MacroSystemHost() override = default;

    virtual void move_main_window(int left, int top, int right,
                                  int bottom) = 0;
    virtual void send_main_frame_command(std::uint32_t command) = 0;
    virtual void enable_viewport_navigation() = 0;
    virtual void set_viewport_origin(int x, int y) = 0;
    virtual void open_world_file(std::string_view path) = 0;
    virtual void set_ground_height(std::uint32_t x_block,
                                   std::uint32_t height) = 0;
    // The native `room` command updates one MapData room record and grows the
    // active room count.  MapData owns record storage and count maintenance;
    // the interpreter exposes only this source-level application operation.
    virtual void set_room_definition(std::uint32_t room_index, int left,
                                     int top, int right, int bottom,
                                     std::uint32_t room_type) = 0;
    virtual void bring_main_window_to_front() = 0;
    // `vrsn` is a language-compatibility gate.  The application adapter owns
    // the native diagnostic text, optional DebugLog, timer-1 shutdown,
    // modal MFC warning, and world-timer disable; Macro owns only the
    // operand/version comparison and the resulting termination state.
    virtual void report_unsupported_language_version(
        Macro& macro, std::uint32_t supported_version,
        std::uint32_t requested_version) = 0;
    virtual void follow_macro_target(Macro& macro) = 0;
    virtual void set_dirty_world_rect(int left, int top, int right,
                                      int bottom) = 0;
    // `scrx` removes one exact four-byte classifier record from the global
    // script-definition table. The scripting registry owns entry storage and
    // compaction; the application adapter supplies that registry operation.
    virtual void remove_script_definition(ScriptClassifier classifier) = 0;
};

class MacroBlackboardHost : public virtual MacroCommandHost {
public:
    ~MacroBlackboardHost() override = default;

    virtual bool has_blackboard_target(const Macro& macro) const = 0;
    virtual std::uint32_t current_word_index(const Macro& macro) const = 0;
    virtual std::string current_word_text(const Macro& macro) const = 0;
    virtual void announce_blackboard_word(Macro& macro, bool spoken,
                                          std::uint32_t word_index,
                                          std::string_view word_text) = 0;
    virtual void write_blackboard_word(Macro& macro, std::uint32_t word_index,
                                       std::uint32_t value,
                                       std::string_view word_text) = 0;
    virtual void set_blackboard_edit_mode(Macro& macro,
                                           std::uint32_t enabled) = 0;
    virtual void redraw_blackboard(Macro& macro, std::uint32_t mode) = 0;
};

struct NewCallButtonRequest {
    std::uint32_t object_file_id = 0;
    std::uint32_t header_record_index = 0;
    std::uint32_t image_count = 0;
    std::uint32_t render_plane = 0;
};

struct NewSceneryRequest {
    CaosToken construction_type = 0;
    std::uint32_t image_count = 0;
    std::uint32_t image_index = 0;
    std::uint32_t render_plane = 0;
};

struct NewVehicleRequest {
    CaosToken sprite_file_id = 0;
    std::uint32_t header_record_index = 0;
    std::uint32_t image_count = 0;
};

struct NewCreatureRequest {
    // Native ExecuteNewCommand's real 'crea' case (confirmed via
    // disassembly) reads this with a plain ParseRValue call -- the same
    // integer-rvalue path as every other `new:` argument -- not a
    // bracketed text literal. `TOKN <name>` (a literal 4-character id) and
    // a variable holding a previously-generated genome id (as `new: gene
    // ... var0` then `new: crea var0 <sex>` -- the standard population-
    // restocking pattern) both work because both are ordinary rvalues.
    std::uint32_t genome_source_filename = 0;
    creatures1::creatures::CreatureConstructionSex construction_sex;
};

struct NewBlackboardRequest {
    CaosToken object_type = 0;
    std::uint32_t header_record_index = 0;
    std::uint32_t image_count = 0;
    std::uint32_t fill_palette_index = 0;
    std::uint32_t text_render_config_1 = 0;
    std::uint32_t text_render_config_2 = 0;
    std::uint32_t tile_x = 0;
    std::uint32_t tile_y = 0;
};

struct NewPartRequest {
    std::uint32_t part_index = 0;
    std::uint32_t local_x_offset = 0;
    std::uint32_t local_y_offset = 0;
    std::uint32_t image_index = 0;
    std::uint32_t render_plane = 0;
};

struct NewSimpleObjectRequest {
    std::uint32_t object_file_id = 0;
    std::uint32_t header_record_index = 0;
    std::uint32_t image_count = 0;
    bool cache_protected = false;
    std::uint32_t render_plane = 0;
};

struct NewCompoundObjectRequest {
    CaosToken sprite_file_id = 0;
    std::uint32_t header_record_index = 0;
    std::uint32_t image_count = 0;
    bool cache_protected = false;
};

struct NewLiftRequest {
    std::int32_t object_file_id = 0;
    std::uint32_t header_record_index = 0;
    std::uint32_t image_count = 0;
};

// `new:` is a game-owned object factory command.  The native implementation
// directly performs operator-new, gallery acquisition, Entity registration,
// MFC-array insertion, and several derived-object initializers.  This host
// keeps those ownership boundaries explicit while Macro retains the command
// grammar, parser order, target-slot policy, and selected-part state.
class MacroNewObjectHost : public virtual MacroCommandHost {
public:
    ~MacroNewObjectHost() override = default;

    virtual objects::Object* create_call_button(
        Macro& macro, const NewCallButtonRequest& request) = 0;
    virtual std::uint32_t generate_offspring_genome_file(
        Macro& macro, std::uint32_t first_parent,
        std::uint32_t second_parent) = 0;
    virtual objects::Object* create_scenery(
        Macro& macro, const NewSceneryRequest& request) = 0;
    virtual objects::Object* create_vehicle(
        Macro& macro, const NewVehicleRequest& request) = 0;
    virtual objects::Object* create_creature(
        Macro& macro, const NewCreatureRequest& request) = 0;
    virtual objects::Object* create_blackboard(
        Macro& macro, const NewBlackboardRequest& request) = 0;
    virtual void create_part(Macro& macro, const NewPartRequest& request) = 0;
    virtual objects::Object* create_simple_object(
        Macro& macro, const NewSimpleObjectRequest& request) = 0;
    virtual objects::Object* create_compound_object(
        Macro& macro, const NewCompoundObjectRequest& request) = 0;
    virtual objects::Object* create_lift(Macro& macro,
                                         const NewLiftRequest& request) = 0;
};

class MacroSchedulerHost : public MacroExecutionHost,
                           public MacroInterpreterHost {
public:
    ~MacroSchedulerHost() override = default;
};

// Composition adapter used at the scheduler boundary.  The application may
// keep execution-state lookup and interpreter-family bindings in separate
// owners; this adapter presents the single MacroSchedulerHost contract that
// Macro::start_execution and the output path consume.  It is non-owning and
// deliberately contains no platform or scheduler policy.
class MacroSchedulerHostAdapter final : public MacroSchedulerHost {
public:
    MacroSchedulerHostAdapter(MacroExecutionHost& execution_host,
                              MacroInterpreterHost& interpreter_host);

    objects::Object* selected_creature() const override;
    objects::Object* initial_auxiliary_object() const override;
    objects::Object* resolve_it_object(
        objects::Object* script_owner) const override;
    MacroInterpreterBindings interpreter_bindings() override;
    void report_too_many_macros(Macro& macro,
                                std::size_t maximum_macros) override;

private:
    MacroExecutionHost& execution_host_;
    MacroInterpreterHost& interpreter_host_;
};

// Typed owner bindings for the outer interpreter.  These are non-owning
// pointers: the scheduler/application owns the adapters and guarantees their
// lifetime for one ExecuteInterpreter call.  Keeping the pointer list here
// makes missing family support visible at the dispatch boundary and prevents
// a new decompiler-shaped "host callback" from becoming the next hiding spot.
struct MacroInterpreterBindings {
    MacroRuntimeHost* runtime = nullptr;
    MacroCommandHost* diagnostics = nullptr;
    MacroExceptionHost* exceptions = nullptr;
    MacroDdeHost* dde = nullptr;
    MacroSystemHost* system = nullptr;
    MacroBlackboardHost* blackboard = nullptr;
    MacroNewObjectHost* new_object = nullptr;
    MacroApplicationHost* application = nullptr;
    MacroObjectMotionHost* object_motion = nullptr;
    MacroStimulusHost* stimulus = nullptr;
    MacroMessageHost* message = nullptr;
    MacroObjectEventHost* object_events = nullptr;
    MacroDebugHost* debug = nullptr;
    MacroSoundHost* sound = nullptr;
    MacroSoundPolicyHost* sound_policy = nullptr;
    MacroSpeechHost* speech = nullptr;
    ScriptDefinitionInstallHost* scripts = nullptr;
    // Optional observer. The owner must outlive one ExecuteInterpreter call;
    // null means production execution with no instrumentation overhead beyond
    // the branch at the recovered outer-loop boundaries.
    MacroInterpreterTrace* trace = nullptr;
};

class Macro {
public:
    virtual ~Macro() = default;

    void serialize(MacroArchive& archive);
    void reset_execution_state(const MacroExecutionHost& host);
    void remove_from_running_scheduler_and_release();
    void remove_from_running_scheduler_and_destroy();
    void load_script_text(std::string_view script_text);
    CaosToken read_next_token();
    CaosToken peek_next_token();
    std::uint32_t parse_rvalue(MacroRuntimeHost& runtime,
                               MacroCommandHost& diagnostics);
    void assign_lvalue(MacroRuntimeHost& runtime,
                       MacroCommandHost& diagnostics,
                       CaosToken destination_token, std::uint32_t value);
    void read_bracketed_text_argument(char* output_text,
                                       int output_capacity);
    std::string read_bracketed_text_argument();
    void consume_execute_command_arguments();
    MacroControlFlowResult execute_global_subroutine_command();
    void report_syntax_error(MacroCommandHost& host,
                             std::string_view expected_token_type);
    void handle_script_execution_exception(MacroExceptionHost& host,
                                           MacroRuntimeHost& runtime);
    void execute_dde_command(MacroDdeHost& host, MacroRuntimeHost& runtime);
    void execute_blackboard_caos_command(MacroBlackboardHost& host,
                                         MacroRuntimeHost& runtime);
    void execute_new_command(MacroNewObjectHost& host,
                             MacroRuntimeHost& runtime);
    void execute_system_caos_command(MacroSystemHost& host,
                                     MacroRuntimeHost& runtime);
    void execute_application_prefix_command(
        MacroApplicationHost& host, MacroRuntimeHost& runtime);
    bool execute_tool_command(MacroCommand command,
                              MacroApplicationHost& host,
                              MacroRuntimeHost& runtime);
    void execute_aim_prefix_command(MacroObjectMotionHost& host,
                                    MacroRuntimeHost& runtime);
    void execute_stimulus_prefix_command(MacroStimulusHost& host,
                                          MacroRuntimeHost& runtime);
    void execute_stimulus_command(MacroStimulusHost& host,
                                  MacroRuntimeHost& runtime);
    void execute_message_command(MacroMessageHost& host,
                                 MacroRuntimeHost& runtime);
    bool execute_chemical_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics);
    bool execute_fire_command(MacroCommand command, MacroRuntimeHost& runtime,
                              MacroCommandHost& diagnostics);
    bool execute_trigger_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroCommandHost& diagnostics);
    bool execute_delete_creature_command(MacroCommand command,
                                          MacroRuntimeHost& runtime);
    bool execute_sleep_command(MacroCommand command,
                               MacroRuntimeHost& runtime,
                               MacroCommandHost& diagnostics);
    bool execute_dream_command(MacroCommand command,
                                MacroRuntimeHost& runtime,
                                MacroCommandHost& diagnostics);
    bool execute_drop_command(MacroCommand command, MacroRuntimeHost& runtime);
    bool execute_done_command(MacroCommand command, MacroRuntimeHost& runtime);
    MacroControlFlowResult execute_touch_command(MacroCommand command,
                               MacroRuntimeHost& runtime);
    bool execute_creature_motor_command(MacroCommand command,
                                        MacroRuntimeHost& runtime,
                                        MacroCommandHost& diagnostics);
    bool execute_creature_runtime_command(
        MacroCommand command, MacroRuntimeHost& runtime,
        MacroCommandHost& diagnostics);
    bool execute_compound_geometry_command(
        MacroCommand command, MacroRuntimeHost& runtime,
        MacroCommandHost& diagnostics);
    bool execute_compound_knob_command(MacroCommand command,
                                       MacroRuntimeHost& runtime,
                                       MacroCommandHost& diagnostics);
    bool execute_vehicle_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroObjectEventHost& object_events);
    bool execute_part_command(MacroCommand command,
                              MacroRuntimeHost& runtime,
                              MacroCommandHost& diagnostics);
    bool execute_instantiate_command(MacroCommand command);
    MacroControlFlowResult execute_over_command(MacroCommand command);
    bool execute_object_bounds_command(MacroCommand command,
                                        MacroRuntimeHost& runtime);
    bool execute_base_command(MacroCommand command,
                              MacroRuntimeHost& runtime,
                              MacroCommandHost& diagnostics);
    bool execute_behavior_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics);
    MacroControlFlowResult execute_pose_command(MacroCommand command,
                              MacroRuntimeHost& runtime,
                              MacroCommandHost& diagnostics);
    bool execute_preload_image_sequence_command(MacroCommand command,
                                                MacroRuntimeHost& runtime);
    bool execute_teleport_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics);
    bool execute_drive_presentation_command(MacroCommand command,
                                            MacroRuntimeHost& runtime);
    bool execute_edit_command(MacroCommand command,
                              MacroRuntimeHost& runtime,
                              MacroSystemHost& system);
    bool execute_version_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroSystemHost& system);
    bool execute_room_command(MacroCommand command,
                              MacroRuntimeHost& runtime,
                              MacroSystemHost& system);
    bool execute_mate_command(MacroCommand command,
                              MacroRuntimeHost& runtime);
    bool execute_timer_command(MacroCommand command,
                               MacroRuntimeHost& runtime,
                               MacroCommandHost& diagnostics);
    bool execute_animation_command(MacroCommand command);
    bool execute_target_command(MacroCommand command,
                                MacroRuntimeHost& runtime,
                                MacroCommandHost& diagnostics);
    MacroControlFlowResult execute_pointer_command(MacroCommand command,
                                 MacroRuntimeHost& runtime);
    // Converts a CAOS value to an object, refusing a pointer the world does
    // not own.  Every command that takes an object rvalue goes through this.
    objects::Object* object_from_value(std::uint32_t value,
                                       const MacroRuntimeHost& runtime) const;
    void validate_object_context(MacroRuntimeHost& runtime);

    MacroControlFlowResult execute_approach_command(MacroCommand command,
                                   MacroRuntimeHost& runtime);
    bool execute_vehicle_bounds_command(MacroCommand command,
                                        MacroRuntimeHost& runtime,
                                        MacroCommandHost& diagnostics);
    bool execute_debug_command(MacroCommand command, MacroDebugHost& host,
                               MacroRuntimeHost& runtime);
    bool execute_remove_event_command(MacroCommand command,
                                      MacroRuntimeHost& runtime,
                                      MacroCommandHost& diagnostics);
    bool execute_event_command(MacroCommand command,
                               MacroRuntimeHost& runtime,
                               MacroCommandHost& diagnostics);
    MacroControlFlowResult execute_script_command(
        MacroCommand command, ScriptDefinitionInstallHost& scripts,
        MacroRuntimeHost& runtime, MacroCommandHost& diagnostics);
    MacroControlFlowResult execute_script_extended_command(
        MacroCommand command, MacroSystemHost& system,
        MacroRuntimeHost& runtime, MacroCommandHost& diagnostics);
    bool execute_arithmetic_command(MacroCommand command,
                                    MacroRuntimeHost& runtime,
                                    MacroCommandHost& diagnostics);
    bool execute_randomize_variable_command(
        MacroCommand command, MacroRuntimeHost& runtime,
        MacroCommandHost& diagnostics);
    bool execute_object_motion_command(MacroCommand command,
                                       MacroObjectMotionHost& host,
                                       MacroRuntimeHost& runtime);
    bool execute_sound_command(MacroCommand command, MacroSoundHost& host,
                               MacroRuntimeHost& runtime);
    bool execute_sound_policy_command(MacroCommand command,
                                      MacroSoundPolicyHost& host);
    bool execute_speech_command(MacroCommand command, MacroSpeechHost& host,
                                MacroRuntimeHost& runtime);
    static MacroCommandFamily classify_command(CaosToken token);
    static bool evaluate_comparison(MacroComparisonOperator operation,
                                    std::uint32_t left,
                                    std::uint32_t right);
    std::optional<bool> parse_comparison_condition(
        MacroRuntimeHost& runtime, MacroCommandHost& diagnostics);
    MacroControlFlowResult begin_loop();
    MacroControlFlowResult begin_counted_repeat(std::uint32_t count);
    MacroControlFlowResult execute_counted_repeat_command(
        MacroRuntimeHost& runtime, MacroCommandHost& diagnostics);
    MacroControlFlowResult execute_kill_command(
        MacroRuntimeHost& runtime, MacroCommandHost& diagnostics);
    MacroControlFlowResult execute_wait_command(
        MacroRuntimeHost& runtime, MacroCommandHost& diagnostics);
    MacroInterpreterFinalization finalize_interpreter_iteration(
        bool iteration_completed);
    MacroControlFlowResult execute_control_flow_command(
        MacroCommand command, std::optional<bool> condition = std::nullopt);
    MacroControlFlowResult dispatch_interpreter_command(
        MacroCommand command, const MacroInterpreterBindings& bindings);
    MacroControlFlowResult execute_object_enumeration_command(
        MacroCommand command, MacroRuntimeHost& runtime,
        MacroCommandHost& diagnostics);
    bool execute_random_target_command(MacroCommand command,
                                       MacroRuntimeHost& runtime,
                                       MacroCommandHost& diagnostics);
    std::size_t execute_to_output_buffer(const MacroExecutionHost& execution_host,
                                         MacroInterpreterHost& interpreter_host,
                                         std::string& output_buffer);
    // The adapter overload makes the scheduler ownership boundary explicit;
    // output_buffer remains caller-owned and is populated before interpreter
    // capture state is released.
    std::size_t execute_to_output_buffer(MacroSchedulerHost& host,
                                         std::string& output_buffer);
    MacroControlFlowResult execute_interpreter(
        const MacroInterpreterBindings& bindings);
    bool start_execution(MacroSchedulerHost& host);
    void censor_script_profanity();

    void clear_output() {
        output_text.clear();
    }
    void append_output(std::string_view text) {
        output_text.append(text);
    }
    const std::string& output() const {
        return output_text;
    }

    bool destroy_when_finished = false;
    bool capture_output_enabled = false;
    std::uint32_t script_capacity_bytes = 0;
    std::string script_buffer;
    std::size_t script_cursor_offset = 0;
    std::array<std::uint32_t, 20> caos_value_stack{};
    std::size_t caos_value_stack_cursor_index = 0;
    std::array<std::uint32_t, 10> caos_work_values{};
    MacroObjectContext object_context{};
    std::int32_t selected_part_index = 0;
    std::uint32_t subroutine_cache_id = 0;
    std::size_t subroutine_cache_cursor_offset = 0;
    std::uint32_t wait_ticks_remaining = 0;
    bool execution_terminated = false;
    std::string output_text;
    // Keep repeated decodes of one malformed operand to one log entry. The
    // outer interpreter resets this before each command dispatch.
    mutable bool object_reference_fault_reported = false;
};

extern std::vector<Macro*> g_running_macros;

// SFCDoc::DeleteContents removes and destroys every scheduler-owned macro
// before script definitions are reset. The scheduler list is the owner for
// this lifecycle operation; callers must not merely clear the pointer vector.
void clear_running_macros();

void purge_destroy_when_finished_macros_for_owner(
    objects::Object* script_owner);

// Permanent object deletion invalidates object references held by every
// running macro, but leaves the macro itself scheduled and its script owner
// intact.  The four context slots mirror the native cleanup policy.
void clear_object_references_from_running_macros(objects::Object* object);

} // namespace creatures1::scripting
