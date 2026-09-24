#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "tables.hpp"

namespace creatures1::objects {
class Object;
}

namespace creatures1::scripting {

class Macro;

class ScriptArchiveWriter {
public:
    virtual ~ScriptArchiveWriter() = default;

    virtual void write_script_count(std::uint32_t count) = 0;
    virtual void write_classifier(ScriptClassifier classifier) = 0;
    virtual void write_script_text(std::string_view text) = 0;
};

// Archive buffering and MFC error policy stay in the archive adapter. The
// deserializer owns only the recovered load-mode check and record order.
class ScriptArchiveReader {
public:
    virtual ~ScriptArchiveReader() = default;
    virtual bool is_loading() const = 0;
    virtual std::int32_t read_script_count() = 0;
    virtual ScriptClassifier read_classifier() = 0;
    virtual std::string read_script_text() = 0;
};

// Script-table mutation, including owned text storage and any replacement
// policy, belongs to the scripting registry owner.
class ScriptDefinitionInstallHost {
public:
    virtual ~ScriptDefinitionInstallHost() = default;
    virtual void report_archive_not_loading() = 0;
    virtual bool confirm_script_replacement(ScriptClassifier classifier,
                                            std::string_view old_text,
                                            std::string_view new_text) = 0;
    virtual void report_script_table_full() = 0;
};

// The CAOS runtime owns logging, running-macro storage, selected-object
// policy, allocation, and interpreter scheduling.  The classifier resolver
// below owns only the recovered lookup and invocation state transitions.
class ScriptExecutionHost {
public:
    virtual ~ScriptExecutionHost() = default;
    virtual void report_script_counts_if_changed(std::size_t stored_count,
                                                 std::size_t running_count) = 0;
    virtual Macro* find_running_macro_for_owner(
        objects::Object* script_owner) const = 0;
    virtual Macro* create_initialized_macro() = 0;
    virtual objects::Object* selected_creature() const = 0;
    virtual void start_macro_execution(Macro& macro) = 0;
    virtual void report_missing_script(ScriptClassifier classifier) = 0;
};

// Installs or replaces one owned source record. Replacement confirmation,
// table-full reporting, and their MFC/UI effects stay in the host adapter.
bool install_script_text_for_classifier(ScriptClassifier classifier,
                                        std::string_view text,
                                        bool confirm_replace,
                                        ScriptDefinitionInstallHost& policy);

// Resolves and launches one script. Runtime/UI/allocation details are
// supplied by ScriptExecutionHost; no executable-address or raw-token
// knowledge belongs in this source-level function.
// `scrx` @ Macro.cpp token 0x78726373 removes the FIRST entry whose complete
// four-byte classifier matches, releases its text, shifts the tail down one
// slot, and decrements the count.  It stops at the first match; a duplicate
// classifier would survive, which the install path makes unreachable but the
// removal does not itself guarantee.  Returns whether an entry was removed.
bool remove_script_definition_for_classifier(ScriptClassifier classifier);

bool execute_script_for_classifier(objects::Object* script_owner,
                                   objects::Object* from_object,
                                   ScriptClassifier classifier,
                                   bool force_restart,
                                   ScriptExecutionHost& runtime);

// Events held back by execute_script_for_classifier because the owner's
// script was paused by a yielding prefixed command.  The scheduler applies
// an owner's held event straight after that owner's script has had its turn.
void apply_deferred_script_events(objects::Object* script_owner,
                                  ScriptExecutionHost& runtime);
// Object deletion and kill: drop events owned by the object and clear it as
// the FROM of any other held event.
void forget_deferred_script_events(const objects::Object* object);
// CanBeDestroyed must not free an object a held event still names.
bool deferred_script_events_reference(const objects::Object* object);
// World reset, with the running Macros.
void clear_deferred_script_events();

// Writes the definitions belonging to one family/genus/species classifier.
// The event byte selects the script record but is deliberately ignored by
// the classifier filter, matching the packed four-byte record semantics.
void serialize_scripts_for_classifier(ScriptArchiveWriter& archive,
                                      ScriptClassifier classifier);

// SFCDoc::Serialize persists the complete fixed-capacity script table in
// insertion order. This is distinct from the CAOS lookup helper above, which
// intentionally filters one classifier family for runtime dispatch.
void serialize_all_scripts(ScriptArchiveWriter& archive);

void deserialize_scripts_for_classifier(ScriptArchiveReader& archive,
                                        ScriptDefinitionInstallHost& policy);

void deserialize_all_scripts(ScriptArchiveReader& archive,
                             ScriptDefinitionInstallHost& policy);

} // namespace creatures1::scripting
