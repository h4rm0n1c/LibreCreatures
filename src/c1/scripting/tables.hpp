#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace creatures1::scripting {

enum class ScriptEvent : std::uint8_t {
    deactivate = 0,
    timer = 9,
};

struct ScriptClassifier {
    ScriptEvent event = ScriptEvent::deactivate;
    std::uint8_t species = 0;
    std::uint8_t genus = 0;
    std::uint8_t family = 0;
};

struct ScriptDefinitionEntry {
    std::string script_text;
    ScriptClassifier classifier_event{};
};

inline constexpr std::size_t kScriptDefinitionCapacity = 2000;

extern std::array<ScriptDefinitionEntry, kScriptDefinitionCapacity>
    g_script_definition_entries;
extern std::size_t g_script_definition_count;

// Initializes the fixed-capacity script-definition registry to its inactive
// state. Script source is owned by each table entry.
void initialize_script_definition_table();

} // namespace creatures1::scripting
