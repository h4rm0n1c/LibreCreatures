#include "windows_egg_transfer.hpp"

#include "windows_macro_host.hpp"

#include "../creatures/egg.hpp"
#include "../objects/simple_object.hpp"
#include "../scripting/macro.hpp"

#include <exception>
#include <memory>
#include <string>

namespace creatures1::platform {
namespace {

// A fresh hatchery egg, as the Hatchery's script makes one
// (c1kit::hatch_script): `new: simp eggs 8 <slot*8> 2000 0`, pose 3 (full
// size), attr 67, a 2400-tick timer, set down at spawn in the kitchen by
// the incubator at x 2408 + 40 * slot, y 870, with the camera on it
// (`sys: cmra 2223 724`).  The slot picks both the egg's picture and its
// spot on the ground.
constexpr int kHatcherySlots = 6;
constexpr std::uint32_t kEggImagesPerSlot = 8;
constexpr std::uint32_t kEggRenderPlane = 2000;
constexpr std::uint32_t kMatureEggPose = 3;
constexpr std::uint32_t kEggAttributes = 67;
constexpr std::int32_t kEggTimer = 2400;
constexpr int kSpawnX = 2408;
constexpr int kSpawnSlotSpacing = 40;
constexpr int kSpawnY = 870;
constexpr int kSpawnCameraX = 2223;
constexpr int kSpawnCameraY = 724;

constexpr char kEggFileFilter[] = "Eggs (*.egg)|*.egg|All files (*.*)|*.*||";

creatures1::creatures::GenomeSex genome_sex(std::uint32_t egg_sex) {
    return egg_sex == 2 ? creatures1::creatures::GenomeSex::female
                        : creatures1::creatures::GenomeSex::male;
}

// A moniker is taken when a creature or another egg in the world uses it,
// or when the world already has a genome file of that name holding other
// genes.  The same genes under the same name (an egg exported from this
// world and brought back) are no clash.
class EggMonikerRegistry final
    : public creatures1::creatures::GenomeFilenameRegistry {
public:
    EggMonikerRegistry(C1WindowsDocument& document,
                       WindowsCreatureInseminationHost& creatures,
                       const creatures1::creatures::Genome& genome)
        : document_(document), creatures_(creatures), genome_(genome) {}

    bool creature_uses_filename(
        creatures1::creatures::GenomeFilenameId id) const override {
        return creatures_.creature_uses_filename(id) ||
               creatures_.family_four_object_uses_filename(id) ||
               egg_uses(id) || file_holds_other_genes(id);
    }
    bool family_four_object_uses_filename(
        creatures1::creatures::GenomeFilenameId) const override {
        return false;  // covered above
    }

private:
    bool egg_uses(creatures1::creatures::GenomeFilenameId id) const {
        for (std::size_t index = 0; index < document_.non_scenery_object_count();
             ++index) {
            const creatures1::objects::Object* object =
                document_.non_scenery_object_at(index);
            if (object != nullptr &&
                creatures1::creatures::Egg::is_egg_classifier(
                    object->classifier_base()) &&
                object->object_variable(0) == id) {
                return true;
            }
        }
        return false;
    }

    bool file_holds_other_genes(
        creatures1::creatures::GenomeFilenameId id) const {
        creatures1::creatures::GenomeFileStore& files = document_.genome_files();
        for (const std::string& path : {files.secondary_genetics_path(id),
                                        files.primary_genetics_path(id)}) {
            if (files.regular_file_exists(path)) {
                return files.read_file(path) != genome_.payload();
            }
        }
        return false;
    }

    C1WindowsDocument& document_;
    WindowsCreatureInseminationHost& creatures_;
    const creatures1::creatures::Genome& genome_;
};

void warn(const char* text) {
    ::AfxMessageBox(text, MB_OK | MB_ICONEXCLAMATION);
}

} // namespace

creatures1::objects::Object* held_egg(C1WindowsDocument& document) {
    // What the hand carries is the one unbounded object besides the hand.
    const creatures1::objects::Object* pointer = document.pointer_tool();
    for (std::size_t index = 0; index < document.non_scenery_object_count();
         ++index) {
        creatures1::objects::Object* object =
            document.non_scenery_object_at(index);
        if (object != nullptr && object != pointer &&
            object->uses_unbounded_world_position() &&
            creatures1::creatures::Egg::is_egg_classifier(
                object->classifier_base())) {
            return object;
        }
    }
    return nullptr;
}

void export_held_egg(C1WindowsDocument& document) {
    creatures1::objects::Object* egg = held_egg(document);
    if (egg == nullptr) {
        return;
    }

    creatures1::creatures::Egg record;
    record.classifier = egg->classifier_base();
    record.sex = egg->object_variable(1);
    record.genome = std::make_unique<creatures1::creatures::Genome>(
        egg->object_variable(0), genome_sex(record.sex),
        creatures1::creatures::GenomeLifeStage::stage_zero,
        &document.genome_files());
    if (record.genome->payload().empty()) {
        warn("This egg's genome is missing from the world's Genetics folder, "
             "so it cannot be exported.");
        return;
    }

    CFileDialog dialog(FALSE, "egg", nullptr,
                       OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR |
                           OFN_NONETWORKBUTTON,
                       kEggFileFilter);
    if (dialog.DoModal() != IDOK) {
        return;
    }

    CFile file;
    CFileException exception;
    if (!file.Open(dialog.GetPathName(),
                   CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive,
                   &exception)) {
        warn("The egg file could not be written.");
        return;
    }
    {
        CArchive archive(&file, CArchive::store);
        C1WindowsDocument::ArchiveHost host(document, archive);
        host.dynamic_objects().write_object_reference(&record, "CEgg");
        archive.Close();
    }
    file.Close();

    // An export moves the egg out of the world, as exporting a creature does.
    // The hand is let go as the egg goes.
    document.delete_world_object(*egg);
}

void import_egg(C1WindowsDocument& document) {
    creatures1::world::WorldRuntime* runtime = document.world_runtime();
    if (runtime == nullptr) {
        return;
    }

    CFileDialog dialog(TRUE, "egg", nullptr,
                       OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
                           OFN_NONETWORKBUTTON,
                       kEggFileFilter);
    if (dialog.DoModal() != IDOK) {
        return;
    }

    std::unique_ptr<creatures1::creatures::Egg> record;
    CFile file;
    CFileException exception;
    if (!file.Open(dialog.GetPathName(), CFile::modeRead | CFile::shareDenyWrite,
                   &exception)) {
        warn("The egg file could not be opened.");
        return;
    }
    try {
        CArchive archive(&file, CArchive::load);
        C1WindowsDocument::ArchiveHost host(document, archive);
        record.reset(static_cast<creatures1::creatures::Egg*>(
            host.dynamic_objects().read_object_reference("CEgg")));
        archive.Close();
    } catch (CException* error) {
        error->Delete();
        record.reset();
    } catch (const std::exception&) {
        record.reset();
    }
    file.Close();
    if (record == nullptr || record->genome == nullptr ||
        record->genome->payload().empty() ||
        !creatures1::creatures::Egg::is_egg_classifier(record->classifier)) {
        warn("That file is not an egg this version can read.");
        return;
    }

    // A clash gets the baby a new moniker: the same genes, a new individual.
    creatures1::creatures::Genome& genome = *record->genome;
    WindowsCreatureInseminationHost creatures(document);
    EggMonikerRegistry monikers(document, creatures, genome);
    if (genome.source_filename() == 0 ||
        monikers.creature_uses_filename(genome.source_filename())) {
        genome.set_source_filename(0);
        creatures1::creatures::generate_unique_genome_filename(
            genome, monikers, creatures);
    }
    genome.save_generated_genome(document.genome_files());

    const int slot = static_cast<int>(creatures.next() % kHatcherySlots);
    WindowsNewObjectHost construction(document);
    auto made = std::make_unique<creatures1::objects::SimpleObject>(
        creatures1::scripting::make_caos_token("eggs"),
        slot * static_cast<int>(kEggImagesPerSlot), kEggImagesPerSlot,
        /*cache_protected=*/false, 0, 0, static_cast<int>(kEggRenderPlane),
        /*bounds_flags=*/0, /*classifier_event=*/0, /*classifier_species=*/0,
        /*classifier_genus=*/0, /*classifier_family=*/2,
        /*click_event_selector_0=*/0xff, /*reserved_word_0=*/0,
        /*reserved_word_1=*/0, /*interaction_event_flags=*/0, construction);
    creatures1::objects::SimpleObject* egg = made.get();
    runtime->adopt_non_scenery_object(std::move(made));

    WindowsEntityImageSequenceRenderHost redraw(document);
    egg->set_relative_image_index(
        static_cast<creatures1::objects::CaosValue>(kMatureEggPose), 0, redraw);
    egg->set_classifier_base(record->classifier);
    egg->set_bounds_flags_for_script(kEggAttributes);
    egg->set_object_variable(0, genome.source_filename());
    egg->set_object_variable(1, record->sex);
    egg->set_timer_for_script(kEggTimer);
    document.move_to_and_redraw(*egg, kSpawnX + slot * kSpawnSlotSpacing,
                                kSpawnY);

    document.request_event_bar_viewport_origin(document.viewport_left(),
                                               document.renderer_viewport_top());
    document.set_renderer_viewport_origin(kSpawnCameraX, kSpawnCameraY);
}

} // namespace creatures1::platform
