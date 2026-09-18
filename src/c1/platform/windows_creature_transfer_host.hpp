#pragma once

#include "windows_macro_host.hpp"
#include "windows_shell.hpp"

namespace creatures1::platform {

// CreatureExportHost.  The recovered policy is already translated in
// application/application.cpp; it needs a CFile/CArchive pair and the
// document's existing dynamic object table, which is the same machinery the
// world save uses.  Genome payloads travel through the GenomeFileStore the
// document already owns.

class WindowsCreatureExportHost final
    : public creatures1::application::CreatureExportHost {
public:
    explicit WindowsCreatureExportHost(C1WindowsDocument& document)
        : document_(document) {}

    bool selected_creature_exists() const override;
    const creatures1::application::CreatureExportState*
    selected_creature_for_export() const override;
    bool prompt_for_export_path(std::string& output_path) override;
    void log_export_started() override;
    std::unique_ptr<creatures1::application::CreatureExportArchive>
    begin_export_archive(std::string_view output_path) override;
    void clear_selected_creature_references() override;
    void restore_selected_creature_runtime_state() override;
    void log_child_genome_export() override;

private:
    C1WindowsDocument& document_;
    mutable creatures1::application::CreatureExportState state_{};
    creatures1::creatures::Creature* exported_ = nullptr;
};

// CreatureDeserializationHost.  Every service it needs already exists: the
// genome payload travels through Genome::serialize over a small adapter, the
// unique-filename policy is the translated generate_unique_genome_filename,
// and materialisation is Creature::initialize_from_genome driven by the
// construction host built for the Testing menu's norn commands.
class WindowsCreatureDeserializationHost final
    : public creatures1::creatures::CreatureDeserializationHost {
public:
    explicit WindowsCreatureDeserializationHost(C1WindowsDocument& document)
        : document_(document), construction_(document), genomes_(document) {}

    std::unique_ptr<creatures1::creatures::Genome> read_genome_reference(
        creatures1::creatures::CreatureArchive& archive) override;
    void ensure_unique_primary_genome_filename(
        creatures1::creatures::Genome& genome,
        const creatures1::creatures::Creature& current_creature) override;
    void ensure_unique_child_genome_filename(
        creatures1::creatures::Genome& genome,
        const creatures1::creatures::Creature& current_creature) override;
    void save_generated_genome(
        const creatures1::creatures::Genome& genome) override;
    bool load_materialized_genome(
        creatures1::creatures::Creature& creature,
        creatures1::creatures::Genome& genome) override;
    void set_unbounded_bounds_and_update(
        creatures1::creatures::Creature& creature) override;
    void move_to_and_redraw(creatures1::creatures::Creature& creature,
                            int world_x, int world_y) override;
    void set_default_bounds_and_update(
        creatures1::creatures::Creature& creature) override;
    void select_loaded_creature(
        creatures1::creatures::Creature& creature) override;
    void rebuild_creature_selection_menu() override;
    void increment_living_norns() override;
    void notify_creature_loaded_to_embedded_kits() override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureConstructionHost construction_;
    WindowsCreatureInseminationHost genomes_;
};

class WindowsCreatureImportHost final
    : public creatures1::application::CreatureImportHost {
public:
    explicit WindowsCreatureImportHost(C1WindowsDocument& document)
        : document_(document) {}

    bool prompt_for_import_path(std::string& path) override;
    void log_import_started() override;
    std::unique_ptr<creatures1::application::CreatureImportArchive>
    begin_import_archive(std::string_view path) override;
    void remove_import_file(std::string_view path) override;

private:
    C1WindowsDocument& document_;
};

} // namespace creatures1::platform
