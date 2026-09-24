#include "classifier_scripts.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#include "macro.hpp"

namespace creatures1::scripting {
namespace {

bool classifier_matches(ScriptClassifier entry, ScriptClassifier query) {
    return entry.family == query.family && entry.genus == query.genus &&
           entry.species == query.species;
}

bool classifiers_equal(ScriptClassifier left, ScriptClassifier right) {
    return left.event == right.event && left.species == right.species &&
           left.genus == right.genus && left.family == right.family;
}

struct DeferredScriptEvent {
    objects::Object* script_owner = nullptr;
    objects::Object* from_object = nullptr;
    ScriptClassifier classifier{};
};

// At most one per owner: native would have let each later event overwrite
// the earlier one, so the latest wins.
std::vector<DeferredScriptEvent> g_deferred_script_events;

void defer_script_event(objects::Object* script_owner,
                        objects::Object* from_object,
                        ScriptClassifier classifier) {
    for (DeferredScriptEvent& event : g_deferred_script_events) {
        if (event.script_owner == script_owner) {
            event = {script_owner, from_object, classifier};
            return;
        }
    }
    g_deferred_script_events.push_back({script_owner, from_object, classifier});
}

bool matches_species_wildcard(ScriptClassifier entry, ScriptClassifier query) {
    // Native compares the entire stored classifier to query & 0xffff00ff.
    // The cleared byte is species, not genus; only an explicit zero matches.
    return entry.event == query.event && entry.species == 0 &&
           entry.genus == query.genus && entry.family == query.family;
}

bool matches_species_genus_wildcard(ScriptClassifier entry,
                                    ScriptClassifier query) {
    return entry.event == query.event && entry.species == 0 &&
           entry.genus == 0 && entry.family == query.family;
}

} // namespace

bool remove_script_definition_for_classifier(ScriptClassifier classifier) {
    const std::size_t count =
        g_script_definition_count < kScriptDefinitionCapacity
            ? g_script_definition_count
            : kScriptDefinitionCapacity;
    for (std::size_t index = 0; index < count; ++index) {
        if (!classifiers_equal(
                g_script_definition_entries[index].classifier_event,
                classifier)) {
            continue;
        }
        for (std::size_t shift = index + 1; shift < count; ++shift) {
            g_script_definition_entries[shift - 1] =
                std::move(g_script_definition_entries[shift]);
        }
        g_script_definition_entries[count - 1] = ScriptDefinitionEntry{};
        g_script_definition_count = count - 1;
        return true;
    }
    return false;
}

bool install_script_text_for_classifier(
    ScriptClassifier classifier, std::string_view text, bool confirm_replace,
    ScriptDefinitionInstallHost& policy) {
    const std::size_t count =
        g_script_definition_count < kScriptDefinitionCapacity
            ? g_script_definition_count
            : kScriptDefinitionCapacity;

    for (std::size_t index = 0; index < count; ++index) {
        ScriptDefinitionEntry& entry = g_script_definition_entries[index];
        if (!classifiers_equal(entry.classifier_event, classifier)) {
            continue;
        }

        if (confirm_replace && entry.script_text != text &&
            !policy.confirm_script_replacement(classifier, entry.script_text,
                                               text)) {
            return false;
        }
        entry.script_text.assign(text);
        return true;
    }

    if (count >= kScriptDefinitionCapacity) {
        policy.report_script_table_full();
        return false;
    }

    ScriptDefinitionEntry& entry = g_script_definition_entries[count];
    entry.classifier_event = classifier;
    entry.script_text.assign(text);
    g_script_definition_count = count + 1;
    return true;
}

bool execute_script_for_classifier(objects::Object* script_owner,
                                   objects::Object* from_object,
                                   ScriptClassifier classifier,
                                   bool force_restart,
                                   ScriptExecutionHost& runtime) {
    runtime.report_script_counts_if_changed(g_script_definition_count,
                                            g_running_macros.size());

    const std::size_t count =
        g_script_definition_count < kScriptDefinitionCapacity
            ? g_script_definition_count
            : kScriptDefinitionCapacity;
    std::size_t exact_match = count;
    std::size_t genus_wildcard_match = count;
    std::size_t species_genus_wildcard_match = count;

    for (std::size_t index = 0; index < count; ++index) {
        const ScriptClassifier stored =
            g_script_definition_entries[index].classifier_event;
        if (classifiers_equal(stored, classifier)) {
            exact_match = index;
            break;
        }
        if (matches_species_wildcard(stored, classifier)) {
            genus_wildcard_match = index;
        }
        if (matches_species_genus_wildcard(stored, classifier)) {
            species_genus_wildcard_match = index;
        }
    }

    std::size_t selected_definition = exact_match;
    if (selected_definition == count) {
        selected_definition = genus_wildcard_match;
    }
    if (selected_definition == count) {
        selected_definition = species_genus_wildcard_match;
    }
    if (selected_definition == count) {
        runtime.report_missing_script(classifier);
        return false;
    }

    Macro* macro = runtime.find_running_macro_for_owner(script_owner);
    if (macro != nullptr && classifier.event == ScriptEvent::timer) {
        return true;
    }
    if (macro != nullptr && script_owner != nullptr && !force_restart &&
        macro->paused_by_prefixed_command) {
        // LibreCreatures deviation.  Native loads the new script into the
        // owner's running Macro here, whatever point that script has reached.
        // A script paused only because new:/sys:/dde:/app: ended its turn
        // loses the commands after it -- which is how an egg-laying norn
        // leaves her egg at the world origin, the "egg in the sky".  Hold the
        // event until the paused script has finished that turn instead.
        defer_script_event(script_owner, from_object, classifier);
        return true;
    }
    if (macro == nullptr || force_restart) {
        macro = runtime.create_initialized_macro();
        if (macro == nullptr) {
            return false;
        }
        // Native constructs every object-script macro with
        // destroy_when_finished set.  That flag is what lets
        // PurgeDestroyWhenFinishedMacrosForOwner stop an object's scripts when
        // the object is killed, dies or is deleted.
        macro->destroy_when_finished = true;
    }

    macro->object_context.script_owner =
        script_owner == nullptr ? runtime.selected_creature() : script_owner;
    macro->object_context.from_object = from_object;
    macro->load_script_text(
        g_script_definition_entries[selected_definition].script_text);
    runtime.start_macro_execution(*macro);
    return true;
}

void serialize_scripts_for_classifier(ScriptArchiveWriter& archive,
                                      ScriptClassifier classifier) {
    const std::size_t count =
        g_script_definition_count < kScriptDefinitionCapacity
            ? g_script_definition_count
            : kScriptDefinitionCapacity;

    std::uint32_t matching_count = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (classifier_matches(g_script_definition_entries[index].classifier_event,
                               classifier)) {
            ++matching_count;
        }
    }
    archive.write_script_count(matching_count);

    for (std::size_t index = 0; index < count; ++index) {
        const ScriptDefinitionEntry& entry = g_script_definition_entries[index];
        if (!classifier_matches(entry.classifier_event, classifier)) {
            continue;
        }
        archive.write_classifier(entry.classifier_event);
        archive.write_script_text(entry.script_text);
    }
}

void serialize_all_scripts(ScriptArchiveWriter& archive) {
    const std::size_t count =
        g_script_definition_count < kScriptDefinitionCapacity
            ? g_script_definition_count
            : kScriptDefinitionCapacity;
    archive.write_script_count(static_cast<std::uint32_t>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const ScriptDefinitionEntry& entry = g_script_definition_entries[index];
        archive.write_classifier(entry.classifier_event);
        archive.write_script_text(entry.script_text);
    }
}

void deserialize_scripts_for_classifier(
    ScriptArchiveReader& archive, ScriptDefinitionInstallHost& policy) {
    if (!archive.is_loading()) {
        policy.report_archive_not_loading();
        return;
    }

    const std::int32_t script_count = archive.read_script_count();
    for (std::int32_t index = 0; index < script_count; ++index) {
        const ScriptClassifier classifier = archive.read_classifier();
        const std::string script_text = archive.read_script_text();
        install_script_text_for_classifier(classifier, script_text, false,
                                           policy);
    }
}

void deserialize_all_scripts(ScriptArchiveReader& archive,
                             ScriptDefinitionInstallHost& policy) {
    if (!archive.is_loading()) {
        policy.report_archive_not_loading();
        return;
    }

    const std::int32_t script_count = archive.read_script_count();
    if (script_count < 0) {
        return;
    }
    for (std::int32_t index = 0; index < script_count; ++index) {
        const ScriptClassifier classifier = archive.read_classifier();
        const std::string script_text = archive.read_script_text();
        install_script_text_for_classifier(classifier, script_text, false,
                                           policy);
    }
}

void apply_deferred_script_events(objects::Object* script_owner,
                                  ScriptExecutionHost& runtime) {
    if (script_owner == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < g_deferred_script_events.size();
         ++index) {
        if (g_deferred_script_events[index].script_owner != script_owner) {
            continue;
        }
        const DeferredScriptEvent event = g_deferred_script_events[index];
        g_deferred_script_events.erase(g_deferred_script_events.begin() +
                                       static_cast<std::ptrdiff_t>(index));
        // If the owner's script paused on another prefixed command this
        // turn, this defers again and is applied after its next turn.
        execute_script_for_classifier(event.script_owner, event.from_object,
                                      event.classifier, false, runtime);
        return;
    }
}

void forget_deferred_script_events(const objects::Object* object) {
    for (std::size_t index = 0; index < g_deferred_script_events.size();) {
        DeferredScriptEvent& event = g_deferred_script_events[index];
        if (event.script_owner == object) {
            g_deferred_script_events.erase(g_deferred_script_events.begin() +
                                           static_cast<std::ptrdiff_t>(index));
            continue;
        }
        if (event.from_object == object) {
            event.from_object = nullptr;
        }
        ++index;
    }
}

bool deferred_script_events_reference(const objects::Object* object) {
    for (const DeferredScriptEvent& event : g_deferred_script_events) {
        if (event.script_owner == object || event.from_object == object) {
            return true;
        }
    }
    return false;
}

void clear_deferred_script_events() {
    g_deferred_script_events.clear();
}

} // namespace creatures1::scripting
