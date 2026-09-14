#include "macro.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "../creatures/stimulus.hpp"
#include "../objects/object.hpp"
#include "classifier_scripts.hpp"
#include "tables.hpp"

namespace creatures1::scripting {

std::vector<Macro*> g_running_macros;

void clear_running_macros() {
    while (!g_running_macros.empty()) {
        Macro* const macro = g_running_macros.back();
        g_running_macros.pop_back();
        delete macro;
    }
}

MacroSchedulerHostAdapter::MacroSchedulerHostAdapter(
    MacroExecutionHost& execution_host, MacroInterpreterHost& interpreter_host)
    : execution_host_(execution_host), interpreter_host_(interpreter_host) {}

objects::Object* MacroSchedulerHostAdapter::selected_creature() const {
    return execution_host_.selected_creature();
}

objects::Object* MacroSchedulerHostAdapter::initial_auxiliary_object() const {
    return execution_host_.initial_auxiliary_object();
}

objects::Object* MacroSchedulerHostAdapter::resolve_it_object(
    objects::Object* script_owner) const {
    return execution_host_.resolve_it_object(script_owner);
}

MacroInterpreterBindings MacroSchedulerHostAdapter::interpreter_bindings() {
    return interpreter_host_.interpreter_bindings();
}

void MacroSchedulerHostAdapter::report_too_many_macros(
    Macro& macro, std::size_t maximum_macros) {
    interpreter_host_.report_too_many_macros(macro, maximum_macros);
}

void purge_destroy_when_finished_macros_for_owner(
    objects::Object* script_owner) {
    for (std::size_t index = 0; index < g_running_macros.size();) {
        Macro* macro = g_running_macros[index];
        if (macro->object_context.script_owner != script_owner ||
            !macro->destroy_when_finished) {
            ++index;
            continue;
        }

        // Remove the slot before destruction. Macro destruction can observe
        // the registry, so the live list must already exclude this object.
        g_running_macros.erase(g_running_macros.begin() + index);
        delete macro;
    }
}

void clear_object_references_from_running_macros(objects::Object* object) {
    for (Macro* macro : g_running_macros) {
        if (macro->object_context.from_object == object) {
            macro->object_context.from_object = nullptr;
        }
        if (macro->object_context.exec_object == object) {
            macro->object_context.exec_object = nullptr;
        }
        if (macro->object_context.it_object == object) {
            macro->object_context.it_object = nullptr;
        }
        if (macro->object_context.target_object == object) {
            macro->object_context.target_object = nullptr;
        }
    }
}

void Macro::serialize(MacroArchive& archive) {
    if (archive.loading()) {
        destroy_when_finished = archive.read_uint32() != 0;
        capture_output_enabled = archive.read_uint32() != 0;
        script_capacity_bytes = archive.read_uint32();
        std::string loaded_script = archive.read_string();
        if (script_capacity_bytes > 0x7fffffffu) {
            script_capacity_bytes = 0;
        }
        script_buffer = std::move(loaded_script);
        script_buffer.resize(std::strlen(script_buffer.c_str()));
        script_cursor_offset = archive.read_uint32();
        script_cursor_offset = std::min(script_cursor_offset,
                                        script_buffer.size());
    for (std::uint32_t& value : caos_value_stack) {
            value = archive.read_uint32();
        }
        caos_value_stack_cursor_index = archive.read_uint32();
        caos_value_stack_cursor_index =
            std::min(caos_value_stack_cursor_index, caos_value_stack.size());
        for (std::uint32_t& value : caos_work_values) {
            value = archive.read_uint32();
        }
        object_context.script_owner = archive.read_object();
        object_context.from_object = archive.read_object();
        object_context.exec_object = archive.read_object();
        object_context.target_object = archive.read_object();
        object_context.it_object = archive.read_object();
        selected_part_index = archive.read_int32();
        subroutine_cache_id = archive.read_uint32();
        subroutine_cache_cursor_offset = archive.read_uint32();
        subroutine_cache_cursor_offset =
            std::min(subroutine_cache_cursor_offset, script_buffer.size());
        wait_ticks_remaining = archive.read_uint32();
        return;
    }

    archive.write_uint32(destroy_when_finished ? 1u : 0u);
    archive.write_uint32(capture_output_enabled ? 1u : 0u);
    archive.write_uint32(script_capacity_bytes);
    archive.write_string(script_buffer);
    archive.write_uint32(static_cast<std::uint32_t>(
        std::min(script_cursor_offset, script_buffer.size())));
    for (std::uint32_t value : caos_value_stack) {
        archive.write_uint32(value);
    }
    archive.write_uint32(static_cast<std::uint32_t>(
        std::min(caos_value_stack_cursor_index, caos_value_stack.size())));
    for (std::uint32_t value : caos_work_values) {
        archive.write_uint32(value);
    }
    archive.write_object(object_context.script_owner);
    archive.write_object(object_context.from_object);
    archive.write_object(object_context.exec_object);
    archive.write_object(object_context.target_object);
    archive.write_object(object_context.it_object);
    archive.write_int32(selected_part_index);
    archive.write_uint32(subroutine_cache_id);
    archive.write_uint32(static_cast<std::uint32_t>(
        std::min(subroutine_cache_cursor_offset, script_buffer.size())));
    archive.write_uint32(wait_ticks_remaining);
}

void Macro::reset_execution_state(const MacroExecutionHost& host) {
    objects::Object* script_owner_snapshot = object_context.script_owner;
    caos_value_stack_cursor_index = 0;
    execution_terminated = false;
    wait_ticks_remaining = 0;
    object_context.target_object = script_owner_snapshot;
    script_cursor_offset = 0;
    object_context.it_object = script_owner_snapshot == nullptr
                                   ? nullptr
                                   : host.resolve_it_object(script_owner_snapshot);
    subroutine_cache_cursor_offset = 0;
    selected_part_index = 0;
    subroutine_cache_id = 0;
    caos_work_values.fill(0);
    capture_output_enabled = false;
}

void Macro::remove_from_running_scheduler_and_release() {
    const auto it = std::find(g_running_macros.begin(), g_running_macros.end(),
                              this);
    if (it != g_running_macros.end()) {
        g_running_macros.erase(it);
    }
    if (!capture_output_enabled || destroy_when_finished) {
        destroy_when_finished = true;
        delete this;
    }
}

void Macro::remove_from_running_scheduler_and_destroy() {
    const auto it = std::find(g_running_macros.begin(), g_running_macros.end(),
                              this);
    if (it != g_running_macros.end()) {
        g_running_macros.erase(it);
    }
    if (destroy_when_finished) {
        delete this;
    }
}

void Macro::load_script_text(std::string_view script_text) {
    const std::size_t required_capacity = script_text.size() + 6;
    if (script_capacity_bytes < required_capacity) {
        script_capacity_bytes = static_cast<std::uint32_t>(required_capacity);
        script_buffer.reserve(script_capacity_bytes);
    }
    script_buffer.assign(script_text);
    if (script_buffer.empty()) {
        script_buffer = "endm";
    } else if (script_buffer.size() < 4 ||
               script_buffer.compare(script_buffer.size() - 4, 4, "endm") != 0) {
        script_buffer += ",endm";
    }
    script_cursor_offset = 0;
}

MacroCommandFamily Macro::classify_command(CaosToken token) {
    switch (static_cast<MacroCommand>(token)) {
    case MacroCommand::do_if:
    case MacroCommand::else_:
    case MacroCommand::end_if:
    case MacroCommand::loop:
    case MacroCommand::ever:
    case MacroCommand::until:
    case MacroCommand::repeat:
    case MacroCommand::repeat_counted:
    case MacroCommand::next:
    case MacroCommand::global_subroutine:
    case MacroCommand::subroutine:
    case MacroCommand::return_from_subroutine:
    case MacroCommand::stop:
        return MacroCommandFamily::control_flow;

    case MacroCommand::set_variable:
    case MacroCommand::add_variable:
    case MacroCommand::subtract_variable:
    case MacroCommand::multiply_variable:
    case MacroCommand::divide_variable:
    case MacroCommand::modulo_variable:
    case MacroCommand::bitwise_and_variable:
    case MacroCommand::bitwise_or_variable:
    case MacroCommand::negate_variable:
    case MacroCommand::randomize_variable:
        return MacroCommandFamily::arithmetic_and_lvalue;

    case MacroCommand::move_by:
    case MacroCommand::move_to:
    case MacroCommand::mcrt:
    case MacroCommand::tele:
        return MacroCommandFamily::object_motion;

    case MacroCommand::pose:
    case MacroCommand::preload_image_sequence:
        return MacroCommandFamily::object_appearance;

    case MacroCommand::instantiate:
    case MacroCommand::script:
    case MacroCommand::script_extended:
    case MacroCommand::endm:
    case MacroCommand::mate:
    case MacroCommand::delete_creature:
    case MacroCommand::dream:
    case MacroCommand::sleep:
    case MacroCommand::drop:
    case MacroCommand::touch:
    case MacroCommand::part:
    case MacroCommand::execute:
    case MacroCommand::quit:
    case MacroCommand::over:
    case MacroCommand::done:
    case MacroCommand::edit:
    case MacroCommand::base:
    case MacroCommand::behavior:
    case MacroCommand::kill:
        return MacroCommandFamily::object_and_script_lifecycle;

    case MacroCommand::spot:
        return MacroCommandFamily::compound_geometry;

    case MacroCommand::set_creature_action_activation_boost:
        return MacroCommandFamily::creature_motor;

    case MacroCommand::less_than_creature:
    case MacroCommand::bacterium_update:
        return MacroCommandFamily::creature_runtime;

    case MacroCommand::vehicle_grab_passengers:
    case MacroCommand::vehicle_set_passenger:
        return MacroCommandFamily::vehicle_interaction;

    case MacroCommand::sound_start:
    case MacroCommand::sound_change:
    case MacroCommand::sound_loop:
    case MacroCommand::sound_fade:
    case MacroCommand::sound_query:
    case MacroCommand::sound_voice:
    case MacroCommand::sound_stop_channel:
    case MacroCommand::fade:
    case MacroCommand::sound_pause:
        return MacroCommandFamily::sound;

    case MacroCommand::say_hash:
    case MacroCommand::say_dollar:
    case MacroCommand::say_name:
    case MacroCommand::message:
    case MacroCommand::target:
    case MacroCommand::from_object:
        return MacroCommandFamily::speech_and_messaging;

    case MacroCommand::stimulus:
    case MacroCommand::speech_range_stimulus:
    case MacroCommand::sign_stimulus:
    case MacroCommand::tact_stimulus:
    case MacroCommand::event:
    case MacroCommand::remove_event:
    case MacroCommand::drive_presentation:
    case MacroCommand::trig:
    case MacroCommand::chem:
    case MacroCommand::fire:
    case MacroCommand::stm_hash:
        return MacroCommandFamily::stimulus_and_events;

    case MacroCommand::debug_message:
    case MacroCommand::debug_value:
    case MacroCommand::dbug:
    case MacroCommand::vocabulary:
    case MacroCommand::version:
    case MacroCommand::room:
    case MacroCommand::cabinet:
    case MacroCommand::knob:
    case MacroCommand::slim:
    case MacroCommand::tick:
    case MacroCommand::enumerate_objects:
    case MacroCommand::random_target:
    case MacroCommand::tool:
    case MacroCommand::walk:
    case MacroCommand::pointer:
    case MacroCommand::wait:
    case MacroCommand::approach:
    case MacroCommand::uppercase_approach:
    case MacroCommand::anim:
    case MacroCommand::hidden_fertilize:
        return MacroCommandFamily::debug_system_and_misc;
    default:
        break;
    }

    switch (static_cast<MacroPrefixCommand>(token)) {
    case MacroPrefixCommand::application:
    case MacroPrefixCommand::aim:
    case MacroPrefixCommand::blackboard:
    case MacroPrefixCommand::dde:
    case MacroPrefixCommand::system:
    case MacroPrefixCommand::new_object:
        return MacroCommandFamily::prefixed;
    default:
        return MacroCommandFamily::unknown;
    }
}

CaosToken Macro::peek_next_token() {
    // The arithmetic commands take their destination WITHOUT consuming it, so
    // their first ParseRValue re-reads that same token and yields the
    // destination's current value.  That is what makes "addv var0 3" mean
    // var0 = var0 + 3 rather than a three-operand form.
    if (script_cursor_offset + 5 > script_capacity_bytes ||
        script_cursor_offset + 4 > script_buffer.size()) {
        execution_terminated = true;
        return 0;
    }
    CaosToken token = 0;
    std::memcpy(&token, script_buffer.data() + script_cursor_offset,
                sizeof(token));
    return token;
}

CaosToken Macro::read_next_token() {
    if (script_cursor_offset + 5 > script_capacity_bytes ||
        script_cursor_offset + 4 > script_buffer.size()) {
        execution_terminated = true;
        return 0;
    }

    CaosToken token = 0;
    std::memcpy(&token, script_buffer.data() + script_cursor_offset,
                sizeof(token));
    script_cursor_offset += 5;
    return token;
}

void Macro::read_bracketed_text_argument(char* output_text,
                                         int output_capacity) {
    if (output_text == nullptr || output_capacity <= 0) {
        execution_terminated = true;
        return;
    }

    const std::size_t readable_capacity = std::min<std::size_t>(
        script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset >= readable_capacity ||
        script_buffer[script_cursor_offset] != '[') {
        *output_text = '\0';
        execution_terminated = true;
        return;
    }

    ++script_cursor_offset;
    int copied_text_length = 0;
    while (script_cursor_offset < readable_capacity &&
           script_buffer[script_cursor_offset] != ']') {
        if (copied_text_length < output_capacity - 1) {
            output_text[copied_text_length] =
                script_buffer[script_cursor_offset];
        }
        ++copied_text_length;
        ++script_cursor_offset;
    }

    output_text[std::min(copied_text_length, output_capacity - 1)] = '\0';
    if (script_cursor_offset < readable_capacity &&
        script_buffer[script_cursor_offset] == ']') {
        script_cursor_offset += 2;
        return;
    }
    execution_terminated = true;
}

std::string Macro::read_bracketed_text_argument() {
    std::array<char, 260> text{};
    read_bracketed_text_argument(text.data(),
                                 static_cast<int>(text.size()));
    return std::string(text.data());
}

void Macro::consume_execute_command_arguments() {
    // Native `exec` at 0041e357 is intentionally parser-only in this build:
    // it reads two 0x80-capacity bracketed fields and immediately joins the
    // common interpreter finalization path.  The native stack destinations
    // are dead after the second read, so the clean port keeps the cursor and
    // malformed-input semantics without exposing the decompiler's overlapping
    // local buffers or inventing a tool-launch side effect.
    std::array<char, 0x80> first_argument{};
    std::array<char, 0x80> second_argument{};
    read_bracketed_text_argument(first_argument.data(),
                                 static_cast<int>(first_argument.size()));
    read_bracketed_text_argument(second_argument.data(),
                                 static_cast<int>(second_argument.size()));
}

MacroControlFlowResult Macro::execute_global_subroutine_command() {
    // Native `gsub` pushes the cursor after its five-byte ID operand, then
    // jumps to the first `subr <id>` record in the script.  The marker scan is
    // deliberately byte-wise and unaligned: it compares every four-byte
    // window against `subr`, exactly as the x86 code at 0041e420-0041e46d
    // does.  A matching record is ten bytes wide (`subr`, separator, ID,
    // separator); execution resumes at the byte immediately after that
    // header.  The value-stack return offset and one-entry cache are Macro
    // state, while scheduling/cleanup remains outside this helper.
    const std::size_t readable_capacity = std::min<std::size_t>(
        script_capacity_bytes, script_buffer.size());
    const std::size_t return_cursor_offset = script_cursor_offset + 5;
    if (caos_value_stack_cursor_index >= caos_value_stack.size()) {
        execution_terminated = true;
    } else {
        caos_value_stack[caos_value_stack_cursor_index++] =
            static_cast<std::uint32_t>(return_cursor_offset);
    }

    std::uint32_t target_id = 0;
    if (script_cursor_offset > readable_capacity ||
        readable_capacity - script_cursor_offset < 5) {
        execution_terminated = true;
    } else {
        std::memcpy(&target_id, script_buffer.data() + script_cursor_offset,
                    sizeof(target_id));
    }

    if (target_id == subroutine_cache_id) {
        script_cursor_offset = std::min(subroutine_cache_cursor_offset,
                                         readable_capacity);
        return execution_terminated
                   ? MacroControlFlowResult::execution_terminated
                   : MacroControlFlowResult::cursor_changed;
    }

    constexpr CaosToken subroutine_marker =
        make_caos_token(std::string_view("subr"));
    std::size_t scan_offset = 0;
    while (scan_offset + sizeof(CaosToken) <= readable_capacity) {
        CaosToken token = 0;
        std::memcpy(&token, script_buffer.data() + scan_offset,
                    sizeof(token));
        ++scan_offset;
        if (token != subroutine_marker) {
            continue;
        }

        const std::size_t id_offset = scan_offset + 4;
        const std::size_t body_offset = scan_offset + 9;
        if (body_offset > readable_capacity) {
            execution_terminated = true;
            scan_offset = id_offset;
            continue;
        }

        std::uint32_t candidate_id = 0;
        std::memcpy(&candidate_id, script_buffer.data() + id_offset,
                    sizeof(candidate_id));
        scan_offset = body_offset;
        if (candidate_id == target_id) {
            subroutine_cache_id = target_id;
            subroutine_cache_cursor_offset = body_offset;
            script_cursor_offset = body_offset;
            return execution_terminated
                       ? MacroControlFlowResult::execution_terminated
                       : MacroControlFlowResult::cursor_changed;
        }
    }

    // The native path leaves the interpreter through its malformed/missing
    // subroutine cleanup path when no marker is found.  Preserve that state
    // explicitly instead of manufacturing a fallback target or raw label.
    execution_terminated = true;
    script_cursor_offset = std::min(scan_offset, readable_capacity);
    return MacroControlFlowResult::execution_terminated;
}

namespace {

constexpr CaosToken caos_token(char first, char second, char third,
                               char fourth) {
    return static_cast<CaosToken>(static_cast<unsigned char>(first)) |
           (static_cast<CaosToken>(static_cast<unsigned char>(second)) << 8) |
           (static_cast<CaosToken>(static_cast<unsigned char>(third)) << 16) |
           (static_cast<CaosToken>(static_cast<unsigned char>(fourth)) << 24);
}

constexpr CaosToken kLive = caos_token('l', 'i', 'v', 'e');
constexpr CaosToken kDied = caos_token('d', 'i', 'e', 'd');
constexpr CaosToken kPanc = caos_token('p', 'a', 'n', 'c');
constexpr CaosToken kGetB = caos_token('g', 'e', 't', 'b');
constexpr CaosToken kPutB = caos_token('p', 'u', 't', 'b');
constexpr CaosToken kWord = caos_token('w', 'o', 'r', 'd');
constexpr CaosToken kLobe = caos_token('l', 'o', 'b', 'e');
constexpr CaosToken kGene = caos_token('g', 'e', 'n', 'e');
constexpr CaosToken kGids = caos_token('g', 'i', 'd', 's');
constexpr CaosToken kCell = caos_token('c', 'e', 'l', 'l');
constexpr CaosToken kScrp = caos_token('s', 'c', 'r', 'p');
// `gids` granularities, in the order ExecuteDDECommand tests them.
constexpr CaosToken kRoot = caos_token('r', 'o', 'o', 't');
constexpr CaosToken kFmly = caos_token('f', 'm', 'l', 'y');
constexpr CaosToken kGnus = caos_token('g', 'n', 'u', 's');
constexpr CaosToken kSpcs = caos_token('s', 'p', 'c', 's');
constexpr CaosToken kPutV = caos_token('p', 'u', 't', 'v');
constexpr CaosToken kPutS = caos_token('p', 'u', 't', 's');
constexpr CaosToken kPict = caos_token('p', 'i', 'c', 't');
constexpr CaosToken kNege = caos_token('n', 'e', 'g', 'g');
constexpr CaosToken kHatc = caos_token('h', 'a', 't', 'c');

constexpr CaosToken kMonk = caos_token('m', 'o', 'n', 'k');
constexpr CaosToken kData = caos_token('d', 'a', 't', 'a');
constexpr CaosToken kOvvd = caos_token('o', 'v', 'v', 'd');
constexpr CaosToken kCnam = caos_token('c', 'n', 'a', 'm');
constexpr CaosToken kCtim = caos_token('c', 't', 'i', 'm');

constexpr CaosToken kBbdEmit = caos_token('e', 'm', 'i', 't');
constexpr CaosToken kBbdWord = caos_token('w', 'o', 'r', 'd');
constexpr CaosToken kBbdEdit = caos_token('e', 'd', 'i', 't');
constexpr CaosToken kBbdShow = caos_token('s', 'h', 'o', 'w');

constexpr CaosToken kNewCallButton = caos_token('c', 'b', 't', 'n');
constexpr CaosToken kNewGene = caos_token('g', 'e', 'n', 'e');
constexpr CaosToken kNewVehicle = caos_token('v', 'h', 'c', 'l');
constexpr CaosToken kNewScenery = caos_token('s', 'c', 'e', 'n');
constexpr CaosToken kNewCreature = caos_token('c', 'r', 'e', 'a');
constexpr CaosToken kNewBlackboard = caos_token('b', 'k', 'b', 'd');
constexpr CaosToken kNewPart = caos_token('p', 'a', 'r', 't');
constexpr CaosToken kNewSimpleObject = caos_token('s', 'i', 'm', 'p');
constexpr CaosToken kNewCompoundObject = caos_token('c', 'o', 'm', 'p');
constexpr CaosToken kNewLift = caos_token('l', 'i', 'f', 't');

constexpr CaosToken kRvalueObjectVariable = caos_token('o', 'b', 'v', 0);
constexpr CaosToken kRvalueObjectPointer = caos_token('o', 'b', 'j', 'p');
constexpr CaosToken kRvalueMovementMinYAlias = caos_token('c', 'a', 'g', 'e');
// YVEC per the SDK guide: the target's y movement vector.  Spelled 'cevy'
// the token could never match anything the interpreter reads.
constexpr CaosToken kRvalueVehicleMovementVectorY = caos_token('y', 'v', 'e', 'c');
constexpr CaosToken kRvalueIt = caos_token('_', 'i', 't', '_');
// DRV!, the creature's most pressing drive.
constexpr CaosToken kRvalueWinningDrive = caos_token('d', 'r', 'v', '!');
constexpr CaosToken kRvalueGroundBlockCount = caos_token('g', 'n', 'd', '#');
constexpr CaosToken kRvalueRoomCount = caos_token('r', 'm', 's', '#');
constexpr CaosToken kRvalueMovementMaxY = caos_token('l', 'i', 'm', 'b');
constexpr CaosToken kRvalueSoundRight = caos_token('p', 'o', 's', 'b');
// XVEC, the target's x movement vector.
constexpr CaosToken kRvalueVehicleMovementVectorX = caos_token('x', 'v', 'e', 'c');
constexpr CaosToken kRvalueAttentionIndex = caos_token('n', 'e', 'i', 'd');
constexpr CaosToken kRvalueExec = caos_token('e', 'x', 'e', 'c');
constexpr CaosToken kRvalueTouching = caos_token('t', 'o', 'u', 'c');
constexpr CaosToken kRvalueDead = caos_token('d', 'e', 'a', 'd');
constexpr CaosToken kRvalueOwnerGender = caos_token('g', 'e', 'n', 'd');
constexpr CaosToken kRvalueWind = caos_token('w', 'i', 'n', 'd');
constexpr CaosToken kRvalueGround = caos_token('g', 'r', 'n', 'd');
constexpr CaosToken kRvalueChemical = caos_token('c', 'h', 'e', 'm');
constexpr CaosToken kRvalueMovementMinX = caos_token('l', 'i', 'm', 'l');
constexpr CaosToken kRvalueSoundLeft = caos_token('p', 'o', 's', 'l');
constexpr CaosToken kRvalueTotal = caos_token('t', 'o', 't', 'l');
constexpr CaosToken kRvalueWidth = caos_token('w', 'd', 't', 'h');
constexpr CaosToken kRvaluePose = caos_token('p', 'o', 's', 'e');
constexpr CaosToken kRvalueTarget = caos_token('t', 'a', 'r', 'g');
constexpr CaosToken kRvalueWindowHeight = caos_token('w', 'i', 'n', 'h');
constexpr CaosToken kRvalueCountdownMinutes = caos_token('c', 'a', 'm', 'n');
constexpr CaosToken kRvalueRoom = caos_token('r', 'o', 'o', 'm');
constexpr CaosToken kRvalueFrom = caos_token('f', 'r', 'o', 'm');
constexpr CaosToken kRvalueToken = caos_token('t', 'o', 'k', 'n');
constexpr CaosToken kRvalueSelectedCreature = caos_token('n', 'o', 'r', 'n');
// VRSN as an rvalue yields the Creatures build id.
constexpr CaosToken kRvalueLanguageVersion = caos_token('v', 'r', 's', 'n');
constexpr CaosToken kRvalueAttentionObject = caos_token('a', 't', 't', 'n');
constexpr CaosToken kRvalueMinutes = caos_token('m', 'i', 'n', 's');
constexpr CaosToken kRvalueOwnerBounds = caos_token('c', 'a', 'r', 'r');
constexpr CaosToken kRvalueTargetBounds = caos_token('t', 'c', 'a', 'r');
constexpr CaosToken kRvalueSoundDescriptor = caos_token('a', 's', 'l', 'p');
constexpr CaosToken kRvalueTemperature = caos_token('t', 'e', 'm', 'p');
constexpr CaosToken kRvalueBump = caos_token('b', 'u', 'm', 'p');
constexpr CaosToken kRvalueMovementMaxX = caos_token('l', 'i', 'm', 'r');
constexpr CaosToken kRvalueOwner = caos_token('o', 'w', 'n', 'r');
constexpr CaosToken kRvalueScore = caos_token('s', 'c', 'o', 'r');
constexpr CaosToken kRvalueHour = caos_token('h', 'o', 'u', 'r');
constexpr CaosToken kRvalueSoundRightFromOrigin = caos_token('p', 'o', 's', 'r');
constexpr CaosToken kRvaluePointerTool = caos_token('p', 'n', 't', 'r');
constexpr CaosToken kRvalueAttributes = caos_token('a', 't', 't', 'r');
constexpr CaosToken kRvalueClassifier = caos_token('c', 'l', 'a', 's');
constexpr CaosToken kRvalueSpecies = caos_token('s', 'p', 'c', 's');
constexpr CaosToken kRvalueSoundSettings = caos_token('s', 'n', 'd', 's');
constexpr CaosToken kRvalueDrive = caos_token('d', 'r', 'i', 'v');
constexpr CaosToken kRvalueHeight = caos_token('h', 'g', 'h', 't');
constexpr CaosToken kRvalueHotObject = caos_token('h', 'o', 't', 's');
constexpr CaosToken kRvalueGenus = caos_token('g', 'n', 'u', 's');
constexpr CaosToken kRvalueMovementMode = caos_token('m', 'o', 'v', 's');
constexpr CaosToken kRvalueEditObject = caos_token('e', 'd', 'i', 't');
constexpr CaosToken kRvalueMovementMinY = caos_token('l', 'i', 'm', 't');
constexpr CaosToken kRvalueSoundSourceY = caos_token('p', 'o', 's', 't');
constexpr CaosToken kRvalueCameraX = caos_token('c', 'm', 'r', 'x');
constexpr CaosToken kRvalueActiveEvent = caos_token('a', 'c', 't', 'v');
constexpr CaosToken kRvalueGroundWidth = caos_token('g', 'n', 'd', 'w');
constexpr CaosToken kRvalueWindowWidth = caos_token('w', 'i', 'n', 'w');
constexpr CaosToken kRvalueMovementMaxYAlias = caos_token('b', 'a', 'b', 'y');
constexpr CaosToken kRvalueFamily = caos_token('f', 'm', 'l', 'y');
constexpr CaosToken kRvalueCameraY = caos_token('c', 'm', 'r', 'y');

CaosToken token_at_cursor(const Macro& macro) {
    const std::size_t readable_capacity = std::min<std::size_t>(
        macro.script_capacity_bytes, macro.script_buffer.size());
    if (macro.script_cursor_offset < 5 ||
        macro.script_cursor_offset - 5 + sizeof(CaosToken) > readable_capacity) {
        return 0;
    }

    CaosToken token = 0;
    std::memcpy(&token, macro.script_buffer.data() +
                              macro.script_cursor_offset - 5,
                sizeof(token));
    return token;
}

std::string script_excerpt_at_cursor(const Macro& macro) {
    const std::size_t readable_capacity = std::min<std::size_t>(
        macro.script_capacity_bytes, macro.script_buffer.size());
    const std::size_t start = macro.script_cursor_offset > 20
                                  ? macro.script_cursor_offset - 20
                                  : 0;
    if (start >= readable_capacity) {
        return {};
    }

    const std::size_t length = std::min<std::size_t>(
        0x30, readable_capacity - start);
    return macro.script_buffer.substr(start, length);
}

void append_pipe_field(std::string& output, std::string_view field) {
    output.append(field);
    output.push_back('|');
}

} // namespace

void Macro::report_syntax_error(MacroCommandHost& host,
                                std::string_view expected_token_type) {
    censor_script_profanity();
    MacroSyntaxDiagnostic diagnostic;
    diagnostic.expected_token_type = std::string(expected_token_type);
    diagnostic.offending_token = token_at_cursor(*this);
    diagnostic.script_excerpt = script_excerpt_at_cursor(*this);
    host.report_syntax_error(*this, diagnostic);
}

namespace {

std::uint32_t object_pointer_value(const objects::Object* object) {
    return static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(object));
}

std::uint32_t classifier_family(const objects::Object& object) {
    return (object.classifier_base() >> 24) & 0xffu;
}

std::uint32_t classifier_genus(const objects::Object& object) {
    return (object.classifier_base() >> 16) & 0xffu;
}

std::uint32_t classifier_species(const objects::Object& object) {
    return (object.classifier_base() >> 8) & 0xffu;
}

} // namespace

std::uint32_t Macro::parse_rvalue(MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics) {
    const std::size_t readable_capacity = std::min<std::size_t>(
        script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset >= readable_capacity) {
        execution_terminated = true;
        report_syntax_error(diagnostics, "Rvalue");
        return 0;
    }

    const char initial_character = script_buffer[script_cursor_offset];
    if (initial_character < ':') {
        // ParseRValue @0041a9d0 advances the cursor PAST each character before
        // testing it, so the separator that ends a numeric literal is consumed
        // as part of reading that literal.  Stopping on the separator instead
        // left it for the next operand, which then parsed the space itself as
        // a digit: every CAOS command with more than one numeric argument read
        // garbage from its second argument onward.
        if (initial_character == '-') {
            ++script_cursor_offset;
            std::uint32_t value = 0;
            if (script_cursor_offset < readable_capacity) {
                value = 0u - static_cast<std::uint32_t>(
                    script_buffer[script_cursor_offset] - '0');
                ++script_cursor_offset;
            }
            while (script_cursor_offset < readable_capacity) {
                const char digit = script_buffer[script_cursor_offset];
                ++script_cursor_offset;
                if (digit < '0') {
                    return value;
                }
                value = value * 10u - static_cast<std::uint32_t>(digit - '0');
            }
            return value;
        }

        std::uint32_t value = static_cast<std::uint32_t>(
            initial_character - '0');
        ++script_cursor_offset;
        while (script_cursor_offset < readable_capacity) {
            const char digit = script_buffer[script_cursor_offset];
            ++script_cursor_offset;
            if (digit < '0') {
                return value;
            }
            value = value * 10u + static_cast<std::uint32_t>(digit - '0');
        }
        return value;
    }

    if (script_cursor_offset + sizeof(CaosToken) + 1 > readable_capacity) {
        execution_terminated = true;
        report_syntax_error(diagnostics, "Rvalue");
        return 0;
    }

    const CaosToken token = read_next_token();
    const std::uint32_t token_low = token & 0x00ffffffu;
    const std::uint32_t token_index = token >> 24;
    objects::Object* target = object_context.target_object;

    if (token_low == (caos_token('v', 'a', 'r', 0) & 0x00ffffffu)) {
        // var0..var9.  AssignLValue @ the native computes
        // caos_value_stack[(token >> 24) - 0x1a]; for '0' that index is 22,
        // and 24 + 4*22 is 112, which is exactly caos_work_values[0].  The
        // ten work values ARE the macro variables -- the port had no rvalue
        // case for them at all, so every `var` read answered zero.
        const std::uint32_t index =
            token_index - static_cast<std::uint32_t>('0');
        return index < caos_work_values.size() ? caos_work_values[index] : 0u;
    }
    if (token_low == (kRvalueObjectVariable & 0x00ffffffu)) {
        if (token_index < 0x30u || token_index > 0x32u || target == nullptr) {
            return 0;
        }
        return target->object_variable(token_index - 0x30u);
    }
    if (token == kRvalueObjectPointer) {
        return object_context.script_owner == nullptr
                   ? 0
                   : object_pointer_value(
                         object_context.script_owner->caos_object_pointer());
    }
    if (token == kRvalueMovementMinYAlias) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->movement_bounds().min_y);
    }
    if (token == kRvalueVehicleMovementVectorY) {
        // YVEC: the vehicle's y movement vector, not a packed bounds state.
        return target == nullptr
                   ? 0
                   : static_cast<std::uint32_t>(
                         runtime.vehicle_movement_vector_y(*target));
    }
    if (token == kRvalueIt || token == kRvalueAttentionObject) {
        return object_pointer_value(object_context.it_object);
    }
    if (token == kRvalueWinningDrive) {
        return target == nullptr || !runtime.is_creature_object(*target)
                   ? 0
                   : runtime.creature_value(
                         *target, MacroCreatureValue::winning_drive, 0);
    }
    if (token == kRvalueGroundBlockCount) {
        return 0x105u;
    }
    if (token == kRvalueRoomCount) {
        return runtime.room_count();
    }
    if (token == kRvalueMovementMaxY) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->movement_bounds().max_y);
    }
    if (token == kRvalueSoundRight) {
        return target == nullptr
                   ? 0
                   : static_cast<std::uint32_t>(
                         target->sound_source_y() + target->current_visual_height());
    }
    if (token == kRvalueVehicleMovementVectorX) {
        // XVEC: the vehicle's x movement vector, not the classifier.
        return target == nullptr
                   ? 0
                   : static_cast<std::uint32_t>(
                         runtime.vehicle_movement_vector_x(*target));
    }
    if (token == kRvalueAttentionIndex) {
        return target == nullptr ? 0 : runtime.attention_record_index(*target);
    }
    if (token == kRvalueExec) {
        return object_pointer_value(object_context.exec_object);
    }
    if (token == kRvalueTouching) {
        objects::Object* first = reinterpret_cast<objects::Object*>(
            static_cast<std::uintptr_t>(parse_rvalue(runtime, diagnostics)));
        objects::Object* second = reinterpret_cast<objects::Object*>(
            static_cast<std::uintptr_t>(parse_rvalue(runtime, diagnostics)));
        if (first == nullptr || second == nullptr) {
            return 0;
        }
        world::WorldRect first_bounds{};
        world::WorldRect second_bounds{};
        if (!first->get_bounds(&first_bounds) ||
            !second->get_bounds(&second_bounds)) {
            return 0;
        }
        return world::wrapped_world_rects_overlap(first_bounds, second_bounds)
                   ? 1u
                   : 0u;
    }
    if (token == kRvalueDead) {
        return target == nullptr || !runtime.is_creature_object(*target)
                   ? 0
                   : runtime.creature_value(
                         *target, MacroCreatureValue::death_state, 0);
    }
    if (token == kRvalueOwnerGender) {
        objects::Object* owner = object_context.script_owner;
        return owner == nullptr || !runtime.is_creature_object(*owner)
                   ? 0
                   : runtime.creature_value(
                         *owner, MacroCreatureValue::gender, 0);
    }
    if (token == kRvalueWind) {
        return static_cast<std::uint32_t>(
            static_cast<std::int32_t>(runtime.ambient_wind()));
    }
    if (token == kRvalueGround) {
        const std::uint32_t x_block = parse_rvalue(runtime, diagnostics);
        return x_block <= 0x104u ? static_cast<std::uint32_t>(
                                       runtime.ground_height(x_block))
                                 : 0;
    }
    if (token == kRvalueChemical) {
        const std::uint32_t index = parse_rvalue(runtime, diagnostics);
        return target == nullptr || index > 0xffu ||
                       !runtime.is_creature_object(*target)
                   ? 0
                   : runtime.creature_value(
                         *target, MacroCreatureValue::chemical_concentration,
                         index);
    }
    if (token == kRvalueMovementMinX) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->movement_bounds().min_x);
    }
    if (token == kRvalueSoundLeft) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->sound_source_x());
    }
    if (token == kRvalueTotal) {
        const std::uint32_t family = parse_rvalue(runtime, diagnostics);
        const std::uint32_t genus = parse_rvalue(runtime, diagnostics);
        const std::uint32_t species = parse_rvalue(runtime, diagnostics);
        return runtime.enabled_object_count_matching(family, genus, species);
    }
    if (token == kRvalueWidth) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->current_visual_width());
    }
    if (token == kRvaluePose) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->relative_image_index(
                                           selected_part_index));
    }
    if (token == kRvalueTarget) {
        return object_pointer_value(target);
    }
    if (token == kRvalueWindowHeight) {
        return runtime.viewport_value(MacroViewportValue::height);
    }
    if (token == kRvalueCountdownMinutes) {
        return target == nullptr ? 0 : static_cast<std::uint32_t>(
            target->timer_countdown_for_script() / 600);
    }
    if (token == kRvalueRoom) {
        const std::uint32_t room_index = parse_rvalue(runtime, diagnostics);
        const std::uint32_t field = parse_rvalue(runtime, diagnostics);
        if (room_index >= runtime.room_count()) {
            return 0;
        }
        return static_cast<std::uint32_t>(
            runtime.room_value(room_index, field));
    }
    if (token == kRvalueFrom) {
        return object_pointer_value(object_context.from_object);
    }
    if (token == kRvalueToken) {
        return read_next_token();
    }
    if (token == kRvalueSelectedCreature) {
        return object_pointer_value(runtime.selected_creature());
    }
    if (token == kRvalueLanguageVersion) {
        return runtime.language_version();
    }
    if (token == kRvalueMinutes) {
        return (runtime.world_tick_count() / 600u) % 60u;
    }
    if (token == kRvalueOwnerBounds) {
        return object_context.script_owner == nullptr
                   ? 0
                   : object_pointer_value(
                         object_context.script_owner->bounds_reference_object());
    }
    if (token == kRvalueTargetBounds) {
        return target == nullptr ? 0
                                 : object_pointer_value(
                                       target->bounds_reference_object());
    }
    if (token == kRvalueSoundDescriptor) {
        return target == nullptr ? 0
                                 : target->continuous_sound_descriptor() >> 24;
    }
    if (token == kRvalueTemperature) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       runtime.ambient_temperature_at(*target));
    }
    if (token == kRvalueBump) {
        return target == nullptr ? 0
                                 : target->current_interaction_event_id_for_script();
    }
    if (token == kRvalueMovementMaxX) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->movement_bounds().max_x);
    }
    if (token == kRvalueOwner) {
        return object_pointer_value(object_context.script_owner);
    }
    if (token == kRvalueScore) {
        const std::uint32_t index = parse_rvalue(runtime, diagnostics);
        return index <= 4u ? runtime.score_value(index) : 0;
    }
    if (token == kRvalueHour) {
        return runtime.world_tick_count() / 36000u;
    }
    if (token == kRvalueSoundRightFromOrigin) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->sound_source_x() +
                                       target->current_visual_width());
    }
    if (token == kRvaluePointerTool) {
        return object_pointer_value(runtime.pointer_tool());
    }
    if (token == kRvalueAttributes) {
        return target == nullptr ? 0 : target->script_bounds_flags();
    }
    if (token == kRvalueClassifier) {
        return target == nullptr ? 0 : target->classifier_base();
    }
    if (token == kRvalueSpecies) {
        return target == nullptr ? 0 : classifier_species(*target);
    }
    if (token == kRvalueSoundSettings) {
        return runtime.sound_settings();
    }
    if (token == kRvalueDrive) {
        const std::uint32_t index = parse_rvalue(runtime, diagnostics);
        return target == nullptr || index > 0xfu ||
                       !runtime.is_creature_object(*target)
                   ? 0
                   : runtime.creature_value(
                         *target, MacroCreatureValue::drive_level, index);
    }
    if (token == kRvalueHeight) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->current_visual_height());
    }
    if (token == kRvalueHotObject) {
        return object_pointer_value(runtime.topmost_pointer_object());
    }
    if (token == kRvalueGenus) {
        return target == nullptr ? 0 : classifier_genus(*target);
    }
    if (token == kRvalueMovementMode) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->bounds_mode());
    }
    if (token == kRvalueEditObject) {
        return object_pointer_value(runtime.edit_object());
    }
    if (token == kRvalueMovementMinY) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->movement_bounds().min_y);
    }
    if (token == kRvalueSoundSourceY) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->sound_source_y());
    }
    if (token == kRvalueCameraX) {
        return runtime.viewport_value(MacroViewportValue::center_x);
    }
    if (token == kRvalueActiveEvent) {
        return target == nullptr ? 0
                                 : target->current_interaction_event_id_for_script();
    }
    if (token == kRvalueGroundWidth) {
        return 0x20u;
    }
    if (token == kRvalueWindowWidth) {
        return runtime.viewport_value(MacroViewportValue::width);
    }
    if (token == kRvalueMovementMaxYAlias) {
        return target == nullptr ? 0
                                 : static_cast<std::uint32_t>(
                                       target->movement_bounds().max_y);
    }
    if (token == kRvalueFamily) {
        return target == nullptr ? 0 : classifier_family(*target);
    }
    if (token == kRvalueCameraY) {
        return runtime.viewport_value(MacroViewportValue::center_y);
    }

    report_syntax_error(diagnostics, "Rvalue");
    return 0;
}

void Macro::assign_lvalue(MacroRuntimeHost& runtime,
                          MacroCommandHost& diagnostics,
                          CaosToken destination_token,
                          std::uint32_t value) {
    const std::uint32_t token_low = destination_token & 0x00ffffffu;
    const std::uint32_t token_index = destination_token >> 24;
    objects::Object* target = object_context.target_object;

    if (token_low == (caos_token('v', 'a', 'r', 0) & 0x00ffffffu)) {
        // The guard admitted token characters 0x1a..0x2d, which are control
        // codes; var0..var9 carry '0'..'9', so no assignment ever landed.
        const std::uint32_t index =
            token_index - static_cast<std::uint32_t>('0');
        if (index < caos_work_values.size()) {
            caos_work_values[index] = value;
            return;
        }
    } else if (token_low == (kRvalueObjectVariable & 0x00ffffffu)) {
        if (token_index >= 0x30u && token_index <= 0x32u && target != nullptr) {
            target->set_object_variable(token_index - 0x30u, value);
            return;
        }
    } else if (destination_token == kRvalueAttributes) {
        if (target != nullptr) {
            target->set_bounds_flags_for_script(value);
            return;
        }
    } else if (destination_token == kRvalueWindowHeight) {
        const std::uint32_t height = static_cast<std::int32_t>(value) < 0x4b0
                                         ? value
                                         : 0x4a0u;
        runtime.set_viewport_value(MacroViewportValue::height, height);
        return;
    } else if (destination_token == kRvalueVehicleMovementVectorX) {
        // Native AssignLValue @ 0x0041b9f0's real xvec write, confirmed via
        // raw disassembly: MOV EAX,[ESP+8] (the assigned value); MOV
        // [target+0x144],EAX -- a plain, unscaled write into the vehicle's
        // own velocity_x_8_8 field. This previously called
        // set_classifier_base(value) instead, which native's real xvec
        // write never touches at all -- every `setv xvec` in the game was
        // silently corrupting the target's classifier rather than setting
        // its velocity, and once corrupted the object no longer matches any
        // enum/totl/next query for its real type. Root cause of a live
        // submarine that never receives its own stop command.
        if (target != nullptr && classifier_family(*target) == 3u) {
            runtime.set_vehicle_movement_vector_x(*target, value);
            return;
        }
    } else if (destination_token == kRvalueVehicleMovementVectorY) {
        // Same fault, same fix: native's real yvec write is MOV
        // [target+0x148],EAX -- velocity_y_8_8, not the bounds-mode/flags
        // byte pair set_packed_bounds_state_for_script actually writes.
        if (target != nullptr && classifier_family(*target) == 3u) {
            runtime.set_vehicle_movement_vector_y(*target, value);
            return;
        }
    } else if (destination_token == kRvalueSelectedCreature) {
        runtime.set_selected_creature(
            reinterpret_cast<objects::Object*>(static_cast<std::uintptr_t>(value)),
            true);
        return;
    } else if (destination_token == kRvalueObjectPointer) {
        if (object_context.script_owner != nullptr) {
            object_context.script_owner->set_caos_object_pointer(
                reinterpret_cast<objects::Object*>(
                    static_cast<std::uintptr_t>(value)));
            return;
        }
    } else if (destination_token == kRvalueActiveEvent) {
        if (target != nullptr) {
            target->set_current_interaction_event_id_for_script(value);
            return;
        }
    } else if (destination_token == kRvalueClassifier) {
        if (target != nullptr) {
            target->set_classifier_base(value);
            return;
        }
    } else if (destination_token == kRvalueMovementMode) {
        if (target != nullptr) {
            runtime.set_object_bounds_mode(*target, value);
            return;
        }
    } else if (destination_token == kRvalueWindowWidth) {
        const std::uint32_t width = static_cast<std::int32_t>(value) < 0x20a0
                                        ? value
                                        : 0x2090u;
        runtime.set_viewport_value(MacroViewportValue::width, width);
        return;
    } else if (destination_token == kRvalueMovementMaxYAlias) {
        if (target != nullptr && classifier_family(*target) == 4u) {
            runtime.set_creature_value(
                *target, MacroCreatureAssignment::egg_movement_limit, value);
        }
        runtime.note_egg_state_change();
        return;
    }

    report_syntax_error(diagnostics, "Lvalue");
}

void Macro::handle_script_execution_exception(MacroExceptionHost& host) {
    censor_script_profanity();
    MacroExecutionExceptionDiagnostic diagnostic;
    diagnostic.offending_token = token_at_cursor(*this);
    diagnostic.script_excerpt = script_excerpt_at_cursor(*this);
    if (object_context.script_owner != nullptr) {
        const std::uint32_t packed_classifier =
            object_context.script_owner->classifier_base();
        diagnostic.owning_classifier.event = static_cast<ScriptEvent>(
            packed_classifier & 0xffu);
        diagnostic.owning_classifier.species = static_cast<std::uint8_t>(
            packed_classifier >> 8);
        diagnostic.owning_classifier.genus = static_cast<std::uint8_t>(
            packed_classifier >> 16);
        diagnostic.owning_classifier.family = static_cast<std::uint8_t>(
            packed_classifier >> 24);
    }
    host.report_execution_exception(*this, diagnostic);
}

void Macro::execute_application_prefix_command(
    MacroApplicationHost& host, MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    // The native app: branch reads the nested token directly and checks for
    // the complete five-byte record before advancing the cursor.
    if (script_cursor_offset > readable_capacity ||
        readable_capacity - script_cursor_offset < sizeof(CaosToken) + 1) {
        execution_terminated = true;
        report_syntax_error(host, "'app:' command");
        return;
    }

    const auto subcommand = static_cast<MacroApplicationSubcommand>(
        read_next_token());
    if (subcommand == MacroApplicationSubcommand::quit) {
        host.shutdown_embedded_kit_tool(parse_rvalue(runtime, host));
        return;
    }

    report_syntax_error(host, "'app:' command");
}

bool Macro::execute_tool_command(MacroCommand command,
                                 MacroApplicationHost& host,
                                 MacroRuntimeHost& runtime) {
    if (command != MacroCommand::tool) {
        return false;
    }

    // Native `tool` reads three bracketed fields with capacities 0x20, 0x20,
    // and 0x40, then one rvalue.  The OLE/registry record copy and menu
    // rebuild are application/platform work; no decompiler-shaped 0xb4-byte
    // local belongs in the interpreter layer.
    std::array<char, 0x20> registry_name{};
    std::array<char, 0x20> display_name{};
    std::array<char, 0x40> launch_command{};
    read_bracketed_text_argument(registry_name.data(), registry_name.size());
    read_bracketed_text_argument(display_name.data(), display_name.size());
    read_bracketed_text_argument(launch_command.data(),
                                 launch_command.size());
    const auto raw_type_code = static_cast<std::uint8_t>(
        parse_rvalue(runtime, host) & 0xffu);
    host.register_embedded_kit_tool(
        *this, std::string_view(registry_name.data()),
        std::string_view(display_name.data()),
        std::string_view(launch_command.data()), raw_type_code);
    return true;
}

void Macro::execute_aim_prefix_command(MacroObjectMotionHost& host,
                                       MacroRuntimeHost& runtime) {
    // Native aim: parses exactly one rvalue.  An absent or non-creature target
    // is a no-op after parsing; it is not a syntax failure.
    const std::uint32_t part_index = parse_rvalue(runtime, host);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        host.set_motion_target_part_index(*target, part_index);
    }
}

void Macro::execute_stimulus_prefix_command(MacroStimulusHost& host,
                                            MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    // The native stm# branch validates the complete nested five-byte token
    // before consuming it. A truncated nested token follows the interpreter's
    // existing terminated/syntax-error path.
    if (script_cursor_offset > readable_capacity ||
        readable_capacity - script_cursor_offset < sizeof(CaosToken) + 1) {
        execution_terminated = true;
        report_syntax_error(host, "'stm#' command");
        return;
    }

    const auto subcommand = static_cast<MacroStimulusSubcommand>(
        read_next_token());
    if (subcommand == MacroStimulusSubcommand::sign ||
        subcommand == MacroStimulusSubcommand::tact) {
        // Native sign/tact each parse one rvalue and pass the script owner to
        // a distinct Creature fan-out helper.  The helper owns the signed
        // stimulus-index range check and queue policy.
        const auto stimulus_index = static_cast<std::int32_t>(
            parse_rvalue(runtime, host));
        objects::Object* source = object_context.script_owner;
        if (source == nullptr) {
            return;
        }
        if (subcommand == MacroStimulusSubcommand::sign) {
            host.queue_sign_stimulus(*source, stimulus_index);
        } else {
            host.queue_tact_stimulus(*source, stimulus_index);
        }
        return;
    }

    if (subcommand == MacroStimulusSubcommand::speech_range) {
        // Native shou is an inline speech-range walk, not an ordinary event
        // broadcast.  The Creature host owns its coordinate predicate,
        // built-in context lookup, and ring submission.
        const auto stimulus_index = static_cast<std::int32_t>(
            parse_rvalue(runtime, host));
        objects::Object* source = object_context.script_owner;
        if (source != nullptr) {
            host.queue_speech_range_stimulus(*source, stimulus_index);
        }
        return;
    }

    if (subcommand != MacroStimulusSubcommand::write) {
        // Unknown nested stimulus tokens are syntax errors.
        report_syntax_error(host, "'stm#' command");
        return;
    }

    const auto target_value = parse_rvalue(runtime, host);
    const auto stimulus_index = static_cast<std::int32_t>(
        parse_rvalue(runtime, host));
    auto* target = reinterpret_cast<objects::Object*>(
        static_cast<std::uintptr_t>(target_value));
    objects::Object* source = object_context.script_owner;

    // The recovered compare is signed and upper-bounded (`JL 0x24`), exactly
    // as in the native branch. The host owns the target Creature's indexed
    // built-in context and can apply its queue policy without exposing raw
    // array arithmetic here.
    if (stimulus_index >= 0x24) {
        host.report_invalid_stimulus_index(stimulus_index);
        return;
    }
    if (source != nullptr && target != nullptr &&
        runtime.is_creature_object(*target)) {
        host.queue_built_in_stimulus(*source, *target, stimulus_index);
    }
}

void Macro::execute_stimulus_command(MacroStimulusHost& host,
                                     MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    // The native `stim` branch validates and consumes one complete nested
    // five-byte mode token before parsing the payload. Unknown modes still
    // consume the complete payload and return through normal finalisation.
    if (script_cursor_offset > readable_capacity ||
        readable_capacity - script_cursor_offset < sizeof(CaosToken) + 1) {
        execution_terminated = true;
        report_syntax_error(host, "'stim' command");
        return;
    }

    const auto subcommand = static_cast<MacroStimulusSubcommand>(
        read_next_token());
    objects::Object* direct_target = nullptr;
    if (subcommand == MacroStimulusSubcommand::write) {
        direct_target = reinterpret_cast<objects::Object*>(
            static_cast<std::uintptr_t>(parse_rvalue(runtime, host)));
    } else if (subcommand == MacroStimulusSubcommand::from) {
        direct_target = object_context.from_object;
    }

    creatures1::creatures::StimulusContext stimulus{};
    stimulus.descriptor.attention_activation = static_cast<std::uint8_t>(
        parse_rvalue(runtime, host));
    stimulus.descriptor.target_neuron_index = static_cast<std::uint8_t>(
        parse_rvalue(runtime, host));
    stimulus.descriptor.target_lobe_activation = static_cast<std::uint8_t>(
        parse_rvalue(runtime, host));
    stimulus.descriptor.flags = static_cast<std::uint8_t>(
        parse_rvalue(runtime, host));

    for (std::size_t index = 0; index < 4; ++index) {
        const auto chemical_id = static_cast<std::uint8_t>(
            parse_rvalue(runtime, host));
        const auto chemical_amount = static_cast<std::uint8_t>(
            parse_rvalue(runtime, host));
        switch (index) {
        case 0:
            stimulus.chemical_ids.first = chemical_id;
            stimulus.chemical_amounts.first = chemical_amount;
            break;
        case 1:
            stimulus.chemical_ids.second = chemical_id;
            stimulus.chemical_amounts.second = chemical_amount;
            break;
        case 2:
            stimulus.chemical_ids.third = chemical_id;
            stimulus.chemical_amounts.third = chemical_amount;
            break;
        default:
            stimulus.chemical_ids.fourth = chemical_id;
            stimulus.chemical_amounts.fourth = chemical_amount;
            break;
        }
    }

    objects::Object* source = object_context.script_owner;
    if (source == nullptr) {
        return;
    }

    switch (subcommand) {
    case MacroStimulusSubcommand::sign:
        host.queue_stimulus_for_perceiving(*source, stimulus);
        return;
    case MacroStimulusSubcommand::tact:
        host.queue_stimulus_for_overlapping(*source, stimulus);
        return;
    case MacroStimulusSubcommand::speech_range:
        host.queue_stimulus_in_speech_range(*source, stimulus);
        return;
    case MacroStimulusSubcommand::write:
    case MacroStimulusSubcommand::from:
        if (direct_target != nullptr &&
            runtime.is_creature_object(*direct_target)) {
            host.queue_direct_stimulus(*source, *direct_target, stimulus);
        }
        return;
    default:
        return;
    }
}

void Macro::execute_message_command(MacroMessageHost& host,
                                    MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    // Native `mesg` validates the complete nested five-byte record before
    // consuming it.  A truncated nested token follows the interpreter's
    // terminated/syntax-error path.
    if (script_cursor_offset > readable_capacity ||
        readable_capacity - script_cursor_offset < sizeof(CaosToken) + 1) {
        execution_terminated = true;
        report_syntax_error(host, "'mesg' command");
        return;
    }

    const auto subcommand = static_cast<MacroMessageSubcommand>(
        read_next_token());
    switch (subcommand) {
    case MacroMessageSubcommand::write: {
        // Native `mesg writ` parses target first, then event id.  The source
        // is the script owner and the direct queue record carries argument 0.
        auto* target = reinterpret_cast<objects::Object*>(
            static_cast<std::uintptr_t>(parse_rvalue(runtime, host)));
        const auto event_id = static_cast<objects::ObjectEventId>(
            parse_rvalue(runtime, host));
        objects::Object* source = object_context.script_owner;
        if (source != nullptr && target != nullptr) {
            host.queue_immediate_object_event(*source, *target, event_id);
        }
        return;
    }
    case MacroMessageSubcommand::sign: {
        const auto event_id = static_cast<objects::ObjectEventId>(
            parse_rvalue(runtime, host));
        objects::Object* source = object_context.script_owner;
        if (source != nullptr) {
            host.queue_perception_message(*source, event_id);
        }
        return;
    }
    case MacroMessageSubcommand::tact: {
        const auto event_id = static_cast<objects::ObjectEventId>(
            parse_rvalue(runtime, host));
        objects::Object* source = object_context.script_owner;
        if (source != nullptr) {
            host.queue_tactile_message(*source, event_id);
        }
        return;
    }
    case MacroMessageSubcommand::speech_range: {
        const auto event_id = static_cast<objects::ObjectEventId>(
            parse_rvalue(runtime, host));
        objects::Object* source = object_context.script_owner;
        if (source != nullptr) {
            host.queue_speech_range_message(*source, event_id);
        }
        return;
    }
    }

    // The native nested dispatcher consumes an unrecognised five-byte
    // subcommand and then resumes the outer interpreter iteration; it does
    // not misclassify it as an ObjectEventId or as `stm#`.
}

bool Macro::execute_chemical_command(MacroCommand command,
                                     MacroRuntimeHost& runtime,
                                     MacroCommandHost& diagnostics) {
    if (command != MacroCommand::chem) {
        return false;
    }

    // Native `chem` parses chemical id first and moles second.  It then
    // operates on the current target only when that object is a Creature;
    // the Biochemistry adapter owns the byte-table update, saturation, and
    // optional category-0x10 diagnostic logging.
    const std::uint32_t chemical_index = parse_rvalue(runtime, diagnostics);
    const std::uint32_t moles = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.add_creature_chemical_moles(*target, chemical_index, moles);
    }
    return true;
}

bool Macro::execute_fire_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroCommandHost& diagnostics) {
    if (command != MacroCommand::fire) {
        return false;
    }

    // Native `fire` consumes all three rvalues before checking the current
    // target.  ParseRValue returns an unsigned storage value, so the explicit
    // int32 conversions preserve the signed comparisons used by the native
    // activation clamp and coordinate tests.
    const std::int32_t global_x = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t global_y = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t activation = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));

    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.fire_creature_neuron(*target, global_x, global_y, activation);
    }
    return true;
}

bool Macro::execute_trigger_command(MacroCommand command,
                                    MacroRuntimeHost& runtime,
                                    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::trig) {
        return false;
    }

    // Native `trig` parses lobe index, neuron index, and activation in that
    // order. It then admits only a Creature target and replaces the two
    // byte-sized neuron lanes when the neuron index is below that lobe's
    // native count. The Brain/Lobe adapter owns those bounds and stores.
    const std::uint32_t lobe_index = parse_rvalue(runtime, diagnostics);
    const std::uint32_t neuron_index = parse_rvalue(runtime, diagnostics);
    const std::uint8_t activation = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.set_creature_brain_neuron_activation(
            *target, lobe_index, neuron_index, activation);
    }
    return true;
}

bool Macro::execute_delete_creature_command(MacroCommand command,
                                            MacroRuntimeHost& runtime) {
    if (command != MacroCommand::delete_creature) {
        return false;
    }

    // Native `cdie` has no operands.  It checks the current target's
    // classifier family and delegates the complete death policy to
    // Creature::Die; the runtime adapter supplies its CreatureDeathHost.
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.die_creature(*target);
    }
    return true;
}

bool Macro::execute_sleep_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics) {
    if (command != MacroCommand::sleep) {
        return false;
    }

    // Native `aslp` parses one operand before checking the current target.
    // Creature::SetSleepIndicator treats any nonzero value as enabled; keep
    // that conversion at the runtime boundary rather than inventing a second
    // sleep-indicator implementation in Macro.
    const std::uint32_t enabled = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.set_creature_sleep_indicator(*target, enabled);
    }
    return true;
}

bool Macro::execute_dream_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics) {
    if (command != MacroCommand::dream) {
        return false;
    }

    // Native `drea` parses one operand before checking the current target. It
    // then stores the value in Creature::InstinctRuntimeState::dream_countdown
    // for a family-0x04 target; the runtime adapter owns the state transition.
    const std::uint32_t countdown = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.set_creature_dream_countdown(*target, countdown);
    }
    return true;
}

bool Macro::execute_drop_command(MacroCommand command,
                                 MacroRuntimeHost& runtime) {
    if (command != MacroCommand::drop) {
        return false;
    }

    // Native `drop` has no operands.  For a Creature target it delegates the
    // complete dependent-notification policy, including the built-in-stimulus
    // fallback when no dependent objects are present.
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.notify_creature_dependents_on_removal(*target);
    }
    return true;
}

bool Macro::execute_done_command(MacroCommand command,
                                 MacroRuntimeHost& runtime) {
    if (command != MacroCommand::done) {
        return false;
    }

    // Native `done` has no operands and only acts on a Creature target. The
    // motor policy is kept on Creature; the runtime supplies object identity.
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.stop_creature_involuntary_action(*target);
    }
    return true;
}

MacroControlFlowResult Macro::execute_touch_command(
    MacroCommand command, MacroRuntimeHost& runtime) {
    if (command != MacroCommand::touch) {
        return MacroControlFlowResult::execution_terminated;
    }

    // Native `touc` has no operands. It admits only a Creature target and
    // invokes the inherited guarded pose selector with force=false. Zero
    // rewinds the consumed five-byte command; -1 clears the selected
    // decision neuron and removes/destroys the running Macro. Ordinary
    // nonzero success leaves the cursor advanced.
    objects::Object* target = object_context.target_object;
    if (target == nullptr || !runtime.is_creature_object(*target)) {
        // The only arm that reaches LAB_00420c6f, which sets the
        // iteration-completed flag before the common join: dispatch carries
        // straight on past the command.
        return MacroControlFlowResult::iteration_complete;
    }

    const std::uint32_t result =
        runtime.select_creature_target_pose_for_motion(*target, false);
    constexpr std::size_t kCaosCommandRecordBytes = sizeof(CaosToken) + 1;
    if (result == 0xffffffffU) {
        runtime.clear_creature_selected_decision_neuron(*target);
        remove_from_running_scheduler_and_destroy();
        // `this` may already be freed.
        return MacroControlFlowResult::macro_destroyed;
    }
    if (result == 0 && script_cursor_offset >= kCaosCommandRecordBytes) {
        script_cursor_offset -= kCaosCommandRecordBytes;
    }
    // Every arm with a real Creature target reaches the common join with the
    // iteration-completed flag still false, so `touc` yields to the scheduler
    // whether or not it rewound.  Reporting iteration_complete here re-fetched
    // the rewound command in the same pass and hung the world.
    return MacroControlFlowResult::cursor_changed;
}

bool Macro::execute_creature_motor_command(MacroCommand command,
                                           MacroRuntimeHost& runtime,
                                           MacroCommandHost& diagnostics) {
    if (command != MacroCommand::set_creature_action_activation_boost) {
        return false;
    }

    // Native `impt` consumes its one rvalue before checking the target.  The
    // target must be Creature family 0x04; the native store is byte-sized,
    // so the runtime adapter deliberately receives the parsed value and
    // performs the explicit low-byte conversion at the Creature boundary.
    const std::uint32_t value = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.set_creature_action_activation_boost(*target, value);
    }
    return true;
}

bool Macro::execute_creature_runtime_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command == MacroCommand::less_than_creature) {
        // Native `ltcy` consumes index, lower bound, and upper bound before
        // checking the target.  Only indices 0 through 7 address a Creature
        // involuntary-action record.  The x86 arithmetic is kept in explicit
        // 32-bit bit form so the adapter receives the same byte result as the
        // original command for ordinary signed bounds.
        const std::uint32_t action_index = parse_rvalue(runtime, diagnostics);
        const std::uint32_t lower_value = parse_rvalue(runtime, diagnostics);
        const std::uint32_t upper_value = parse_rvalue(runtime, diagnostics);
        objects::Object* target = object_context.target_object;
        if (target != nullptr && action_index < 8 &&
            runtime.is_creature_object(*target)) {
            const std::uint32_t span_bits =
                upper_value - lower_value + 1u;
            const auto span = static_cast<std::int32_t>(span_bits);
            const auto offset = static_cast<std::int32_t>(std::rand()) % span;
            const auto result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(lower_value) + offset);
            runtime.set_creature_involuntary_action_cooldown(
                *target, action_index, static_cast<std::uint8_t>(result));
        }
        return true;
    }

    if (command == MacroCommand::bacterium_update) {
        // Native `snez` has no operands and only updates the embedded
        // bacterium for a Creature target.  Bacterium owns the world policy;
        // the runtime adapter supplies that policy at the Creature boundary.
        objects::Object* target = object_context.target_object;
        if (target != nullptr && runtime.is_creature_object(*target)) {
            runtime.update_creature_bacterium(*target);
        }
        return true;
    }

    return false;
}

bool Macro::execute_compound_geometry_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::spot) {
        return false;
    }

    // Native `spot` consumes slot, left, top, right, and bottom before
    // checking the target.  The target must be CompoundObject family 0x03;
    // the native unsigned compare admits slots 0..5 only.  The four stores
    // are signed 32-bit WorldRect words at CompoundObject +0xcc.
    const std::uint32_t part_bounds_index = parse_rvalue(runtime, diagnostics);
    const std::int32_t min_x = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t min_y = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t max_x = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t max_y = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));

    objects::Object* target = object_context.target_object;
    if (target != nullptr && part_bounds_index < 6 &&
        runtime.is_compound_object(*target)) {
        runtime.set_compound_part_bounds(
            *target, part_bounds_index, min_x, min_y, max_x, max_y);
    }
    return true;
}

bool Macro::execute_compound_knob_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::knob) {
        return false;
    }

    // Native `knob` consumes the mapping index/function first and the
    // hotspot index second.  Both operands are consumed before target and
    // index admission; only indices 0..5 write the compound object's six
    // creature-event mappings.
    const std::uint32_t function_index = parse_rvalue(runtime, diagnostics);
    const std::uint32_t hotspot_index = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && function_index < 6u &&
        runtime.is_compound_object(*target)) {
        runtime.set_compound_knob_function(*target, function_index,
                                            hotspot_index);
    }
    return true;
}

bool Macro::execute_vehicle_command(MacroCommand command,
                                    MacroRuntimeHost& runtime,
                                    MacroObjectEventHost& object_events) {
    if (command == MacroCommand::vehicle_grab_passengers) {
        // Native `gpas` has no operands. It only acts when the current target
        // is a Vehicle; Vehicle owns the local-bounds predicate, creature
        // registry, and immediate event production.
        objects::Object* target = object_context.target_object;
        if (target != nullptr && runtime.is_vehicle_object(*target)) {
            runtime.grab_vehicle_passengers(*target);
        }
        return true;
    }

    if (command == MacroCommand::vehicle_set_passenger) {
        // Native `spas` parses two object rvalues and submits event 4 directly
        // to the shared immediate ring.  The bytes show no Vehicle check: the
        // object-event adapter, not Macro or a guessed Vehicle shim, owns the
        // ring/capacity policy.
        auto* source = reinterpret_cast<objects::Object*>(
            static_cast<std::uintptr_t>(parse_rvalue(runtime, object_events)));
        auto* target = reinterpret_cast<objects::Object*>(
            static_cast<std::uintptr_t>(parse_rvalue(runtime, object_events)));
        if (source != nullptr && target != nullptr) {
            object_events.queue_immediate_object_event(
                *source, *target, objects::ObjectEventId::event_4);
        }
        return true;
    }

    return false;
}

bool Macro::execute_part_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroCommandHost& diagnostics) {
    if (command != MacroCommand::part) {
        return false;
    }

    // Native `part` consumes one rvalue and stores its full 32-bit CAOS value
    // in the Macro selected-part slot.  It has no target guard and performs
    // no world-side effect; later object/image commands consume this state.
    selected_part_index = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    return true;
}

bool Macro::execute_instantiate_command(MacroCommand command) {
    if (command != MacroCommand::instantiate) {
        return false;
    }

    // Native `inst` has no operands or target-side effects.  Its complete
    // branch is the Macro +0x08 write recovered by Ghidra, which is the
    // capture-output state used by the Macro execution lifecycle.
    capture_output_enabled = true;
    return true;
}

MacroControlFlowResult Macro::execute_over_command(MacroCommand command) {
    if (command != MacroCommand::over) {
        return MacroControlFlowResult::execution_terminated;
    }

    // Native `over` has no operands.  A null target completes the command;
    // otherwise the Object virtual image-sequence query owns the per-part
    // animation state.  An active sequence rewinds the five-byte command so
    // the scheduler retries OVER on the next interpreter tick.
    objects::Object* target = object_context.target_object;
    if (target == nullptr ||
        target->image_sequence_is_empty(selected_part_index)) {
        return MacroControlFlowResult::iteration_complete;
    }

    constexpr std::size_t kCaosCommandRecordBytes = sizeof(CaosToken) + 1;
    if (script_cursor_offset < kCaosCommandRecordBytes) {
        // A well-formed command can never reach this path.  Keep the clean
        // model bounded if a malformed buffer is supplied by a caller.
        execution_terminated = true;
        return MacroControlFlowResult::execution_terminated;
    }
    script_cursor_offset -= kCaosCommandRecordBytes;

    // The rewind only means "retry on the next tick" if it reaches the
    // scheduler, which is what `wait` does with cursor_changed.  Reporting
    // iteration_complete instead sent the interpreter straight back to the
    // rewound OVER in the same pass, so a script waiting on an animation --
    // `loop,anim [..],over,...` -- spun inside one ExecuteInterpreter call
    // and never returned.
    return MacroControlFlowResult::cursor_changed;
}

bool Macro::execute_object_bounds_command(MacroCommand command,
                                          MacroRuntimeHost& runtime) {
    if (command != MacroCommand::slim) {
        return false;
    }

    // Native `slim` targets the current object, resets its bounds mode to
    // DEFAULT_WORLD, and immediately recomputes movement bounds.  Both
    // operations belong to the Object/world adapter; Macro contributes no
    // renderer, map, or container implementation here.
    objects::Object* target = object_context.target_object;
    if (target != nullptr) {
        runtime.set_object_bounds_mode(*target, 0u);
        runtime.update_object_movement_bounds(*target);
    }
    return true;
}

bool Macro::execute_base_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroCommandHost& diagnostics) {
    if (command != MacroCommand::base) {
        return false;
    }

    // Native `base` parses its one rvalue before checking the current target.
    // The image argument is truncated to one byte by the virtual-call ABI;
    // the selected part index is passed unchanged.  The target may be any
    // Object: the vtable decides whether the concrete object owns an image.
    const std::uint8_t image_index = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    objects::Object* target = object_context.target_object;
    if (target != nullptr) {
        runtime.set_object_image_index(*target, image_index,
                                       selected_part_index);
    }
    return true;
}

bool Macro::execute_behavior_command(MacroCommand command,
                                     MacroRuntimeHost& runtime,
                                     MacroCommandHost& diagnostics) {
    if (command != MacroCommand::behavior) {
        return false;
    }

    // Native BHVR parses both operands before checking the current target.
    // The first selects one of five packed SimpleObject records; the second
    // contributes only its low byte to the interaction-event mask.
    const std::uint32_t behavior_index = parse_rvalue(runtime, diagnostics);
    const std::uint32_t interaction_flags = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_simple_object(*target)) {
        runtime.configure_simple_object_behavior(
            *target, behavior_index, interaction_flags);
    }
    return true;
}

MacroControlFlowResult Macro::execute_pose_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::pose) {
        return MacroControlFlowResult::execution_terminated;
    }

    // Native `pose` parses one rvalue before checking the target.  The Object
    // virtual owns image selection and redraw; its false result is a retry
    // signal, so restore the consumed four-byte token plus separator exactly
    // as the native interpreter does.
    const std::uint32_t relative_index = parse_rvalue(runtime, diagnostics);
    objects::Object* target = object_context.target_object;
    if (target != nullptr &&
        !runtime.set_object_relative_image_index(
            *target, relative_index, selected_part_index)) {
        constexpr std::size_t kCaosCommandRecordBytes = sizeof(CaosToken) + 1;
        if (script_cursor_offset >= kCaosCommandRecordBytes) {
            script_cursor_offset -= kCaosCommandRecordBytes;
        } else {
            // A well-formed command cannot reach this path.  Keep malformed
            // input bounded instead of letting the unsigned cursor underflow.
            execution_terminated = true;
        }
    }
    // Every native arm reaches the LAB_00420c74 join without setting the
    // iteration-completed flag, so `pose` yields to the scheduler.  Retrying a
    // rewound `pose` in the same pass could never make progress.
    return execution_terminated
               ? MacroControlFlowResult::execution_terminated
               : MacroControlFlowResult::cursor_changed;
}

bool Macro::execute_preload_image_sequence_command(
    MacroCommand command, MacroRuntimeHost& runtime) {
    if (command != MacroCommand::preload_image_sequence) {
        return false;
    }

    // Native `prld` passes the current script cursor and selected part to the
    // Object vtable slot 39.  A null target still consumes the bracketed
    // sequence, but does not invoke the image/cache owner.
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset >= readable_capacity) {
        execution_terminated = true;
        return true;
    }

    objects::Object* target = object_context.target_object;
    if (target != nullptr) {
        char* sequence_cursor = script_buffer.data() + script_cursor_offset;
        char* next_cursor = runtime.preload_object_image_sequence(
            *target, sequence_cursor, selected_part_index);
        if (next_cursor >= script_buffer.data() &&
            next_cursor <= script_buffer.data() + script_buffer.size()) {
            script_cursor_offset = static_cast<std::size_t>(
                next_cursor - script_buffer.data());
        } else {
            execution_terminated = true;
        }
        return true;
    }

    while (script_cursor_offset < readable_capacity &&
           script_buffer[script_cursor_offset] != ']') {
        ++script_cursor_offset;
    }
    if (script_cursor_offset < readable_capacity) {
        script_cursor_offset += 2;
    } else {
        execution_terminated = true;
    }
    return true;
}

bool Macro::execute_edit_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroSystemHost& system) {
    if (command != MacroCommand::edit) {
        return false;
    }

    // Native `edit` has no operands.  It publishes the current target only
    // when non-null, then calls USER32 SetForegroundWindow on the main frame
    // window.  The edit-object lifetime and the Windows handle are separate
    // application/platform boundaries; Macro preserves their native order.
    objects::Object* target = object_context.target_object;
    if (target != nullptr) {
        runtime.set_edit_object(target);
        system.bring_main_window_to_front();
    }
    return true;
}

bool Macro::execute_version_command(MacroCommand command,
                                    MacroRuntimeHost& runtime,
                                    MacroSystemHost& system) {
    if (command != MacroCommand::version) {
        return false;
    }

    const std::uint32_t requested_version = parse_rvalue(runtime, system);
    const std::uint32_t supported_version = runtime.language_version();
    if (supported_version < requested_version) {
        // Native `vrsn` leaves the interpreter through its release path after
        // the application diagnostic.  Marking the Macro terminated preserves
        // that scheduler decision in the clean interpreter; formatting,
        // Windows/MFC, and timer ownership remain in the host adapter.
        system.report_unsupported_language_version(
            *this, supported_version, requested_version);
        execution_terminated = true;
    }
    return true;
}

bool Macro::execute_room_command(MacroCommand command,
                                 MacroRuntimeHost& runtime,
                                 MacroSystemHost& system) {
    if (command != MacroCommand::room) {
        return false;
    }

    // Native `room` always consumes the index and four bounds first.  The
    // fifth operand is consumed only when the index names one of the forty
    // fixed MapData records; an out-of-range index takes the common discard
    // path without consuming room_type.
    const std::uint32_t room_index = parse_rvalue(runtime, system);
    const int left = static_cast<int>(parse_rvalue(runtime, system));
    const int top = static_cast<int>(parse_rvalue(runtime, system));
    const int right = static_cast<int>(parse_rvalue(runtime, system));
    const int bottom = static_cast<int>(parse_rvalue(runtime, system));
    if (room_index > 0x27u) {
        return true;
    }

    const std::uint32_t room_type = parse_rvalue(runtime, system);
    system.set_room_definition(room_index, left, top, right, bottom,
                               room_type);
    return true;
}

bool Macro::execute_mate_command(MacroCommand command,
                                 MacroRuntimeHost& runtime) {
    if (command != MacroCommand::mate &&
        command != MacroCommand::hidden_fertilize) {
        return false;
    }

    // Native `mate` and its hidden `f**k` synonym have no operands.  Both
    // admit only a Creature-family target and leave motion-link validation,
    // fertility state, probability, genome file creation, and source-gamete
    // clearing to Creature::process_insemination.  `f**k` is a real native
    // interpreter token; censor_script_profanity() only rewrites script text
    // for diagnostics and must not be treated as its semantic implementation.
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        runtime.process_creature_insemination(*target);
    }
    return true;
}

bool Macro::execute_timer_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics) {
    if (command != MacroCommand::tick) {
        return false;
    }

    // Native `tick` parses one rvalue, then writes that exact 32-bit value to
    // both Object timer fields when a target exists.  The Object boundary
    // preserves the signed storage representation without making Macro know
    // the native offsets or duplicating timer state.
    const auto timer_value = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    objects::Object* target = object_context.target_object;
    if (target != nullptr) {
        target->set_timer_for_script(timer_value);
    }
    return true;
}

bool Macro::execute_animation_command(MacroCommand command) {
    if (command != MacroCommand::anim) {
        return false;
    }

    // Native `anim` gives the current target the remaining bracketed image
    // sequence and adopts the returned script cursor.  A null target skips
    // to the closing bracket, preserving the command's parser consumption
    // without manufacturing an image/rendering implementation in Macro.
    objects::Object* target = object_context.target_object;
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset >= readable_capacity) {
        execution_terminated = true;
        return true;
    }

    char* sequence_cursor = script_buffer.data() + script_cursor_offset;
    if (target != nullptr) {
        char* next_cursor = target->parse_image_sequence(
            sequence_cursor, selected_part_index);
        if (next_cursor >= script_buffer.data() &&
            next_cursor <= script_buffer.data() + script_buffer.size()) {
            script_cursor_offset = static_cast<std::size_t>(
                next_cursor - script_buffer.data());
        } else {
            execution_terminated = true;
        }
        return true;
    }

    while (script_cursor_offset < readable_capacity &&
           script_buffer[script_cursor_offset] != ']') {
        ++script_cursor_offset;
    }
    if (script_cursor_offset < readable_capacity) {
        script_cursor_offset += 2;
    } else {
        execution_terminated = true;
    }
    return true;
}

bool Macro::execute_target_command(MacroCommand command,
                                   MacroRuntimeHost& runtime,
                                   MacroCommandHost& diagnostics) {
    if (command != MacroCommand::target) {
        return false;
    }

    // Native `targ` consumes one rvalue.  A non-null result replaces the
    // current target; zero is deliberately a no-op rather than a clear.  The
    // rvalue parser owns CAOS pointer decoding, so this handler only performs
    // the recovered object-context assignment.
    const auto target_value = parse_rvalue(runtime, diagnostics);
    if (target_value != 0) {
        object_context.target_object = reinterpret_cast<objects::Object *>(
            static_cast<std::uintptr_t>(target_value));
    }
    return true;
}

bool Macro::execute_debug_command(MacroCommand command, MacroDebugHost& host,
                                  MacroRuntimeHost& runtime) {
    if (command == MacroCommand::vocabulary) {
        objects::Object* target = object_context.target_object;
        if (target != nullptr && runtime.is_creature_object(*target)) {
            runtime.initialize_creature_default_vocabulary(*target);
        }
        return true;
    }
    if (command == MacroCommand::dbug) {
        // Native `dbug` parses one rvalue, formats/logs "Debug: %d\n" through
        // the debug-console boundary, and unconditionally sets Macro +0x08
        // (capture_output_enabled) afterward.
        host.log_debug_command_value(*this, parse_rvalue(runtime, host));
        capture_output_enabled = true;
        return true;
    }
    if (command == MacroCommand::debug_message) {
        // Native `dbgm` calls ReadBracketedTextArgument with an 0x80-byte
        // temporary, then sends `Debug Message: %s\\n` to DebugLog(category=2)
        // when the debug-console boundary is present.  The temporary buffer
        // and formatting belong to the adapter; Macro owns only the parser
        // cursor and command routing.
        std::array<char, 0x80> text_buffer{};
        read_bracketed_text_argument(text_buffer.data(), text_buffer.size());
        host.log_debug_message(*this, std::string_view(text_buffer.data()));
        return true;
    }
    if (command == MacroCommand::debug_value) {
        // Native `dbgv` has the same one-rvalue/debug-console shape but uses
        // the distinct `Debug Value: %d\\n` message and does not set the
        // dbgm capture flag.
        host.log_debug_value(*this, parse_rvalue(runtime, host));
        return true;
    }
    return false;
}

bool Macro::execute_remove_event_command(MacroCommand command,
                                         MacroRuntimeHost& runtime,
                                         MacroCommandHost& diagnostics) {
    if (command != MacroCommand::remove_event) {
        return false;
    }

    // Native `rmev` consumes one Object rvalue and calls the EventBar display
    // list removal routine with its auxiliary-state flag set.  A null object
    // has no display-list match and therefore has no effect.
    auto* object = reinterpret_cast<objects::Object*>(static_cast<std::uintptr_t>(
        parse_rvalue(runtime, diagnostics)));
    if (object != nullptr) {
        runtime.remove_object_from_event_bar(*object, true);
    }
    return true;
}

bool Macro::execute_event_command(MacroCommand command,
                                  MacroRuntimeHost& runtime,
                                  MacroCommandHost& diagnostics) {
    if (command != MacroCommand::event) {
        return false;
    }

    // Native `evnt` consumes one Object rvalue and passes the raw pointer to
    // AddObjectToEventBarDisplayList.  That routine deliberately accepts the
    // null result as a list entry, so do not add the null guard used by `rmev`.
    auto* object = reinterpret_cast<objects::Object*>(static_cast<std::uintptr_t>(
        parse_rvalue(runtime, diagnostics)));
    runtime.add_object_to_event_bar(object);
    return true;
}

MacroControlFlowResult Macro::execute_script_command(
    MacroCommand command, ScriptDefinitionInstallHost& scripts,
    MacroRuntimeHost& runtime, MacroCommandHost& diagnostics) {
    if (command != MacroCommand::script) {
        return MacroControlFlowResult::not_control_flow;
    }

    // Native `scrp` parses family, genus, species, and event in that order,
    // then hands the script pointer at the current cursor to the classifier
    // registry with replacement confirmation enabled.  The string view is
    // bounded by the same readable-capacity rule used by the Macro parser;
    // the registry owns copying the text into its fixed-capacity table.
    const auto family = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    const auto genus = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    const auto species = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    const auto event = static_cast<ScriptEvent>(
        parse_rvalue(runtime, diagnostics));

    const std::size_t readable_capacity = std::min<std::size_t>(
        script_capacity_bytes, script_buffer.size());
    const std::size_t cursor = std::min(script_cursor_offset,
                                        readable_capacity);
    install_script_text_for_classifier(
        ScriptClassifier{event, species, genus, family},
        std::string_view(script_buffer).substr(cursor), true, scripts);

    // Native `scrp` removes this macro from the running scheduler and returns
    // from ExecuteInterpreter immediately.  The eventual dispatcher owns
    // the physical destruction/scheduler operation represented by this
    // control-flow result; this handler must not fall through to ordinary
    // command finalization.
    return MacroControlFlowResult::interpreter_returned;
}

MacroControlFlowResult Macro::execute_script_extended_command(
    MacroCommand command, MacroSystemHost& system, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::script_extended) {
        return MacroControlFlowResult::not_control_flow;
    }

    // Native `scrx` parses family, genus, species, and event in that order,
    // removes the exact classifier record, and returns directly from the
    // interpreter rather than joining ordinary command finalization. Keep
    // that early-return result explicit for the eventual dispatcher.
    const auto family = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    const auto genus = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    const auto species = static_cast<std::uint8_t>(
        parse_rvalue(runtime, diagnostics));
    const auto event = static_cast<ScriptEvent>(
        parse_rvalue(runtime, diagnostics));
    system.remove_script_definition(
        ScriptClassifier{event, species, genus, family});
    return MacroControlFlowResult::interpreter_returned;
}

MacroControlFlowResult Macro::execute_pointer_command(
    MacroCommand command, MacroRuntimeHost& runtime) {
    if (command != MacroCommand::pointer) {
        return MacroControlFlowResult::execution_terminated;
    }

    // Native `poin` has no operands.  It admits only a Creature target and
    // calls Creature's inherited Skeleton::SelectTargetPoseForMotionGuarded
    // with force=1.  A zero result rewinds the already-consumed five-byte
    // command; success and the native -1/no-motion result leave the cursor
    // at its current position.
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_creature_object(*target)) {
        const std::uint32_t result =
            runtime.select_creature_target_pose_for_motion(*target, true);
        constexpr std::size_t kCaosCommandRecordBytes =
            sizeof(CaosToken) + 1;
        if (result == 0 && script_cursor_offset >= kCaosCommandRecordBytes) {
            script_cursor_offset -= kCaosCommandRecordBytes;
        }
    }
    // As with `pose` and `touc`, every native arm joins at LAB_00420c74 with
    // the iteration-completed flag clear, so `poin` yields.
    return MacroControlFlowResult::cursor_changed;
}

MacroControlFlowResult Macro::execute_approach_command(
    MacroCommand command, MacroRuntimeHost& runtime) {
    if (command != MacroCommand::approach &&
        command != MacroCommand::uppercase_approach) {
        return MacroControlFlowResult::execution_terminated;
    }

    constexpr std::size_t kCaosCommandRecordBytes = sizeof(CaosToken) + 1;
    const auto rewrite_consumed_command =
        [this, kCaosCommandRecordBytes](CaosToken replacement) {
        if (script_cursor_offset < sizeof(CaosToken) + 1 ||
            script_cursor_offset - (sizeof(CaosToken) + 1) +
                    sizeof(CaosToken) >
                std::min<std::size_t>(script_capacity_bytes,
                                      script_buffer.size())) {
            execution_terminated = true;
            return;
        }
        std::memcpy(script_buffer.data() + script_cursor_offset -
                        kCaosCommandRecordBytes,
                    &replacement, sizeof(replacement));
        };

    objects::Object* target = object_context.target_object;
    if (command == MacroCommand::approach) {
        if (target != nullptr && runtime.is_creature_object(*target)) {
            runtime.select_creature_walk_gait(*target);
        }
        // Native `appr` converts the consumed command to `APPR` and rewinds
        // to it.  The next interpreter tick therefore evaluates the
        // approach continuation against the current motion target.
        rewrite_consumed_command(
            static_cast<CaosToken>(MacroCommand::uppercase_approach));
        if (execution_terminated) {
            return MacroControlFlowResult::execution_terminated;
        }
        script_cursor_offset -= kCaosCommandRecordBytes;
        // Rewound: the continuation belongs to the next scheduler tick, so
        // this yields rather than re-dispatching APPR in the same pass.
        return MacroControlFlowResult::cursor_changed;
    }

    if (target == nullptr || !runtime.is_creature_object(*target)) {
        // A non-Creature target takes the native fallback: lower-case `appr`
        // is left for the ordinary gait-selection branch on the next pass.
        rewrite_consumed_command(static_cast<CaosToken>(MacroCommand::approach));
        // Not rewound: execution carries on past the rewritten command.
        return execution_terminated
                   ? MacroControlFlowResult::execution_terminated
                   : MacroControlFlowResult::iteration_complete;
    }

    if (runtime.creature_approach_is_ready(*target)) {
        rewrite_consumed_command(static_cast<CaosToken>(MacroCommand::approach));
        runtime.reset_creature_animation_sequence(*target);
        return execution_terminated
                   ? MacroControlFlowResult::execution_terminated
                   : MacroControlFlowResult::iteration_complete;
    }

    // The native far-from-target path retries APPR on the next scheduler
    // tick without changing the command bytes -- which only happens if the
    // rewind yields, the same reason `over` must.
    if (script_cursor_offset < kCaosCommandRecordBytes) {
        execution_terminated = true;
        return MacroControlFlowResult::execution_terminated;
    }
    script_cursor_offset -= kCaosCommandRecordBytes;
    return MacroControlFlowResult::cursor_changed;
}

bool Macro::execute_vehicle_bounds_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::cabinet) {
        return false;
    }

    // Native `cabn` consumes all four signed rectangle words before checking
    // the target.  The family-3 target is the Vehicle object whose exact
    // field layout places this rectangle at +0x154.
    const std::int32_t min_x = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t min_y = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t max_x = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));
    const std::int32_t max_y = static_cast<std::int32_t>(
        parse_rvalue(runtime, diagnostics));

    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_vehicle_object(*target)) {
        runtime.set_vehicle_creature_event_bounds(
            *target, min_x, min_y, max_x, max_y);
    }
    return true;
}

void Macro::execute_dde_command(MacroDdeHost& host, MacroRuntimeHost& runtime) {
    const std::size_t command_start = script_cursor_offset;
    if (command_start + sizeof(CaosToken) + 1 >
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size())) {
        execution_terminated = true;
        report_syntax_error(host, "'dde:' command");
        return;
    }

    const CaosToken command = read_next_token();
    switch (command) {
    case kLive:
        host.adjust_score(DdeScoreCounter::living_norns, 1);
        host.notify_score_changed();
        return;
    case kDied:
        host.adjust_score(DdeScoreCounter::dead_norns, 1);
        host.adjust_score(DdeScoreCounter::living_norns, -1);
        host.notify_score_changed();
        return;
    case kNege:
        host.adjust_score(DdeScoreCounter::natural_eggs_laid, 1);
        host.notify_score_changed();
        return;
    case kHatc:
        // `hatc` counts a hatchery egg rather than a natural one; it was
        // missing from the port's subcommand set entirely.
        host.adjust_score(DdeScoreCounter::hatchery_eggs_used, 1);
        host.notify_score_changed();
        return;
    case kPanc:
        host.pan_view_to_selected_creature();
        return;
    case kGetB: {
        const CaosToken subcommand = read_next_token();
        std::optional<DdeGetBQuery> query;
        switch (subcommand) {
        case kMonk: query = DdeGetBQuery::genome_source; break;
        case kData: query = DdeGetBQuery::creature_history; break;
        case kOvvd: query = DdeGetBQuery::selected_creature_status; break;
        case kCnam: query = DdeGetBQuery::creature_name; break;
        case kCtim: query = DdeGetBQuery::creature_age; break;
        default: return;
        }
        // Every native writer appends its own trailing separator -- the
        // `getb monk` loop does it with strcat_s(buf, "|") per record -- and
        // ExecuteToOutputBuffer then overwrites that last byte with the NUL.
        // Assigning the value raw made the overwrite eat a real character
        // instead, so every getb reply came back one character short.
        const std::optional<std::string> result = host.query_getb(*this, *query);
        if (result.has_value()) {
            output_text.clear();
            append_pipe_field(output_text, *result);
        } else {
            output_text.clear();
        }
        return;
    }
    case kPutB: {
        const std::string text = read_bracketed_text_argument();
        const CaosToken subcommand = read_next_token();
        if (subcommand == kCnam) {
            host.update_putb(*this, DdePutBCommand::creature_name, text);
        } else if (subcommand == kData) {
            host.update_putb(*this, DdePutBCommand::creature_history, text);
        }
        return;
    }
    case kWord: {
        // Trailing separator per the native writers; see kGetB.
        const std::string rendered = host.render_learned_words(*this);
        output_text.clear();
        if (!rendered.empty()) {
            append_pipe_field(output_text, rendered);
        }
        return;
    }
    case kLobe:
        // Binary, and it carries its own trailing 0xff for the terminator to
        // overwrite -- no separator is appended here.  See render_brain_lobe.
        output_text = host.render_brain_lobe(*this);
        return;
    case kGene:
        // Fourteen "%d|" fields; the final bar is already there and is the
        // byte the reply terminator overwrites, so nothing is appended.
        output_text = host.render_gene_counts(*this);
        return;
    case kGids: {
        // ExecuteDDECommand's `gids` branch: each granularity filters the
        // script table on a classifier prefix and sets a bit for the next
        // field down, then prints the set bits in ascending order, each
        // followed by a space.  The Injector uses this to enumerate what is
        // installed.
        //
        // The operands after the granularity are rvalues parsed from the
        // script.  Reading only the granularity token left `gids fmly 4`'s 4
        // to be mis-read as the next command, so the cursor desynchronised as
        // well as the answer being empty.
        const CaosToken granularity = read_next_token();
        if (granularity != kRoot && granularity != kFmly &&
            granularity != kGnus && granularity != kSpcs) {
            return;
        }
        std::uint32_t family = 0;
        std::uint32_t genus = 0;
        std::uint32_t species = 0;
        if (granularity != kRoot) {
            family = parse_rvalue(runtime, host);
        }
        if (granularity == kGnus || granularity == kSpcs) {
            genus = parse_rvalue(runtime, host);
        }
        if (granularity == kSpcs) {
            species = parse_rvalue(runtime, host);
        }

        std::array<bool, 256> present{};
        for (std::size_t index = 0; index < g_script_definition_count;
             ++index) {
            const ScriptClassifier& classifier =
                g_script_definition_entries[index].classifier_event;
            if (granularity == kRoot) {
                present[classifier.family] = true;
            } else if (granularity == kFmly) {
                if (classifier.family == family) {
                    present[classifier.genus] = true;
                }
            } else if (granularity == kGnus) {
                if (classifier.family == family &&
                    classifier.genus == genus) {
                    present[classifier.species] = true;
                }
            } else if (classifier.family == family &&
                       classifier.genus == genus &&
                       classifier.species == species) {
                present[static_cast<std::uint8_t>(classifier.event)] = true;
            }
        }

        std::string identifiers;
        for (std::size_t identifier = 0; identifier < present.size();
             ++identifier) {
            if (present[identifier]) {
                identifiers += std::to_string(identifier);
                identifiers += ' ';
            }
        }
        append_pipe_field(output_text, identifiers);
        return;
    }
    case kCell: {
        const std::uint32_t cell = parse_rvalue(runtime, host);
        const std::uint32_t variable = parse_rvalue(runtime, host);
        const std::uint32_t field = parse_rvalue(runtime, host);
        append_output(host.render_cell_values(*this, cell, variable, field));
        return;
    }
    case kScrp: {
        const std::uint32_t family = parse_rvalue(runtime, host);
        const std::uint32_t genus = parse_rvalue(runtime, host);
        const std::uint32_t species = parse_rvalue(runtime, host);
        const std::uint32_t event = parse_rvalue(runtime, host);
        // The script table is the C1 layer's own, so the lookup belongs here
        // rather than behind a platform binding that had nothing platform
        // about it.  A miss yields a bare separator, which the reply
        // terminator turns into an empty payload.
        for (std::size_t index = 0; index < g_script_definition_count;
             ++index) {
            const ScriptDefinitionEntry& entry =
                g_script_definition_entries[index];
            if (entry.classifier_event.family == family &&
                entry.classifier_event.genus == genus &&
                entry.classifier_event.species == species &&
                static_cast<std::uint32_t>(entry.classifier_event.event) ==
                    event) {
                append_pipe_field(output_text, entry.script_text);
                return;
            }
        }
        output_text = "|";
        return;
    }
    case kPutV:
        // The native formats with wsprintfA("%d|", value): the CAOS value is
        // 32 bits but it prints SIGNED, so a negated variable reads back as
        // -5 rather than 4294967291.
        append_pipe_field(output_text,
                          std::to_string(static_cast<std::int32_t>(
                              parse_rvalue(runtime, host))));
        return;
    case kPutS:
        append_pipe_field(output_text, read_bracketed_text_argument());
        return;
    case kPict: {
        std::string output_path;
        if (host.capture_picture(*this, output_path)) {
            output_text = output_path;
        } else {
            output_text.clear();
        }
        return;
    }
    default:
        // The native dispatcher leaves unknown DDE subcommands without an
        // output mutation.  Truncated commands are the syntax-error case.
        return;
    }
}

void Macro::execute_blackboard_caos_command(MacroBlackboardHost& host,
                                            MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset + sizeof(CaosToken) + 1 > readable_capacity) {
        execution_terminated = true;
        report_syntax_error(host, "'bbd:' command");
        return;
    }

    const CaosToken command = read_next_token();
    switch (command) {
    case kBbdEmit: {
        const bool spoken = parse_rvalue(runtime, host) != 0;
        if (host.has_blackboard_target(*this)) {
            const std::string word_text = host.current_word_text(*this);
            if (!word_text.empty()) {
                host.announce_blackboard_word(*this, spoken,
                                               host.current_word_index(*this),
                                               word_text);
            }
        }
        return;
    }
    case kBbdWord: {
        const std::uint32_t word_index = parse_rvalue(runtime, host);
        const std::uint32_t word_value = parse_rvalue(runtime, host);
        const std::string word_text = read_bracketed_text_argument();
        if (host.has_blackboard_target(*this) && word_index < 0x10u) {
            host.write_blackboard_word(
                *this, word_index, word_value,
                word_text.substr(0, std::min<std::size_t>(0x0a, word_text.size())));
        }
        return;
    }
    case kBbdEdit:
        host.set_blackboard_edit_mode(*this, parse_rvalue(runtime, host));
        return;
    case kBbdShow:
        host.redraw_blackboard(*this, parse_rvalue(runtime, host));
        return;
    default:
        report_syntax_error(host, "'bbd:' command");
        return;
    }
}

void Macro::execute_new_command(MacroNewObjectHost& host,
                                MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset + sizeof(CaosToken) + 1 > readable_capacity) {
        execution_terminated = true;
        report_syntax_error(host, "'new:' command");
        return;
    }

    const CaosToken command = read_next_token();
    switch (command) {
    case kNewCallButton: {
        const std::uint32_t object_file_id = read_next_token();
        const std::uint32_t first_value = parse_rvalue(runtime, host);
        const std::uint32_t header_record_index = parse_rvalue(runtime, host);
        const std::uint32_t render_plane = parse_rvalue(runtime, host);
        object_context.target_object = host.create_call_button(
            *this, NewCallButtonRequest{object_file_id, header_record_index,
                                        first_value, render_plane});
        return;
    }
    case kNewGene: {
        const std::uint32_t first_parent = parse_rvalue(runtime, host);
        const std::uint32_t second_parent = parse_rvalue(runtime, host);
        const CaosToken destination_token = read_next_token();
        const std::uint32_t genome_file =
            host.generate_offspring_genome_file(*this, first_parent,
                                                second_parent);
        assign_lvalue(runtime, host, destination_token, genome_file);
        return;
    }
    case kNewVehicle: {
        const CaosToken sprite_file_id = read_next_token();
        const std::uint32_t image_count = parse_rvalue(runtime, host);
        const std::uint32_t header_record_index = parse_rvalue(runtime, host);
        object_context.target_object = host.create_vehicle(
            *this, NewVehicleRequest{sprite_file_id, header_record_index,
                                     image_count});
        return;
    }
    case kNewScenery: {
        const CaosToken construction_type = read_next_token();
        const std::uint32_t image_count = parse_rvalue(runtime, host);
        const std::uint32_t image_index = parse_rvalue(runtime, host);
        const std::uint32_t render_plane = parse_rvalue(runtime, host);
        object_context.target_object = host.create_scenery(
            *this, NewSceneryRequest{construction_type, image_count,
                                     image_index, render_plane});
        return;
    }
    case kNewCreature: {
        const std::string genome_source_filename = host.parse_rvalue_text(*this);
        const auto construction_sex = static_cast<
            creatures1::creatures::CreatureConstructionSex>(
            parse_rvalue(runtime, host));
        object_context.target_object = host.create_creature(
            *this, NewCreatureRequest{genome_source_filename, construction_sex});
        return;
    }
    case kNewBlackboard: {
        const CaosToken object_type = read_next_token();
        const std::uint32_t image_count = parse_rvalue(runtime, host);
        const std::uint32_t header_record_index = parse_rvalue(runtime, host);
        const std::uint32_t fill_palette_index = parse_rvalue(runtime, host);
        const std::uint32_t text_render_config_1 = parse_rvalue(runtime, host);
        const std::uint32_t text_render_config_2 = parse_rvalue(runtime, host);
        const std::uint32_t tile_x = parse_rvalue(runtime, host);
        const std::uint32_t tile_y = parse_rvalue(runtime, host);
        object_context.target_object = host.create_blackboard(
            *this, NewBlackboardRequest{object_type, header_record_index,
                                        image_count, fill_palette_index,
                                        text_render_config_1,
                                        text_render_config_2, tile_x, tile_y});
        return;
    }
    case kNewPart: {
        const std::uint32_t part_index = parse_rvalue(runtime, host);
        const std::uint32_t local_x_offset = parse_rvalue(runtime, host);
        const std::uint32_t local_y_offset = parse_rvalue(runtime, host);
        const std::uint32_t image_index = parse_rvalue(runtime, host);
        const std::uint32_t render_plane = parse_rvalue(runtime, host);
        host.create_part(*this, NewPartRequest{part_index, local_x_offset,
                                               local_y_offset, image_index,
                                               render_plane});
        selected_part_index = static_cast<std::int32_t>(part_index);
        return;
    }
    case kNewSimpleObject: {
        const std::uint32_t object_file_id = read_next_token();
        const std::uint32_t image_count = parse_rvalue(runtime, host);
        const std::uint32_t header_record_index = parse_rvalue(runtime, host);
        const std::uint32_t render_plane = parse_rvalue(runtime, host);
        const bool cache_protected = parse_rvalue(runtime, host) != 0;
        object_context.target_object = host.create_simple_object(
            *this, NewSimpleObjectRequest{object_file_id, header_record_index,
                                          image_count, cache_protected,
                                          render_plane});
        return;
    }
    case kNewCompoundObject: {
        const CaosToken sprite_file_id = read_next_token();
        const std::uint32_t image_count = parse_rvalue(runtime, host);
        const std::uint32_t header_record_index = parse_rvalue(runtime, host);
        const bool cache_protected = parse_rvalue(runtime, host) != 0;
        object_context.target_object = host.create_compound_object(
            *this, NewCompoundObjectRequest{sprite_file_id, header_record_index,
                                            image_count, cache_protected});
        return;
    }
    case kNewLift: {
        const auto object_file_id = static_cast<std::int32_t>(read_next_token());
        const std::uint32_t image_count = parse_rvalue(runtime, host);
        const std::uint32_t header_record_index = parse_rvalue(runtime, host);
        object_context.target_object = host.create_lift(
            *this, NewLiftRequest{object_file_id, header_record_index,
                                  image_count});
        return;
    }
    default:
        report_syntax_error(host, "'new:' command");
        return;
    }
}

bool Macro::evaluate_comparison(MacroComparisonOperator operation,
                                std::uint32_t left,
                                std::uint32_t right) {
    switch (operation) {
    case MacroComparisonOperator::equal:
        return left == right;
    case MacroComparisonOperator::not_equal:
        return left != right;
    case MacroComparisonOperator::greater_than:
        // Native CAOS uses signed 32-bit conditional jumps for relational
        // operators.  Keep the stored values bit-exact, but compare their
        // signed interpretation here; treating 0xffffffff as an unsigned
        // maximum changes the result of `gt`/`lt` and their inclusive forms.
        return static_cast<std::int32_t>(left) >
               static_cast<std::int32_t>(right);
    case MacroComparisonOperator::less_than:
        return static_cast<std::int32_t>(left) <
               static_cast<std::int32_t>(right);
    case MacroComparisonOperator::greater_or_equal:
        return static_cast<std::int32_t>(left) >=
               static_cast<std::int32_t>(right);
    case MacroComparisonOperator::less_or_equal:
        return static_cast<std::int32_t>(left) <=
               static_cast<std::int32_t>(right);
    case MacroComparisonOperator::bit_set:
        return (left & right) != 0;
    case MacroComparisonOperator::bit_clear:
        return (left & right) == 0;
    }
    return false;
}

std::optional<bool> Macro::parse_comparison_condition(
    MacroRuntimeHost& runtime, MacroCommandHost& diagnostics) {
    // Native DOIF/UNTL parse the left rvalue, read a two-byte operator, skip
    // its one-byte separator, then parse the right rvalue.  The operator is
    // not a five-byte CAOS command token, so using read_next_token() here
    // would shift the cursor and corrupt every following operand.
    const std::uint32_t left = parse_rvalue(runtime, diagnostics);
    if (execution_terminated) {
        return std::nullopt;
    }

    const std::size_t readable_capacity = std::min<std::size_t>(
        script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset + sizeof(std::uint16_t) + 1 >
        readable_capacity) {
        execution_terminated = true;
        report_syntax_error(diagnostics, "comparison operator");
        return std::nullopt;
    }

    std::uint16_t encoded_operator = 0;
    std::memcpy(&encoded_operator, script_buffer.data() + script_cursor_offset,
                sizeof(encoded_operator));
    script_cursor_offset += sizeof(encoded_operator) + 1;

    const auto operation = static_cast<MacroComparisonOperator>(
        encoded_operator);
    switch (operation) {
    case MacroComparisonOperator::equal:
    case MacroComparisonOperator::not_equal:
    case MacroComparisonOperator::greater_than:
    case MacroComparisonOperator::less_than:
    case MacroComparisonOperator::greater_or_equal:
    case MacroComparisonOperator::less_or_equal:
    case MacroComparisonOperator::bit_set:
    case MacroComparisonOperator::bit_clear:
        break;
    default:
        execution_terminated = true;
        report_syntax_error(diagnostics, "comparison operator");
        return std::nullopt;
    }

    const std::uint32_t right = parse_rvalue(runtime, diagnostics);
    if (execution_terminated) {
        return std::nullopt;
    }
    return evaluate_comparison(operation, left, right);
}

MacroControlFlowResult Macro::begin_loop() {
    if (caos_value_stack_cursor_index >= caos_value_stack.size()) {
        execution_terminated = true;
        return MacroControlFlowResult::execution_terminated;
    }

    // `loop` stores the cursor immediately after its own five-byte token.
    caos_value_stack[caos_value_stack_cursor_index++] =
        static_cast<std::uint32_t>(script_cursor_offset);
    return MacroControlFlowResult::iteration_complete;
}

MacroControlFlowResult Macro::begin_counted_repeat(std::uint32_t count) {
    if (caos_value_stack_cursor_index + 2 > caos_value_stack.size()) {
        execution_terminated = true;
        return MacroControlFlowResult::execution_terminated;
    }

    // `reps` pushes the continuation cursor followed by the remaining count.
    caos_value_stack[caos_value_stack_cursor_index++] =
        static_cast<std::uint32_t>(script_cursor_offset);
    caos_value_stack[caos_value_stack_cursor_index++] = count;
    return MacroControlFlowResult::iteration_complete;
}

MacroControlFlowResult Macro::execute_counted_repeat_command(
    MacroRuntimeHost& runtime, MacroCommandHost& diagnostics) {
    // Native `reps` consumes its count as one CAOS rvalue before pushing the
    // post-token cursor and the remaining count onto the VM value stack.
    const std::uint32_t count = parse_rvalue(runtime, diagnostics);
    if (execution_terminated) {
        return MacroControlFlowResult::execution_terminated;
    }
    return begin_counted_repeat(count);
}

MacroControlFlowResult Macro::execute_kill_command(
    MacroRuntimeHost& runtime, MacroCommandHost& diagnostics) {
    // Native `kill` parses an explicit Object rvalue, invokes the target's
    // Object vtable slot +0x40 (InitializeRuntimeState), then immediately
    // leaves the interpreter when that target is this Macro's script owner.
    objects::Object* target = reinterpret_cast<objects::Object*>(
        static_cast<std::uintptr_t>(parse_rvalue(runtime, diagnostics)));
    if (target == nullptr) {
        return execution_terminated
                   ? MacroControlFlowResult::execution_terminated
                   : MacroControlFlowResult::iteration_complete;
    }

    runtime.initialize_object_runtime_state(*target);
    if (target == object_context.script_owner) {
        execution_terminated = true;
        return MacroControlFlowResult::scheduler_cleanup_required;
    }
    return MacroControlFlowResult::iteration_complete;
}

MacroControlFlowResult Macro::execute_object_enumeration_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    const auto push_value = [this](std::uint32_t value) {
        if (caos_value_stack_cursor_index >= caos_value_stack.size()) {
            execution_terminated = true;
            return false;
        }
        caos_value_stack[caos_value_stack_cursor_index++] = value;
        return true;
    };

    const auto pop_value = [this]() -> std::optional<std::uint32_t> {
        if (caos_value_stack_cursor_index == 0) {
            execution_terminated = true;
            return std::nullopt;
        }
        return caos_value_stack[--caos_value_stack_cursor_index];
    };

    const auto is_matching_candidate = [](const objects::Object& candidate,
                                          std::uint32_t classifier,
                                          std::uint32_t mask) {
        return (candidate.classifier_base() & mask) == classifier &&
               !candidate.is_sound_source_below_world_y();
    };

    const auto seek_next_command = [this]() {
        // Native enum scans byte-by-byte because `next` is a five-byte CAOS
        // token embedded in a variable-length script stream.  It resumes at
        // the first byte after the enum operands and leaves the cursor just
        // after the matching token.
        const std::size_t readable_capacity = std::min<std::size_t>(
            script_capacity_bytes, script_buffer.size());
        for (std::size_t candidate = script_cursor_offset;
             candidate + sizeof(CaosToken) <= readable_capacity; ++candidate) {
            CaosToken token = 0;
            std::memcpy(&token, script_buffer.data() + candidate,
                        sizeof(token));
            if (token == static_cast<CaosToken>(MacroCommand::next)) {
                if (candidate + sizeof(CaosToken) + 1 > readable_capacity) {
                    execution_terminated = true;
                    return false;
                }
                script_cursor_offset = candidate + sizeof(CaosToken) + 1;
                return true;
            }
        }
        execution_terminated = true;
        return false;
    };

    if (command == MacroCommand::enumerate_objects) {
        // The native packed query excludes the low event byte.  Zero means
        // wildcard for each component independently.
        const std::uint32_t family = parse_rvalue(runtime, diagnostics);
        const std::uint32_t genus = parse_rvalue(runtime, diagnostics);
        const std::uint32_t species = parse_rvalue(runtime, diagnostics);
        const std::uint32_t classifier =
            (family << 24) | (genus << 16) | (species << 8);
        const std::uint32_t mask =
            (family == 0 ? 0u : 0xff000000u) |
            (genus == 0 ? 0u : 0x00ff0000u) |
            (species == 0 ? 0u : 0x0000ff00u);
        const std::size_t registry_count = runtime.non_scenery_object_count();

        for (std::size_t index = 0; index < registry_count; ++index) {
            objects::Object* candidate = runtime.non_scenery_object_at(index);
            if (!is_matching_candidate(*candidate, classifier, mask)) {
                continue;
            }

            object_context.target_object = candidate;
            if (!push_value(static_cast<std::uint32_t>(script_cursor_offset)) ||
                !push_value(classifier) || !push_value(mask) ||
                !push_value(static_cast<std::uint32_t>(index))) {
                return MacroControlFlowResult::execution_terminated;
            }
            return MacroControlFlowResult::iteration_complete;
        }

        object_context.target_object = object_context.script_owner;
        seek_next_command();
        return execution_terminated
                   ? MacroControlFlowResult::execution_terminated
                   : MacroControlFlowResult::cursor_changed;
    }

    if (command != MacroCommand::next) {
        return MacroControlFlowResult::not_control_flow;
    }

    // enum stores [saved cursor, classifier, mask, registry index].  Native
    // `next` pops that tuple from the top in reverse order, advances the
    // index, and restores the tuple on a successful continuation.
    const std::optional<std::uint32_t> index_value = pop_value();
    const std::optional<std::uint32_t> mask_value = pop_value();
    const std::optional<std::uint32_t> classifier_value = pop_value();
    const std::optional<std::uint32_t> saved_cursor = pop_value();
    if (!index_value.has_value() || !mask_value.has_value() ||
        !classifier_value.has_value() || !saved_cursor.has_value()) {
        return MacroControlFlowResult::execution_terminated;
    }

    for (std::size_t index = static_cast<std::size_t>(*index_value) + 1;
         index < runtime.non_scenery_object_count(); ++index) {
        objects::Object* candidate = runtime.non_scenery_object_at(index);
        if (!is_matching_candidate(*candidate, *classifier_value,
                                   *mask_value)) {
            continue;
        }

        object_context.target_object = candidate;
        script_cursor_offset = *saved_cursor;
        if (!push_value(*saved_cursor) || !push_value(*classifier_value) ||
            !push_value(*mask_value) ||
            !push_value(static_cast<std::uint32_t>(index))) {
            return MacroControlFlowResult::execution_terminated;
        }
        return MacroControlFlowResult::cursor_changed;
    }

    object_context.target_object = object_context.script_owner;
    return MacroControlFlowResult::iteration_complete;
}

bool Macro::execute_random_target_command(MacroCommand command,
                                           MacroRuntimeHost& runtime,
                                           MacroCommandHost& diagnostics) {
    if (command != MacroCommand::random_target) {
        return false;
    }

    // Native `rtar` consumes family, genus, and species before it touches the
    // current target. Zero remains a per-component wildcard. The runtime
    // adapter owns the registry scan, sound-source exclusion, and CRT random
    // selection; Macro owns only the CAOS operand order and target update.
    const MacroClassifierPattern pattern{
        parse_rvalue(runtime, diagnostics),
        parse_rvalue(runtime, diagnostics),
        parse_rvalue(runtime, diagnostics),
    };
    object_context.target_object = runtime.random_non_scenery_object(pattern);
    return true;
}

MacroControlFlowResult Macro::execute_control_flow_command(
    MacroCommand command, std::optional<bool> condition) {
    const auto push_value = [this](std::uint32_t value) {
        if (caos_value_stack_cursor_index >= caos_value_stack.size()) {
            execution_terminated = true;
            return false;
        }
        caos_value_stack[caos_value_stack_cursor_index++] = value;
        return true;
    };

    const auto pop_value = [this]() -> std::optional<std::uint32_t> {
        if (caos_value_stack_cursor_index == 0) {
            execution_terminated = true;
            return std::nullopt;
        }
        return caos_value_stack[--caos_value_stack_cursor_index];
    };

    const auto scan_branch = [this](bool stop_at_else) {
        const std::size_t readable_capacity = std::min<std::size_t>(
            script_capacity_bytes, script_buffer.size());
        std::size_t candidate = script_cursor_offset;
        std::uint32_t nesting_depth = 1;

        while (candidate + sizeof(CaosToken) <= readable_capacity) {
            CaosToken token = 0;
            std::memcpy(&token, script_buffer.data() + candidate,
                        sizeof(token));
            const auto command_token = static_cast<MacroCommand>(token);

            if (stop_at_else && nesting_depth == 1 &&
                command_token == MacroCommand::else_) {
                if (candidate + 5 > readable_capacity) {
                    execution_terminated = true;
                    return false;
                }
                script_cursor_offset = candidate + 5;
                return true;
            }
            if (command_token == MacroCommand::do_if) {
                ++nesting_depth;
            } else if (command_token == MacroCommand::end_if) {
                if (nesting_depth == 0) {
                    execution_terminated = true;
                    return false;
                }
                --nesting_depth;
                if (nesting_depth == 0) {
                    if (candidate + 5 > readable_capacity) {
                        execution_terminated = true;
                        return false;
                    }
                    script_cursor_offset = candidate + 5;
                    return true;
                }
            }
            ++candidate;
        }

        execution_terminated = true;
        return false;
    };

    const auto repeat_saved_cursor = [&]() {
        const std::optional<std::uint32_t> saved_cursor = pop_value();
        if (!saved_cursor.has_value()) {
            return MacroControlFlowResult::execution_terminated;
        }
        script_cursor_offset = *saved_cursor;
        if (!push_value(*saved_cursor)) {
            return MacroControlFlowResult::execution_terminated;
        }
        return MacroControlFlowResult::cursor_changed;
    };

    switch (command) {
    case MacroCommand::do_if:
        if (!condition.has_value()) {
            execution_terminated = true;
            return MacroControlFlowResult::execution_terminated;
        }
        if (*condition) {
            return MacroControlFlowResult::iteration_complete;
        }
        return scan_branch(true)
                   ? MacroControlFlowResult::cursor_changed
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommand::else_:
        return scan_branch(false)
                   ? MacroControlFlowResult::cursor_changed
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommand::end_if:
        return MacroControlFlowResult::iteration_complete;

    case MacroCommand::loop:
        return begin_loop();

    case MacroCommand::until:
        if (!condition.has_value()) {
            execution_terminated = true;
            return MacroControlFlowResult::execution_terminated;
        }
        if (*condition) {
            if (!pop_value().has_value()) {
                return MacroControlFlowResult::execution_terminated;
            }
            return MacroControlFlowResult::iteration_complete;
        }
        return repeat_saved_cursor();

    case MacroCommand::repeat: {
        const std::optional<std::uint32_t> count = pop_value();
        if (!count.has_value()) {
            return MacroControlFlowResult::execution_terminated;
        }
        if (*count <= 1) {
            if (!pop_value().has_value()) {
                return MacroControlFlowResult::execution_terminated;
            }
            return MacroControlFlowResult::iteration_complete;
        }

        const std::optional<std::uint32_t> saved_cursor = pop_value();
        if (!saved_cursor.has_value()) {
            return MacroControlFlowResult::execution_terminated;
        }
        script_cursor_offset = *saved_cursor;
        if (!push_value(*saved_cursor) || !push_value(*count - 1)) {
            return MacroControlFlowResult::execution_terminated;
        }
        return MacroControlFlowResult::cursor_changed;
    }

    case MacroCommand::ever:
        return repeat_saved_cursor();

    case MacroCommand::return_from_subroutine: {
        const std::optional<std::uint32_t> saved_cursor = pop_value();
        if (!saved_cursor.has_value()) {
            script_cursor_offset = 0;
            return MacroControlFlowResult::execution_terminated;
        }
        script_cursor_offset = *saved_cursor;
        return MacroControlFlowResult::cursor_changed;
    }

    case MacroCommand::endm:
    case MacroCommand::stop:
    case MacroCommand::subroutine:
        // Native cleanup is performed by the scheduler after rewinding to the
        // command's five-byte start.  Keep that ownership boundary explicit;
        // the outer interpreter must remove/destroy the Macro and must not
        // continue dispatching this instance.
        if (script_cursor_offset < 5) {
            execution_terminated = true;
            script_cursor_offset = 0;
            return MacroControlFlowResult::execution_terminated;
        }
        script_cursor_offset -= 5;
        return MacroControlFlowResult::scheduler_cleanup_required;

    case MacroCommand::repeat_counted:
    case MacroCommand::kill:
    default:
        return MacroControlFlowResult::not_control_flow;
    }
}

MacroControlFlowResult Macro::dispatch_interpreter_command(
    MacroCommand command, const MacroInterpreterBindings& bindings) {
    const auto missing_binding = [this]() {
        execution_terminated = true;
        return MacroControlFlowResult::execution_terminated;
    };
    const auto require_runtime = [&]() -> MacroRuntimeHost* {
        return bindings.runtime;
    };
    const auto require_diagnostics = [&]() -> MacroCommandHost* {
        return bindings.diagnostics;
    };

    // The family switch is the recovered ExecuteInterpreter ownership map.
    // Each leaf remains a typed source-level handler; this router performs no
    // raw offset arithmetic and does not recreate the native monolith.
    switch (classify_command(static_cast<CaosToken>(command))) {
    case MacroCommandFamily::control_flow:
        if (command == MacroCommand::repeat_counted) {
            if (require_runtime() == nullptr || require_diagnostics() == nullptr) {
                return missing_binding();
            }
            return execute_counted_repeat_command(*bindings.runtime,
                                                  *bindings.diagnostics);
        }
        if (command == MacroCommand::next) {
            if (require_runtime() == nullptr || require_diagnostics() == nullptr) {
                return missing_binding();
            }
            return execute_object_enumeration_command(
                command, *bindings.runtime, *bindings.diagnostics);
        }
        if (command == MacroCommand::global_subroutine) {
            return execute_global_subroutine_command();
        }
        if (command == MacroCommand::do_if || command == MacroCommand::until) {
            if (require_runtime() == nullptr || require_diagnostics() == nullptr) {
                return missing_binding();
            }
            const auto condition = parse_comparison_condition(
                *bindings.runtime, *bindings.diagnostics);
            return execute_control_flow_command(command, condition);
        }
        return execute_control_flow_command(command);

    case MacroCommandFamily::arithmetic_and_lvalue:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::randomize_variable) {
            return execute_randomize_variable_command(
                       command, *bindings.runtime, *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        return execute_arithmetic_command(command, *bindings.runtime,
                                          *bindings.diagnostics)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::object_motion:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::tele) {
            return execute_teleport_command(command, *bindings.runtime,
                                            *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (bindings.object_motion == nullptr) {
            return missing_binding();
        }
        return execute_object_motion_command(command, *bindings.object_motion,
                                             *bindings.runtime)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::object_appearance:
        if (bindings.runtime == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::pose) {
            if (bindings.diagnostics == nullptr) {
                return missing_binding();
            }
            return execute_pose_command(command, *bindings.runtime,
                                        *bindings.diagnostics);
        }
        return execute_preload_image_sequence_command(command,
                                                       *bindings.runtime)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::object_and_script_lifecycle:
        if (command == MacroCommand::kill) {
            if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
                return missing_binding();
            }
            return execute_kill_command(*bindings.runtime, *bindings.diagnostics);
        }
        if (command == MacroCommand::endm) {
            return execute_control_flow_command(command);
        }
        if (command == MacroCommand::script) {
            if (bindings.scripts == nullptr || bindings.runtime == nullptr ||
                bindings.diagnostics == nullptr) {
                return missing_binding();
            }
            return execute_script_command(command, *bindings.scripts,
                                          *bindings.runtime,
                                          *bindings.diagnostics);
        }
        if (command == MacroCommand::script_extended) {
            if (bindings.system == nullptr || bindings.runtime == nullptr ||
                bindings.diagnostics == nullptr) {
                return missing_binding();
            }
            return execute_script_extended_command(
                command, *bindings.system, *bindings.runtime,
                *bindings.diagnostics);
        }
        if (command == MacroCommand::instantiate) {
            return execute_instantiate_command(command)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::over) {
            return execute_over_command(command);
        }
        if (command == MacroCommand::part) {
            if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
                return missing_binding();
            }
            return execute_part_command(command, *bindings.runtime,
                                        *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::execute) {
            consume_execute_command_arguments();
            return MacroControlFlowResult::iteration_complete;
        }
        if (command == MacroCommand::mate ||
            command == MacroCommand::hidden_fertilize) {
            if (bindings.runtime == nullptr) {
                return missing_binding();
            }
            return execute_mate_command(command, *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::delete_creature) {
            return execute_delete_creature_command(command, *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::dream) {
            return execute_dream_command(command, *bindings.runtime,
                                         *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::sleep) {
            return execute_sleep_command(command, *bindings.runtime,
                                         *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::drop) {
            return execute_drop_command(command, *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::done) {
            return execute_done_command(command, *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::touch) {
            return execute_touch_command(command, *bindings.runtime);
        }
        if (command == MacroCommand::edit || command == MacroCommand::version ||
            command == MacroCommand::room) {
            if (bindings.system == nullptr) {
                return missing_binding();
            }
            if (command == MacroCommand::edit) {
                return execute_edit_command(command, *bindings.runtime,
                                            *bindings.system)
                           ? MacroControlFlowResult::iteration_complete
                           : MacroControlFlowResult::execution_terminated;
            }
            if (command == MacroCommand::version) {
                return execute_version_command(command, *bindings.runtime,
                                               *bindings.system)
                           ? MacroControlFlowResult::iteration_complete
                           : MacroControlFlowResult::execution_terminated;
            }
            if (command == MacroCommand::room) {
                return execute_room_command(command, *bindings.runtime,
                                            *bindings.system)
                           ? MacroControlFlowResult::iteration_complete
                           : MacroControlFlowResult::execution_terminated;
            }
        }
        if (command == MacroCommand::base) {
            return execute_base_command(command, *bindings.runtime,
                                        *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::behavior) {
            return execute_behavior_command(command, *bindings.runtime,
                                            *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        // `quit` is a native scheduler-release command. The physical removal
        // is deliberately performed by the finalization join below.
        if (command == MacroCommand::quit) {
            execution_terminated = true;
            return MacroControlFlowResult::scheduler_cleanup_required;
        }
        return missing_binding();

    case MacroCommandFamily::compound_geometry:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        return execute_compound_geometry_command(
                   command, *bindings.runtime, *bindings.diagnostics)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::creature_motor:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        return execute_creature_motor_command(command, *bindings.runtime,
                                              *bindings.diagnostics)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::creature_runtime:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        return execute_creature_runtime_command(command, *bindings.runtime,
                                                *bindings.diagnostics)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::sound:
        if (bindings.runtime == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::sound_fade) {
            if (bindings.sound_policy == nullptr) {
                return missing_binding();
            }
            return execute_sound_policy_command(command, *bindings.sound_policy)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (bindings.sound == nullptr) {
            return missing_binding();
        }
        if (bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        return execute_sound_command(command, *bindings.sound, *bindings.runtime)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::speech_and_messaging:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::message) {
            if (bindings.message == nullptr) {
                return missing_binding();
            }
            execute_message_command(*bindings.message, *bindings.runtime);
            return MacroControlFlowResult::iteration_complete;
        }
        if (command == MacroCommand::target) {
            return execute_target_command(command, *bindings.runtime,
                                          *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::from_object) {
            object_context.from_object = reinterpret_cast<objects::Object*>(
                static_cast<std::uintptr_t>(
                    parse_rvalue(*bindings.runtime, *bindings.diagnostics)));
            return MacroControlFlowResult::iteration_complete;
        }
        if (bindings.speech == nullptr) {
            return missing_binding();
        }
        return execute_speech_command(command, *bindings.speech,
                                      *bindings.runtime)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::stimulus_and_events:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::stimulus) {
            if (bindings.stimulus == nullptr) {
                return missing_binding();
            }
            execute_stimulus_command(*bindings.stimulus, *bindings.runtime);
            return MacroControlFlowResult::iteration_complete;
        }
        if (command == MacroCommand::remove_event) {
            return execute_remove_event_command(command, *bindings.runtime,
                                                *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::event) {
            return execute_event_command(command, *bindings.runtime,
                                         *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::trig) {
            return execute_trigger_command(command, *bindings.runtime,
                                           *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::chem) {
            return execute_chemical_command(command, *bindings.runtime,
                                            *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::fire) {
            return execute_fire_command(command, *bindings.runtime,
                                        *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::drive_presentation) {
            return execute_drive_presentation_command(command,
                                                      *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (bindings.stimulus == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::stm_hash) {
            execute_stimulus_prefix_command(*bindings.stimulus,
                                            *bindings.runtime);
            return MacroControlFlowResult::iteration_complete;
        }
        return missing_binding();

    case MacroCommandFamily::vehicle_interaction:
        if (bindings.runtime == nullptr || bindings.object_events == nullptr) {
            return missing_binding();
        }
        return execute_vehicle_command(command, *bindings.runtime,
                                       *bindings.object_events)
                   ? MacroControlFlowResult::iteration_complete
                   : MacroControlFlowResult::execution_terminated;

    case MacroCommandFamily::debug_system_and_misc:
        if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
            return missing_binding();
        }
        if (command == MacroCommand::enumerate_objects) {
            return execute_object_enumeration_command(
                command, *bindings.runtime, *bindings.diagnostics);
        }
        if (command == MacroCommand::random_target) {
            return execute_random_target_command(command, *bindings.runtime,
                                                 *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::wait) {
            return execute_wait_command(*bindings.runtime, *bindings.diagnostics);
        }
        if (command == MacroCommand::walk) {
            objects::Object* target = object_context.target_object;
            if (target != nullptr && bindings.runtime->is_creature_object(*target)) {
                bindings.runtime->select_creature_walk_gait(*target);
            }
            return MacroControlFlowResult::iteration_complete;
        }
        if (command == MacroCommand::mcrt) {
            if (bindings.object_motion == nullptr) {
                return missing_binding();
            }
            return execute_object_motion_command(
                       command, *bindings.object_motion, *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::tele) {
            return execute_teleport_command(command, *bindings.runtime,
                                            *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::approach ||
            command == MacroCommand::uppercase_approach) {
            return execute_approach_command(command, *bindings.runtime);
        }
        if (command == MacroCommand::pointer) {
            return execute_pointer_command(command, *bindings.runtime);
        }
        if (command == MacroCommand::anim) {
            return execute_animation_command(command)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::cabinet) {
            return execute_vehicle_bounds_command(
                       command, *bindings.runtime, *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::knob) {
            return execute_compound_knob_command(
                       command, *bindings.runtime, *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::slim) {
            return execute_object_bounds_command(command, *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::tick) {
            return execute_timer_command(command, *bindings.runtime,
                                         *bindings.diagnostics)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::tool) {
            if (bindings.application == nullptr) {
                return missing_binding();
            }
            return execute_tool_command(command, *bindings.application,
                                        *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::debug_message ||
            command == MacroCommand::debug_value || command == MacroCommand::dbug ||
            command == MacroCommand::vocabulary) {
            if (bindings.debug == nullptr) {
                return missing_binding();
            }
            return execute_debug_command(command, *bindings.debug,
                                         *bindings.runtime)
                       ? MacroControlFlowResult::iteration_complete
                       : MacroControlFlowResult::execution_terminated;
        }
        if (command == MacroCommand::version || command == MacroCommand::room) {
            if (bindings.system == nullptr) {
                return missing_binding();
            }
            return command == MacroCommand::version
                       ? (execute_version_command(command, *bindings.runtime,
                                                  *bindings.system)
                              ? MacroControlFlowResult::iteration_complete
                              : MacroControlFlowResult::execution_terminated)
                       : (execute_room_command(command, *bindings.runtime,
                                               *bindings.system)
                              ? MacroControlFlowResult::iteration_complete
                              : MacroControlFlowResult::execution_terminated);
        }
        return missing_binding();

    case MacroCommandFamily::prefixed:
        if (bindings.runtime == nullptr) {
            return missing_binding();
        }
        switch (static_cast<MacroPrefixCommand>(static_cast<CaosToken>(command))) {
        case MacroPrefixCommand::application:
            if (bindings.application == nullptr) return missing_binding();
            execute_application_prefix_command(*bindings.application,
                                               *bindings.runtime);
            break;
        case MacroPrefixCommand::aim:
            if (bindings.object_motion == nullptr) return missing_binding();
            execute_aim_prefix_command(*bindings.object_motion,
                                        *bindings.runtime);
            break;
        case MacroPrefixCommand::blackboard:
            if (bindings.blackboard == nullptr) return missing_binding();
            execute_blackboard_caos_command(*bindings.blackboard,
                                            *bindings.runtime);
            break;
        case MacroPrefixCommand::dde:
            if (bindings.dde == nullptr) return missing_binding();
            execute_dde_command(*bindings.dde, *bindings.runtime);
            break;
        case MacroPrefixCommand::system:
            if (bindings.system == nullptr) return missing_binding();
            execute_system_caos_command(*bindings.system, *bindings.runtime);
            break;
        case MacroPrefixCommand::new_object:
            if (bindings.new_object == nullptr) return missing_binding();
            execute_new_command(*bindings.new_object, *bindings.runtime);
            break;
        }
        return MacroControlFlowResult::iteration_complete;

    case MacroCommandFamily::unknown:
        break;
    }

    if (bindings.diagnostics != nullptr) {
        report_syntax_error(*bindings.diagnostics, "CAOS command");
    }
    return missing_binding();
}

MacroControlFlowResult Macro::execute_wait_command(
    MacroRuntimeHost& runtime, MacroCommandHost& diagnostics) {
    // ExecuteInterpreter has already consumed the five-byte `wait` command
    // token.  The native implementation deliberately rewinds to that token
    // while a wait is pending, so the scheduler re-enters the same command on
    // the next tick instead of advancing into its operand.
    if (script_cursor_offset < sizeof(CaosToken) + 1) {
        execution_terminated = true;
        script_cursor_offset = 0;
        return MacroControlFlowResult::execution_terminated;
    }

    const std::size_t command_start = script_cursor_offset - 5;
    if (wait_ticks_remaining == 0) {
        // The first pass parses the wait count from the operand immediately
        // following `wait`, then rewinds before storing count - 1.  Keep the
        // unsigned wraparound for a zero count: it is the native uint32_t
        // decrement and therefore represents a nonzero pending wait.
        const std::uint32_t wait_count = parse_rvalue(runtime, diagnostics);
        script_cursor_offset = command_start;
        wait_ticks_remaining = wait_count - 1u;
    } else {
        --wait_ticks_remaining;
        script_cursor_offset = command_start;
    }

    if (wait_ticks_remaining != 0) {
        return MacroControlFlowResult::cursor_changed;
    }

    // On the completing pass native execution advances to the operand and
    // parses/discards it once more.  This is observable cursor behavior and
    // must not be replaced by a simplified "skip wait" implementation.
    const std::size_t readable_capacity = std::min<std::size_t>(
        script_capacity_bytes, script_buffer.size());
    const std::size_t operand_cursor = command_start + 5;
    if (operand_cursor > readable_capacity) {
        execution_terminated = true;
        // Preserve the native malformed-end path: the cursor is still at the
        // rewound command start when ParseRValue is entered.
        parse_rvalue(runtime, diagnostics);
        return MacroControlFlowResult::execution_terminated;
    }

    script_cursor_offset = operand_cursor;
    parse_rvalue(runtime, diagnostics);
    return execution_terminated
               ? MacroControlFlowResult::execution_terminated
               : MacroControlFlowResult::iteration_complete;
}

MacroInterpreterFinalization Macro::finalize_interpreter_iteration(
    bool iteration_completed) {
    // Native ExecuteInterpreter first snapshots execution_terminated.  A
    // terminated output-capture macro is removed from the running scheduler
    // but retained unless destruction was already requested.  All other
    // terminated paths set the destruction request before scheduler cleanup.
    // The scheduler performs the actual erase/delete operation; this method
    // only returns the typed lifecycle decision and records the native flag.
    if (execution_terminated) {
        if (capture_output_enabled && !destroy_when_finished) {
            return MacroInterpreterFinalization::remove_from_scheduler_and_retain;
        }
        destroy_when_finished = true;
        return MacroInterpreterFinalization::remove_from_scheduler_and_destroy;
    }

    // A non-capturing interpreter returns when a command did not complete an
    // iteration.  Once a command reaches the common iteration-complete join,
    // native control flow loops back to the dispatch entry.
    if (!capture_output_enabled && !iteration_completed) {
        return MacroInterpreterFinalization::return_to_caller;
    }
    return MacroInterpreterFinalization::continue_dispatch;
}

bool Macro::execute_arithmetic_command(MacroCommand command,
                                       MacroRuntimeHost& runtime,
                                       MacroCommandHost& diagnostics) {
    const auto is_arithmetic_command = [](MacroCommand candidate) {
        switch (candidate) {
        case MacroCommand::set_variable:
        case MacroCommand::add_variable:
        case MacroCommand::subtract_variable:
        case MacroCommand::multiply_variable:
        case MacroCommand::divide_variable:
        case MacroCommand::modulo_variable:
        case MacroCommand::bitwise_and_variable:
        case MacroCommand::bitwise_or_variable:
        case MacroCommand::negate_variable:
            return true;
        default:
            return false;
        }
    };

    if (!is_arithmetic_command(command)) {
        return false;
    }

    // Every arithmetic command begins with the destination lvalue token.
    // ParseRValue and AssignLValue retain the native unsigned 32-bit CAOS
    // value representation; division and remainder deliberately use the
    // signed native operation recovered from ExecuteInterpreter.
    // SETV is the one that consumes its destination: the native advances the
    // cursor past both the opcode and the destination before parsing the
    // value.  Every other arithmetic command peeks, so its first rvalue is
    // the destination's own current value.
    if (command == MacroCommand::set_variable) {
        const CaosToken destination_token = read_next_token();
        assign_lvalue(runtime, diagnostics, destination_token,
                      parse_rvalue(runtime, diagnostics));
        return true;
    }

    const CaosToken destination_token = peek_next_token();
    if (command == MacroCommand::negate_variable) {
        const std::uint32_t value = parse_rvalue(runtime, diagnostics);
        assign_lvalue(runtime, diagnostics, destination_token,
                      static_cast<std::uint32_t>(0u - value));
        return true;
    }

    const std::uint32_t first_value = parse_rvalue(runtime, diagnostics);
    const std::uint32_t second_value = parse_rvalue(runtime, diagnostics);
    std::uint32_t result = 0;
    bool should_assign = true;
    switch (command) {
    case MacroCommand::add_variable:
        result = first_value + second_value;
        break;
    case MacroCommand::subtract_variable:
        result = first_value - second_value;
        break;
    case MacroCommand::multiply_variable:
        result = first_value * second_value;
        break;
    case MacroCommand::divide_variable:
        if (second_value == 0) {
            should_assign = false;
        } else {
            result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(first_value) /
                static_cast<std::int32_t>(second_value));
        }
        break;
    case MacroCommand::modulo_variable:
        if (second_value == 0) {
            should_assign = false;
        } else {
            result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(first_value) %
                static_cast<std::int32_t>(second_value));
        }
        break;
    case MacroCommand::bitwise_and_variable:
        result = first_value & second_value;
        break;
    case MacroCommand::bitwise_or_variable:
        result = first_value | second_value;
        break;
    default:
        return false;
    }

    if (should_assign) {
        assign_lvalue(runtime, diagnostics, destination_token, result);
    }
    return true;
}

bool Macro::execute_randomize_variable_command(
    MacroCommand command, MacroRuntimeHost& runtime,
    MacroCommandHost& diagnostics) {
    if (command != MacroCommand::randomize_variable) {
        return false;
    }

    // Native `rndv` reads an lvalue token, then parses the inclusive lower
    // and upper bounds.  The comparison is signed; when the upper bound is
    // below the lower bound the native branch assigns the lower bound
    // unchanged.  Otherwise the original CRT rand() result is reduced by
    // the inclusive span and added to the lower bound.
    const CaosToken destination_token = read_next_token();
    const std::uint32_t lower_value = parse_rvalue(runtime, diagnostics);
    const std::uint32_t upper_value = parse_rvalue(runtime, diagnostics);
    const auto lower = static_cast<std::int32_t>(lower_value);
    const auto upper = static_cast<std::int32_t>(upper_value);

    if (upper < lower) {
        assign_lvalue(runtime, diagnostics, destination_token, lower_value);
        return true;
    }

    // Use unsigned bit arithmetic for the 32-bit x86 value representation;
    // this preserves the native wrapped span calculation without invoking
    // C++ signed-overflow language-level behavior.
    const std::uint32_t inclusive_span =
        static_cast<std::uint32_t>(upper_value - lower_value) + 1u;
    const std::uint32_t random_offset =
        static_cast<std::uint32_t>(std::rand()) % inclusive_span;
    assign_lvalue(runtime, diagnostics, destination_token,
                  static_cast<std::uint32_t>(lower_value + random_offset));
    return true;
}

bool Macro::execute_object_motion_command(MacroCommand command,
                                          MacroObjectMotionHost& host,
                                          MacroRuntimeHost& runtime) {
    if (command != MacroCommand::move_to &&
        command != MacroCommand::move_by &&
        command != MacroCommand::mcrt) {
        return false;
    }

    const int first_value = static_cast<int>(parse_rvalue(runtime, host));
    const int second_value = static_cast<int>(parse_rvalue(runtime, host));
    objects::Object* target = object_context.target_object;
    if (target != nullptr) {
        if (command == MacroCommand::mcrt) {
            // Native `mcrt` temporarily detaches the target from any bounds
            // reference, moves it with UNBOUNDED_2 semantics, and restores
            // DEFAULT_WORLD afterward.  The virtual call is the same typed
            // Object movement boundary used by `mvto`; the two bounds passes
            // are deliberately kept in native order around that call.
            target->set_bounds_reference_object(nullptr);
            runtime.set_object_bounds_mode(
                *target, static_cast<std::uint32_t>(
                             objects::Object::BoundsMode::unbounded_2));
            runtime.update_object_movement_bounds(*target);
            host.move_to_and_redraw(*target, first_value, second_value);
            runtime.set_object_bounds_mode(
                *target, static_cast<std::uint32_t>(
                             objects::Object::BoundsMode::default_world));
            runtime.update_object_movement_bounds(*target);
        } else if (command == MacroCommand::move_to) {
            host.move_to_and_redraw(*target, first_value, second_value);
        } else {
            host.move_by_and_redraw(*target, first_value, second_value);
        }
    }
    return true;
}

bool Macro::execute_teleport_command(MacroCommand command,
                                     MacroRuntimeHost& runtime,
                                     MacroCommandHost& diagnostics) {
    if (command != MacroCommand::tele) {
        return false;
    }

    // Native `tele` consumes both coordinates before it inspects the
    // Creature registry.  Registry ownership, the matched-entry +0x1c
    // linkage, temporary bounds modes, Object movement, and selected-view
    // refresh remain in the application adapter.
    const int world_x =
        static_cast<int>(parse_rvalue(runtime, diagnostics));
    const int world_y =
        static_cast<int>(parse_rvalue(runtime, diagnostics));
    runtime.teleport_target_and_refresh_selection(object_context.target_object,
                                                  world_x, world_y);
    return true;
}

bool Macro::execute_drive_presentation_command(MacroCommand command,
                                               MacroRuntimeHost& runtime) {
    if (command != MacroCommand::drive_presentation) {
        return false;
    }

    // Native `dpas` has no explicit operands.  It requires a non-null
    // family-3 target, then asks the application-owned registry/ring adapter
    // to queue event 5 for each Creature whose bounds reference is that
    // target.  The adapter preserves the native full-ring drop behavior.
    objects::Object* target = object_context.target_object;
    if (target != nullptr && runtime.is_vehicle_object(*target)) {
        runtime.drive_presentation_for_bound_creatures(*target);
    }
    return true;
}

bool Macro::execute_sound_command(MacroCommand command, MacroSoundHost& host,
                                  MacroRuntimeHost& runtime) {
    if (command != MacroCommand::sound_start &&
        command != MacroCommand::sound_change &&
        command != MacroCommand::sound_loop &&
        command != MacroCommand::sound_query &&
        command != MacroCommand::sound_voice &&
        command != MacroCommand::sound_pause &&
        command != MacroCommand::fade &&
        command != MacroCommand::sound_stop_channel) {
        return false;
    }

    objects::Object* target = object_context.target_object;
    if (command == MacroCommand::fade) {
        if (target != nullptr) {
            host.fade_continuous_sound(*target);
        }
        return true;
    }
    if (command == MacroCommand::sound_stop_channel) {
        if (target != nullptr) {
            host.stop_continuous_sound(*target);
        }
        return true;
    }

    if (command == MacroCommand::sound_voice) {
        std::array<char, 0x80> text{};
        read_bracketed_text_argument(text.data(), static_cast<int>(text.size()));

        // Native sndv consumes the bracketed bytes, sign-extends each of the
        // first four bytes, and assembles them little-endian.  Sound names are
        // normally printable ASCII, but retaining the signed-byte assembly
        // keeps the boundary exact for every four-byte descriptor.
        const auto signed_byte = [](char value) -> std::uint32_t {
            return static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
        };
        std::uint32_t sound_id = signed_byte(text[3]);
        sound_id = (sound_id << 8) + signed_byte(text[2]);
        sound_id = (sound_id << 8) + signed_byte(text[1]);
        sound_id = (sound_id << 8) + signed_byte(text[0]);

        if (target != nullptr) {
            host.play_sound_effect(*target, sound_id, 0);
        }
        return true;
    }

    // These commands carry the sound descriptor as a five-byte CAOS literal,
    // rather than as an rvalue. Even a null target consumes the descriptor;
    // the native dispatcher only suppresses the object-side call.
    const sound::SoundId sound_id = read_next_token();
    if (command == MacroCommand::sound_pause) {
        if (target != nullptr) {
            host.load_sound_cache_if_audible(*target, sound_id);
        }
        return true;
    }
    if (command == MacroCommand::sound_query) {
        const int queue_delay_ticks =
            static_cast<int>(parse_rvalue(runtime, host));
        if (target != nullptr) {
            host.play_sound_effect(*target, sound_id, queue_delay_ticks);
        }
        return true;
    }

    if (target == nullptr) {
        return true;
    }
    if (command == MacroCommand::sound_start) {
        host.play_sound_effect(*target, sound_id, 0);
    } else if (command == MacroCommand::sound_change) {
        host.set_continuous_sound(*target, sound_id, false);
    } else {
        host.set_continuous_sound(*target, sound_id, true);
    }
    return true;
}

bool Macro::execute_sound_policy_command(MacroCommand command,
                                         MacroSoundPolicyHost& host) {
    if (command != MacroCommand::sound_fade) {
        return false;
    }

    // The native sndf handler consumes one five-byte subcommand regardless of
    // whether it is recognised. Unknown policy words therefore do not fall
    // through to the outer opcode error path.
    const MacroPrefixSubcommand policy =
        static_cast<MacroPrefixSubcommand>(read_next_token());
    switch (policy) {
    case MacroPrefixSubcommand::sound_foreground:
        host.set_sound_foreground_policy();
        break;
    case MacroPrefixSubcommand::sound_on:
        host.enable_sound_and_restore_if_ready();
        break;
    case MacroPrefixSubcommand::sound_off:
        host.disable_sound_and_suspend_if_ready();
        break;
    case MacroPrefixSubcommand::sound_conservative:
        host.set_sound_conservative_policy();
        break;
    default:
        break;
    }
    return true;
}

bool Macro::execute_speech_command(MacroCommand command, MacroSpeechHost& host,
                                   MacroRuntimeHost& runtime) {
    if (command != MacroCommand::say_hash &&
        command != MacroCommand::say_dollar &&
        command != MacroCommand::say_name) {
        return false;
    }

    objects::Object* target = object_context.target_object;
    if (command == MacroCommand::say_hash) {
        const std::uint32_t learned_word_index =
            parse_rvalue(runtime, host);
        if (target != nullptr) {
            host.speak_learned_word(*target, learned_word_index);
        }
        return true;
    }
    if (command == MacroCommand::say_dollar) {
        // The native handler uses a fixed 0x80-byte stack buffer. Preserve
        // that truncation boundary even though the general Macro helper can
        // read a larger bracketed argument for other commands.
        std::array<char, 0x80> text{};
        read_bracketed_text_argument(text.data(), static_cast<int>(text.size()));
        if (target != nullptr) {
            host.speak_text(*target, std::string_view(text.data()));
        }
        return true;
    }
    if (target != nullptr) {
        host.speak_dominant_drive_phrase(*target);
    }
    return true;
}

MacroControlFlowResult Macro::execute_interpreter(
    const MacroInterpreterBindings& bindings) {
    if (bindings.runtime == nullptr || bindings.diagnostics == nullptr) {
        execution_terminated = true;
        return MacroControlFlowResult::execution_terminated;
    }

    // One iteration is one native five-byte command fetch plus its typed
    // family handler. Cursor rewinds (wait, approach, over, and motion retry)
    // intentionally return through the common finalization join so the
    // scheduler, rather than a hidden callback, decides whether to re-enter.
    while (!execution_terminated) {
        const CaosToken token = read_next_token();
        if (execution_terminated) {
            break;
        }

        const std::size_t command_start =
            script_cursor_offset >= sizeof(CaosToken) + 1
                ? script_cursor_offset - (sizeof(CaosToken) + 1)
                : 0;
        if (bindings.trace != nullptr) {
            bindings.trace->command_fetched(*this, token, command_start);
        }

        MacroControlFlowResult result =
            MacroControlFlowResult::execution_terminated;
        try {
            result = dispatch_interpreter_command(
                static_cast<MacroCommand>(token), bindings);
        } catch (...) {
            // The native body has an exception cleanup path which reports the
            // current script context before joining scheduler finalization.
            // Keep that boundary explicit; host adapters may translate their
            // platform exceptions, but the Macro still owns termination.
            if (bindings.exceptions != nullptr) {
                handle_script_execution_exception(*bindings.exceptions);
            }
            if (bindings.trace != nullptr) {
                bindings.trace->execution_exception(*this);
            }
            execution_terminated = true;
            result = MacroControlFlowResult::execution_terminated;
        }
        if (bindings.trace != nullptr) {
            bindings.trace->command_result(*this, token, result,
                                           script_cursor_offset);
        }
        if (result == MacroControlFlowResult::macro_destroyed) {
            // The handler already performed the scheduler removal and may have
            // deleted this Macro; no member access is valid from here.
            return result;
        }
        if (result == MacroControlFlowResult::interpreter_returned) {
            if (bindings.trace != nullptr) {
                bindings.trace->scheduler_action(
                    *this, MacroInterpreterFinalization::return_to_caller);
            }
            remove_from_running_scheduler_and_release();
            return result;
        }
        if (result == MacroControlFlowResult::scheduler_cleanup_required) {
            if (bindings.trace != nullptr) {
                bindings.trace->scheduler_action(
                    *this,
                    destroy_when_finished
                        ? MacroInterpreterFinalization::remove_from_scheduler_and_destroy
                        : MacroInterpreterFinalization::remove_from_scheduler_and_retain);
            }
            remove_from_running_scheduler_and_release();
            return result;
        }

        const bool iteration_completed =
            result == MacroControlFlowResult::iteration_complete;
        const MacroInterpreterFinalization finalization =
            finalize_interpreter_iteration(iteration_completed);
        if (bindings.trace != nullptr) {
            bindings.trace->iteration_finalized(*this, finalization,
                                                script_cursor_offset);
        }
        switch (finalization) {
        case MacroInterpreterFinalization::continue_dispatch:
            continue;
        case MacroInterpreterFinalization::return_to_caller:
            if (bindings.trace != nullptr) {
                bindings.trace->scheduler_action(*this, finalization);
            }
            return MacroControlFlowResult::interpreter_returned;
        case MacroInterpreterFinalization::remove_from_scheduler_and_retain:
            if (bindings.trace != nullptr) {
                bindings.trace->scheduler_action(*this, finalization);
            }
            remove_from_running_scheduler_and_release();
            return MacroControlFlowResult::scheduler_cleanup_required;
        case MacroInterpreterFinalization::remove_from_scheduler_and_destroy:
            if (bindings.trace != nullptr) {
                bindings.trace->scheduler_action(*this, finalization);
            }
            remove_from_running_scheduler_and_destroy();
            return MacroControlFlowResult::scheduler_cleanup_required;
        }
    }

    const MacroInterpreterFinalization finalization =
        finalize_interpreter_iteration(false);
    if (finalization ==
        MacroInterpreterFinalization::remove_from_scheduler_and_retain) {
        if (bindings.trace != nullptr) {
            bindings.trace->iteration_finalized(*this, finalization,
                                                script_cursor_offset);
            bindings.trace->scheduler_action(*this, finalization);
        }
        remove_from_running_scheduler_and_release();
    } else if (finalization ==
               MacroInterpreterFinalization::remove_from_scheduler_and_destroy) {
        if (bindings.trace != nullptr) {
            bindings.trace->iteration_finalized(*this, finalization,
                                                script_cursor_offset);
            bindings.trace->scheduler_action(*this, finalization);
        }
        remove_from_running_scheduler_and_destroy();
    } else if (bindings.trace != nullptr) {
        bindings.trace->iteration_finalized(*this, finalization,
                                            script_cursor_offset);
    }
    return MacroControlFlowResult::scheduler_cleanup_required;
}

std::size_t Macro::execute_to_output_buffer(
    const MacroExecutionHost& execution_host,
    MacroInterpreterHost& interpreter_host, std::string& output_buffer) {
    reset_execution_state(execution_host);
    output_text.clear();
    capture_output_enabled = true;
    destroy_when_finished = false;
    const MacroInterpreterBindings bindings =
        interpreter_host.interpreter_bindings();
    execute_interpreter(bindings);
    capture_output_enabled = false;
    output_buffer = output_text;
    // Macro::ExecuteToOutputBuffer @ 0041a020 overwrites the LAST character
    // with a NUL and returns the unmodified count:
    //     output_buffer[output_character_count - 1] = '\0';
    //     return output_character_count;
    // Every field writer appends its own trailing '|' -- `putv` formats "%d|"
    // -- so the separator after the final field is exactly the byte that gets
    // replaced.  `|` therefore reaches a kit as a separator BETWEEN fields and
    // never after the last one.  Returning size() + 1 instead kept that final
    // '|' and added a NUL after it, so every reply the game sends a kit was one
    // byte too long and ended in a spurious separator.
    if (output_buffer.empty()) {
        return 0;
    }
    output_buffer.back() = '\0';
    return output_buffer.size();
}

std::size_t Macro::execute_to_output_buffer(MacroSchedulerHost& host,
                                             std::string& output_buffer) {
    return execute_to_output_buffer(
        static_cast<const MacroExecutionHost&>(host),
        static_cast<MacroInterpreterHost&>(host), output_buffer);
}

bool Macro::start_execution(MacroSchedulerHost& host) {
    reset_execution_state(host);
    const auto running_macro = std::find(g_running_macros.begin(),
                                         g_running_macros.end(), this);
    if (running_macro == g_running_macros.end()) {
        constexpr std::size_t kMaximumRunningMacros = 1000;
        if (g_running_macros.size() >= kMaximumRunningMacros) {
            host.report_too_many_macros(*this, kMaximumRunningMacros);
            return false;
        }
        g_running_macros.push_back(this);
    }

    execute_interpreter(host.interpreter_bindings());
    return true;
}

namespace {

constexpr CaosToken kWindowPosition = caos_token('w', 'p', 'o', 's');
// CMND id#, which posts WM_COMMAND to the main frame.
constexpr CaosToken kCommand = caos_token('c', 'm', 'n', 'd');
constexpr CaosToken kCamera = caos_token('c', 'm', 'r', 'a');
constexpr CaosToken kWorld = caos_token('w', 'r', 'l', 'd');
constexpr CaosToken kGround = caos_token('g', 'r', 'n', 'd');
constexpr CaosToken kWindowFront = caos_token('w', 't', 'o', 'p');
constexpr CaosToken kFollow = caos_token('c', 'a', 'm', 't');
constexpr CaosToken kDirty = caos_token('e', 'd', 'i', 't');
constexpr CaosToken kQuit = caos_token('q', 'u', 'i', 't');
constexpr CaosToken kAbort = caos_token('a', 'b', 'r', 't');

} // namespace

void Macro::execute_system_caos_command(MacroSystemHost& host,
                                        MacroRuntimeHost& runtime) {
    const std::size_t readable_capacity =
        std::min<std::size_t>(script_capacity_bytes, script_buffer.size());
    if (script_cursor_offset + sizeof(CaosToken) + 1 > readable_capacity) {
        execution_terminated = true;
        report_syntax_error(host, "'sys:' command");
        return;
    }

    const CaosToken command = read_next_token();
    switch (command) {
    case kWindowPosition: {
        const int left = static_cast<int>(parse_rvalue(runtime, host));
        const int top = static_cast<int>(parse_rvalue(runtime, host));
        const int width = static_cast<int>(parse_rvalue(runtime, host));
        const int height = static_cast<int>(parse_rvalue(runtime, host));
        host.move_main_window(left, top, left + width, top + height);
        return;
    }
    case kCommand:
        host.send_main_frame_command(parse_rvalue(runtime, host));
        return;
    case kCamera: {
        host.enable_viewport_navigation();
        // The operands must be consumed from the script in source order.  As
        // arguments to one call their evaluation order is unspecified, so the
        // x and y rvalues could be read back to front.
        const int origin_x = static_cast<int>(parse_rvalue(runtime, host));
        const int origin_y = static_cast<int>(parse_rvalue(runtime, host));
        host.set_viewport_origin(origin_x, origin_y);
        return;
    }
    case kWorld:
        host.open_world_file(read_bracketed_text_argument());
        return;
    case kGround: {
        const std::uint32_t x_block = parse_rvalue(runtime, host);
        const std::uint32_t height = parse_rvalue(runtime, host);
        if (x_block <= 0x104u) {
            host.set_ground_height(x_block, height);
        }
        return;
    }
    case kWindowFront:
        host.bring_main_window_to_front();
        return;
    case kFollow:
        host.enable_viewport_navigation();
        host.follow_macro_target(*this);
        return;
    case kDirty: {
        // Native parses left, top, right and bottom in that order; see the
        // kCamera note on argument evaluation order.
        const int left = static_cast<int>(parse_rvalue(runtime, host));
        const int top = static_cast<int>(parse_rvalue(runtime, host));
        const int right = static_cast<int>(parse_rvalue(runtime, host));
        const int bottom = static_cast<int>(parse_rvalue(runtime, host));
        host.set_dirty_world_rect(left, top, right, bottom);
        return;
    }
    case kQuit:
    case kAbort:
        host.send_main_frame_command(0xe141u);
        return;
    default:
        report_syntax_error(host, "'sys:' command");
        return;
    }
}

void Macro::censor_script_profanity() {
    constexpr std::string_view censored = "f**k";
    constexpr std::string_view replacement = "mate";
    std::size_t cursor = script_buffer.find(censored);
    while (cursor != std::string::npos) {
        script_buffer.replace(cursor, censored.size(), replacement);
        cursor = script_buffer.find(censored, cursor + replacement.size());
    }
}

} // namespace creatures1::scripting
