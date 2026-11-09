// The object variables: Macro ParseRValue @0041a9d0 against LC's
// Macro::parse_rvalue, on the CAOS r-values obv0, obv1 and obv2.
//
//   rvalue_equivalence.exe <Creatures.exe> [cases]
//
// The original's path for "obvN" (0041aab4..0041ab07) reads the four
// characters at the cursor (Macro +0x10; the script starts at +0xc, its
// length is +0x14), and for "obv" with N in 0..2 returns the dword at
// TARG (Macro +0xa8) + 0x34 + 4 * N.  LC runs its own interpreter on the
// same text with an Object whose object variables hold the same values.
// The same result for every N proves that the original keeps object
// variable N at Object +0x34 + 4 * N, which the load test's table uses
// (src/c1/creatures/load_dump_objects.inc).  The original's Macro and
// Object here are byte blocks; no import of the original is called.
#include "native_image.hpp"

#include "objects/object.hpp"
#include "scripting/classifier_scripts.hpp"
#include "scripting/macro.hpp"
#include "scripting/tables.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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

// A copy of the null runtime of tests/c1_macro_interpreter_trace_test.cpp,
// except that the test's target object is live.
class TargetRuntime : public MacroRuntimeHost {
public:
    // The one object the test passes as TARG is live; nothing else is.
    const Object* live = nullptr;
    bool is_live_object(const Object* object) const override { return object != nullptr && object == live; }
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


constexpr std::uint32_t kParseRValue = 0x0041a9d0;
constexpr std::size_t kMacroSize = 0x400, kScriptStart = 0xc, kScriptCursor = 0x10,
                      kScriptLength = 0x14, kTarget = 0xa8;
constexpr std::size_t kObjectSize = 0x200, kObjectVariables = 0x34;
using NativeParseRValue = std::uint32_t(__attribute__((thiscall)) *)(void*);

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: rvalue_equivalence <Creatures.exe> [cases]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 100000;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    Random random{0x9e3779b97f4a7c15ull};
    long differing = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> native_macro(kMacroSize, 0), native_object(kObjectSize, 0);
        Object lc_object;
        for (std::size_t v = 0; v < 3; ++v) {
            const std::uint32_t value = random.next() ^ (random.next() << 16);
            lc_object.set_object_variable(v, value);
            field<std::uint32_t>(native_object.data(), kObjectVariables + 4 * v) = value;
        }
        const unsigned which = random.next() % 3;
        char text[8];
        std::snprintf(text, sizeof text, "obv%u ", which);
        std::vector<char> script(text, text + std::strlen(text));
        field<char*>(native_macro.data(), kScriptStart) = script.data();
        field<char*>(native_macro.data(), kScriptCursor) = script.data();
        field<std::int32_t>(native_macro.data(), kScriptLength) = static_cast<std::int32_t>(script.size());
        field<void*>(native_macro.data(), kTarget) = native_object.data();
        const std::uint32_t native_value =
            reinterpret_cast<NativeParseRValue>(image.at<void*>(kParseRValue))(native_macro.data());

        TargetRuntime runtime;
        runtime.live = &lc_object;
        NullDiagnostics diagnostics;
        Macro macro;
        macro.load_script_text(text);
        macro.object_context.target_object = &lc_object;
        const std::uint32_t lc_value = macro.parse_rvalue(runtime, diagnostics);
        if (native_value != lc_value) {
            if (++differing <= 10) {
                std::printf("case %ld %s: native %08x lc %08x\n", index, text,
                            static_cast<unsigned>(native_value), static_cast<unsigned>(lc_value));
            }
        }
    }
    std::printf("%ld cases (obv0, obv1, obv2), differing: %ld -> %s\n", cases, differing,
                differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
