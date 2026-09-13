#include "tables.hpp"

namespace creatures1::scripting {

std::array<ScriptDefinitionEntry, kScriptDefinitionCapacity>
    g_script_definition_entries{};
std::size_t g_script_definition_count = 0;

void initialize_script_definition_table() {
    g_script_definition_count = 0;
    for (ScriptDefinitionEntry& entry : g_script_definition_entries) {
        entry.script_text.clear();
        entry.classifier_event = ScriptClassifier{};
    }
}

} // namespace creatures1::scripting
