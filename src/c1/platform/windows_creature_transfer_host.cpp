#include "windows_creature_transfer_host.hpp"

namespace creatures1::platform {
namespace {

// The export archive writes the selected creature as one dynamic object, then
// appends the raw genome payloads the recovered policy selects.
class MfcCreatureExportArchive final
    : public creatures1::application::CreatureExportArchive {
public:
    MfcCreatureExportArchive(C1WindowsDocument& document,
                             std::unique_ptr<CFile> file,
                             std::unique_ptr<CArchive> archive)
        : document_(document),
          file_(std::move(file)),
          archive_(std::move(archive)),
          host_(document, *archive_),
          creature_(document.selected_creature()) {}

    ~MfcCreatureExportArchive() override {
        archive_->Close();
        file_->Close();
    }

    // Native holds the selected creature from the start of the export:
    // RemoveFromWorld deselects it before it is written.
    void write_selected_creature() override {
        creatures1::creatures::Creature* creature = creature_;
        if (creature == nullptr) {
            return;
        }
        // The archive identifies a Creature record by the Skeleton the
        // registry holds, not by the Creature itself.
        host_.dynamic_objects().write_object_reference(&creature->skeleton(),
                                                       "Creature");
    }

    void write_genome(creatures1::creatures::GenomeFilenameId source_filename,
                      creatures1::creatures::GenomeSex sex,
                      creatures1::creatures::GenomeLifeStage life_stage)
        override {
        // OnExportCurrentCreature @ 00431d20 constructs a CGenome from the
        // stored file and writes it with CArchive::WriteObject: a CGenome
        // class record, then CGenome::Serialize @ 004185c0 (payload size,
        // filename, sex, life stage, payload).
        creatures1::creatures::Genome genome(source_filename, sex, life_stage,
                                             &document_.genome_files());
        host_.dynamic_objects().write_object_reference(&genome, "CGenome");
    }

private:
    C1WindowsDocument& document_;
    std::unique_ptr<CFile> file_;
    std::unique_ptr<CArchive> archive_;
    C1WindowsDocument::ArchiveHost host_;
    creatures1::creatures::Creature* creature_ = nullptr;
};

} // namespace

bool WindowsCreatureExportHost::selected_creature_exists() const {
    return document_.selected_creature() != nullptr;
}

const creatures1::application::CreatureExportState*
WindowsCreatureExportHost::selected_creature_for_export() const {
    const creatures1::creatures::Creature* creature =
        document_.selected_creature();
    if (creature == nullptr) {
        return nullptr;
    }
    state_ = {};
    state_.classifier_family =
        creature->skeleton().classifier_base() & 0xff000000u;
    state_.genome_source_filename = creature->skeleton().genome_source_filename;
    state_.genome_sex =
        creature->gender() == creatures1::creatures::CreatureGender::female
            ? creatures1::creatures::GenomeSex::female
            : creatures1::creatures::GenomeSex::male;
    state_.genome_life_stage = creature->genome_life_stage();
    state_.child_genome_source_filename =
        creature->child_genome_source_filename();
    return &state_;
}

bool WindowsCreatureExportHost::prompt_for_export_path(
    std::string& output_path) {
    // PromptForExpFilePath @ 0x00417d70 passes 0x2000e: OFN_NOCHANGEDIR keeps
    // the dialog from moving the process's current directory away from the
    // Main Directory that relative paths are resolved against.
    CFileDialog dialog(FALSE, "exp", nullptr,
                       OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR |
                           OFN_NONETWORKBUTTON,
                       "Exported creatures (*.exp)|*.exp|"
                       "All files (*.*)|*.*||");
    if (dialog.DoModal() != IDOK) {
        return false;
    }
    output_path = dialog.GetPathName().GetString();
    return true;
}

void WindowsCreatureExportHost::log_export_started() {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x1,
                                      "Exporting selected creature\n");
    }
}

std::unique_ptr<creatures1::application::CreatureExportArchive>
WindowsCreatureExportHost::begin_export_archive(
    std::string_view output_path) {
    const std::string path(output_path);
    auto file = std::make_unique<CFile>();
    CFileException exception;
    if (!file->Open(path.c_str(),
                    CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive,
                    &exception)) {
        return nullptr;
    }
    auto archive =
        std::make_unique<CArchive>(file.get(), CArchive::store);
    return std::make_unique<MfcCreatureExportArchive>(
        document_, std::move(file), std::move(archive));
}

void WindowsCreatureExportHost::clear_selected_creature_references() {
    // OnExportCurrentCreature runs vtable slot 24, Creature::RemoveFromWorld
    // (0040e0d0), before writing: carried objects are let go, the creature
    // leaves the selection and the score, so the file holds none of that.
    exported_ = document_.selected_creature();
    if (exported_ != nullptr) {
        WindowsCreatureRemovalHost removal(document_);
        exported_->remove_from_world(removal);
    }
}

void WindowsCreatureExportHost::restore_selected_creature_runtime_state() {
    // After writing, slot 16 deletes the creature: an export moves it out of
    // the world rather than copying it.
    if (exported_ != nullptr) {
        document_.delete_creature(*exported_);
        exported_ = nullptr;
    }
}

void WindowsCreatureExportHost::log_child_genome_export() {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x1,
                                      "Exporting child genome\n");
    }
}


// --- CreatureDeserializationHost -------------------------------------------

std::unique_ptr<creatures1::creatures::Genome>
WindowsCreatureDeserializationHost::read_genome_reference(
    creatures1::creatures::CreatureArchive& archive) {
    // Creature::Deserialize @ 0040dda0 reads each genome with
    // CArchive::ReadObject(CGenome), so the class record comes first.
    return std::unique_ptr<creatures1::creatures::Genome>(
        static_cast<creatures1::creatures::Genome*>(
            archive.read_object_reference("CGenome")));
}

void WindowsCreatureDeserializationHost::ensure_unique_primary_genome_filename(
    creatures1::creatures::Genome& genome,
    const creatures1::creatures::Creature& current_creature) {
    // The native scan excludes the creature being restored, so a genome that
    // already belongs to it is not treated as a collision.
    for (std::size_t index = 0; index < document_.creature_count(); ++index) {
        const auto* other = dynamic_cast<const creatures1::creatures::Creature*>(
            document_.creature_at(index));
        if (other == nullptr || other == &current_creature) {
            continue;
        }
        if (other->skeleton().genome_source_filename ==
            genome.source_filename()) {
            genome.set_source_filename(0);
            creatures1::creatures::generate_unique_genome_filename(
                genome, genomes_, genomes_);
            return;
        }
    }
}

void WindowsCreatureDeserializationHost::ensure_unique_child_genome_filename(
    creatures1::creatures::Genome& genome,
    const creatures1::creatures::Creature& current_creature) {
    // The child scan has its own native loop and deliberately does not
    // inherit the primary scan's exclusion.
    static_cast<void>(current_creature);
    for (std::size_t index = 0; index < document_.creature_count(); ++index) {
        const auto* other = dynamic_cast<const creatures1::creatures::Creature*>(
            document_.creature_at(index));
        if (other == nullptr) {
            continue;
        }
        if (other->child_genome_source_filename() ==
            genome.source_filename()) {
            genome.set_source_filename(0);
            creatures1::creatures::generate_unique_genome_filename(
                genome, genomes_, genomes_);
            return;
        }
    }
}

void WindowsCreatureDeserializationHost::save_generated_genome(
    const creatures1::creatures::Genome& genome) {
    genome.save_generated_genome(document_.genome_files());
}

bool WindowsCreatureDeserializationHost::load_materialized_genome(
    creatures1::creatures::Creature& creature,
    creatures1::creatures::Genome& genome) {
    // Creature::initialize_from_genome rebuilds the Skeleton sprites, voice
    // and biochemistry from the creature's own stored filename, so the
    // materialised genome's filename is adopted first.
    creature.skeleton().genome_source_filename = genome.source_filename();
    creature.initialize_from_genome(construction_);
    return true;
}

std::string WindowsCreatureDeserializationHost::format_moniker(
    std::uint32_t value) const {
    return document_.format_moniker(value);
}

void WindowsCreatureDeserializationHost::set_unbounded_bounds_and_update(
    creatures1::creatures::Creature& creature) {
    creatures1::objects::Object& object = creature.skeleton();
    object.set_bounds_mode(
        static_cast<std::uint32_t>(
            creatures1::objects::Object::BoundsMode::unbounded_1),
        document_.renderables());
    object.update_movement_bounds(document_);
}

void WindowsCreatureDeserializationHost::move_to_and_redraw(
    creatures1::creatures::Creature& creature, int world_x, int world_y) {
    document_.move_to_and_redraw(creature.skeleton(), world_x, world_y);
}

void WindowsCreatureDeserializationHost::set_default_bounds_and_update(
    creatures1::creatures::Creature& creature) {
    creatures1::objects::Object& object = creature.skeleton();
    object.set_bounds_mode(
        static_cast<std::uint32_t>(
            creatures1::objects::Object::BoundsMode::default_world),
        document_.renderables());
    object.update_movement_bounds(document_);
}

void WindowsCreatureDeserializationHost::select_loaded_creature(
    creatures1::creatures::Creature& creature) {
    // Creature::Deserialize @ 0040dda0's own `if (this != g_selected_creature)`
    // block is the exact selection-change sequence apply_selected_creature
    // already implements for menu selection and next/previous navigation:
    // broadcast 6 (or 8 if null), title, eye view, a conditional second
    // broadcast 8 if the new selection is dead, then viewport/event-bar/
    // toolbar refresh.  A bare field set skipped every one of those, so an
    // imported creature never told any open kit it had become selected.
    creatures1::application::apply_selected_creature(document_, &creature,
                                                      true);
}

void WindowsCreatureDeserializationHost::rebuild_creature_selection_menu() {
    document_.rebuild_creature_selection_menu();
}

void WindowsCreatureDeserializationHost::increment_living_norns() {
    document_.increment_living_norn_score();
}

void WindowsCreatureDeserializationHost::notify_creature_loaded_to_embedded_kits() {
    // Creature::Deserialize's tail pings embedded record slot 8 with the
    // same "Dummy" VT_I4 OLE dispatch SFCDoc::UpdateWorld's periodic score
    // notification and Creature::RemoveFromWorld/RemoveObjectFromWorld use --
    // it is not the twenty-slot BroadcastEmbeddedControlState call at all,
    // and code 0 is not a real state either path ever sends.
    document_.publish_periodic_score_to_embedded_control({});
}

// --- CreatureImportHost -----------------------------------------------------

namespace {

class MfcCreatureImportArchive final
    : public creatures1::application::CreatureImportArchive {
public:
    MfcCreatureImportArchive(C1WindowsDocument& document,
                             std::unique_ptr<CFile> file,
                             std::unique_ptr<CArchive> archive)
        : document_(document),
          file_(std::move(file)),
          archive_(std::move(archive)),
          host_(document, *archive_) {}

    ~MfcCreatureImportArchive() override {
        archive_->Close();
        file_->Close();
    }

    creatures1::creatures::Creature* read_creature() override {
        void* identity =
            host_.dynamic_objects().read_object_reference("Creature");
        return identity == nullptr
                   ? nullptr
                   : document_.mutable_creature_for_object(
                         *static_cast<creatures1::objects::Object*>(identity));
    }

    void dispatch_after_bounds_update(
        creatures1::creatures::Creature& creature) override {
        creature.skeleton().update_movement_bounds(document_);
    }

    bool is_family_four(
        const creatures1::creatures::Creature& creature) const override {
        return ((creature.skeleton().classifier_base() >> 24) & 0xff) == 4;
    }

    void deserialize_creature(
        creatures1::creatures::Creature& creature) override {
        creatures1::platform::MfcArchiveStream stream(*archive_);
        creatures1::platform::MfcObjectArchive objects(
            stream,
            [this](std::string_view name) {
                return host_.dynamic_objects().read_object_reference(name);
            },
            [this](const void* object, std::string_view name) {
                host_.dynamic_objects().write_object_reference(object, name);
            });
        // An imported creature's scripts are installed like a loaded world's.
        objects.set_script_install_host(&document_);
        creatures1::platform::MfcCreatureArchive creature_archive(objects);
        WindowsCreatureDeserializationHost deserialization(document_);
        creature.deserialize(creature_archive, deserialization);
    }

private:
    C1WindowsDocument& document_;
    std::unique_ptr<CFile> file_;
    std::unique_ptr<CArchive> archive_;
    C1WindowsDocument::ArchiveHost host_;
};

} // namespace

bool WindowsCreatureImportHost::prompt_for_import_path(std::string& path) {
    // PromptForExpFilePath @ 0x00417d70 passes 0x2000e: OFN_NOCHANGEDIR keeps
    // the dialog from moving the process's current directory away from the
    // Main Directory that relative paths are resolved against.
    CFileDialog dialog(TRUE, "exp", nullptr,
                       OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
                           OFN_NONETWORKBUTTON,
                       "Exported creatures (*.exp)|*.exp|"
                       "All files (*.*)|*.*||");
    if (dialog.DoModal() != IDOK) {
        return false;
    }
    path = dialog.GetPathName().GetString();
    return true;
}

void WindowsCreatureImportHost::log_import_started() {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x1, "Importing creature\n");
    }
}

std::unique_ptr<creatures1::application::CreatureImportArchive>
WindowsCreatureImportHost::begin_import_archive(std::string_view path) {
    const std::string file_path(path);
    auto file = std::make_unique<CFile>();
    CFileException exception;
    if (!file->Open(file_path.c_str(),
                    CFile::modeRead | CFile::shareDenyWrite, &exception)) {
        return nullptr;
    }
    auto archive = std::make_unique<CArchive>(file.get(), CArchive::load);
    return std::make_unique<MfcCreatureImportArchive>(
        document_, std::move(file), std::move(archive));
}

void WindowsCreatureImportHost::remove_import_file(std::string_view path) {
    // The recovered import consumes the file so a creature cannot be imported
    // twice from the same export.
    const std::string file_path(path);
    ::DeleteFileA(file_path.c_str());
}

} // namespace creatures1::platform
