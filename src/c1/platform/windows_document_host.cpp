#include "windows_creature_hosts.hpp"
#include "windows_object_event_host.hpp"
#include "windows_pointer_tool_host.hpp"
#include "windows_macro_host.hpp"
#include "windows_shell.hpp"
#include "windows_embedded_kit_host.hpp"

#include <limits>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace creatures1::platform {

namespace {

void log_world_save_failure(const char* path, const char* reason) {
    if (FILE* log = std::fopen("Creatures.save.log", "a")) {
        std::fprintf(log, "C1 save failed: path=%s reason=%s\n",
                     path == nullptr ? "(null)" : path, reason);
        std::fclose(log);
    }
}

// Opt-in step probe.  Travel in C1 comes from exactly one comparison in
// Skeleton::UpdateAnchorAndBounds @0043b950: when the swinging leg's endpoint
// passes below movement_bounds.max_y the down foot switches to that leg and
// the body re-anchors there.  If that never holds, the creature cycles poses
// on the spot.  Log both sides of it, plus the bounds state that produces the
// floor, so an unreachable pose, a wrong floor and a stalled macro are
// distinguishable from one run.  Enable with C1_TRACE_CREATURE=<moniker hex>.
bool creature_trace_disabled() {
    // Enabled by default.  Setting an environment variable is awkward on
    // Windows, and a diagnostic nobody can switch on is a diagnostic that
    // never gets used.  C1_TRACE_CREATURE=0 (or "off") turns it off; a
    // moniker in hex narrows the motor trace to one creature.
    static const char* setting = std::getenv("C1_TRACE_CREATURE");
    return setting != nullptr &&
           (std::strcmp(setting, "0") == 0 || std::strcmp(setting, "off") == 0);
}

void log_creature_step_probe(const creatures1::creatures::Creature& creature,
                             std::uint32_t world_tick) {
    if (creature_trace_disabled()) {
        return;
    }
    static const char* trace_moniker = std::getenv("C1_TRACE_CREATURE");
    const auto& motor = creature.skeleton();
    // Unset or "*" traces every creature.  A shared row budget lets creatures already
    // in the save consume the whole trace before a newborn ever ticks, which
    // is exactly why earlier log tails could not establish newborn state, so
    // budget each moniker separately.
    const bool trace_all = trace_moniker == nullptr || trace_moniker[0] == '*';
    if (!trace_all && motor.genome_source_filename !=
                          std::strtoul(trace_moniker, nullptr, 16)) {
        return;
    }

    constexpr std::size_t kTracedCreatureLimit = 16;
    constexpr std::size_t kRowsPerCreature = 4000;
    static std::uint32_t traced_monikers[kTracedCreatureLimit]{};
    static std::size_t traced_rows[kTracedCreatureLimit]{};
    static std::size_t traced_count = 0;

    std::size_t slot = 0;
    while (slot < traced_count &&
           traced_monikers[slot] != motor.genome_source_filename) {
        ++slot;
    }
    if (slot == traced_count) {
        if (traced_count == kTracedCreatureLimit) {
            return;
        }
        traced_monikers[slot] = motor.genome_source_filename;
        ++traced_count;
    }
    if (traced_rows[slot] >= kRowsPerCreature) {
        return;
    }
    ++traced_rows[slot];

    const auto bounds = motor.movement_bounds();
    // UpdateAnchorAndBounds indexes the opposite leg as (down_foot == left) + 1.
    const std::size_t opposite =
        static_cast<std::size_t>(
            motor.down_foot == creatures1::creatures::DownFoot::left) + 1;
    const bool step_predicate = bounds.max_y < motor.limb_chain_end_y[opposite];

    // Brain summary: lobe count, total neurons, how many neurons have any
    // activation/firing anywhere, and the strongest decision-lobe (lobe 6)
    // firing.  "No brain activity" and "decision lobe never fires" are then
    // visible without a kit.
    unsigned lobe_count = 0, neuron_total = 0, active_neurons = 0,
             firing_neurons = 0, decision_neurons = 0, decision_best = 0,
             decision_best_index = 0;
    if (const auto* brain = creature.brain()) {
        lobe_count = brain->lobe_count();
        for (std::uint32_t l = 0; l < lobe_count; ++l) {
            const auto& lobe = brain->lobe(l);
            const std::uint32_t n = lobe.neuron_count_value();
            neuron_total += n;
            for (std::uint32_t i = 0; i < n; ++i) {
                const auto& neuron = lobe.neuron(i);
                active_neurons += neuron.activation != 0 ? 1u : 0u;
                firing_neurons += neuron.firing_strength != 0 ? 1u : 0u;
                if (l == 6 && neuron.firing_strength > decision_best) {
                    decision_best = neuron.firing_strength;
                    decision_best_index = i;
                }
            }
            if (l == 6) {
                decision_neurons = n;
            }
        }
    }

    // Per-lobe active/firing counts and the non-zero chemical count, so a
    // dead input (no drives, no perception) is identifiable by lobe.
    char per_lobe[360] = {};
    std::size_t per_lobe_len = 0;
    unsigned nonzero_chemicals = 0;
    if (const auto* brain = creature.brain()) {
        for (std::uint32_t l = 0; l < brain->lobe_count() && l < 12; ++l) {
            const auto& lobe = brain->lobe(l);
            unsigned a = 0, f = 0;
            for (std::uint32_t i = 0; i < lobe.neuron_count_value(); ++i) {
                a += lobe.neuron(i).activation != 0 ? 1u : 0u;
                f += lobe.neuron(i).firing_strength != 0 ? 1u : 0u;
            }
            // d = dendrites with a live target, w = those with non-zero
            // current weight.  A fresh genome-built brain and a
            // save-loaded one should both have wiring here.
            unsigned d = 0, w = 0, t = 0;
            for (std::uint32_t i = 0; i < lobe.neuron_count_value(); ++i) {
                const auto& neuron = lobe.neuron(i);
                const std::pair<const creatures1::brain::LobeConnection*,
                                unsigned> groups[2] = {
                    {neuron.rule0_connections_begin,
                     neuron.rule0_connection_count},
                    {neuron.rule1_connections_begin,
                     neuron.rule1_connection_count}};
                for (const auto& group : groups) {
                    for (unsigned c = 0; group.first != nullptr &&
                                         c < group.second; ++c) {
                        d += group.first[c].target_neuron != nullptr ? 1u : 0u;
                        w += group.first[c].baseline_weight;
                        t += group.first[c].target_weight;
                    }
                }
            }
            int wrote = std::snprintf(per_lobe + per_lobe_len,
                                      sizeof(per_lobe) - per_lobe_len,
                                      "%s%u:%u/%u/d%u/b%u/t%u",
                                      l == 0 ? "" : ",", l, a, f, d, w, t);
            if (wrote > 0) {
                per_lobe_len += static_cast<std::size_t>(wrote);
            }
        }
    }
    if (const auto* chemistry = creature.biochemistry()) {
        for (const auto& chemical : chemistry->chemical_states()) {
            nonzero_chemicals += chemical.concentration != 0 ? 1u : 0u;
        }
    }

    // Once per creature: dump the leg chains (limb order, sprite image base,
    // per-view start/end anchors) so left/right geometry can be compared.
    if (traced_rows[slot] == 1) {
        if (FILE* limbs = std::fopen("Creatures.limbs.log", "a")) {
            for (std::size_t chain = 1; chain <= 2; ++chain) {
                std::size_t depth = 0;
                for (const auto* limb = motor.limb_chain_heads[chain];
                     limb != nullptr; limb = limb->next_in_chain, ++depth) {
                    std::fprintf(limbs,
                                 "moniker=%08x chain=%zu depth=%zu limb=%p "
                                 "image_base=%u views(ax,ay,bx,by)=",
                                 motor.genome_source_filename, chain, depth,
                                 static_cast<const void*>(limb),
                                 static_cast<unsigned>(limb->image_index_base()));
                    const auto& t = limb->attachment_table;
                    for (std::size_t v = 0; v < 10; ++v) {
                        std::fprintf(limbs, "%u,%u,%u,%u ", t.anchor_a_x[v],
                                     t.anchor_a_y[v], t.anchor_b_x[v],
                                     t.anchor_b_y[v]);
                    }
                    std::fprintf(limbs, "\n");
                }
            }
            if (motor.body != nullptr) {
                const auto& j = motor.body->attachment_table;
                for (std::size_t chain = 1; chain <= 2; ++chain) {
                    std::fprintf(limbs, "moniker=%08x body_join chain=%zu (x,y)=",
                                 motor.genome_source_filename, chain);
                    for (std::size_t v = 0; v < 10; ++v) {
                        std::fprintf(limbs, "%u,%u ", j.join_x[chain][v],
                                     j.join_y[chain][v]);
                    }
                    std::fprintf(limbs, "\n");
                }
            }
            std::fclose(limbs);
        }
    }

    // Once per creature: dump each lobe's connection rules so the rule
    // programs driving dendrite growth/decay can be read directly.
    if (traced_rows[slot] == 1) {
        if (const auto* brain = creature.brain()) {
            if (FILE* rules = std::fopen("Creatures.rules.log", "a")) {
                for (std::uint32_t l = 0; l < brain->lobe_count(); ++l) {
                    const auto& lobe = brain->lobe(l);
                    for (std::size_t r = 0; r < 2; ++r) {
                        const auto& rule = lobe.connection_rule(r);
                        auto tokens = [&](const creatures1::brain::LobeRuleExpression& e) {
                            static char buf[64];
                            std::size_t n = 0;
                            for (auto t : e.tokens) {
                                n += static_cast<std::size_t>(std::snprintf(
                                    buf + n, sizeof(buf) - n, "%u.", t));
                            }
                            return buf;
                        };
                        std::fprintf(rules,
                            "moniker=%08x lobe=%u rule=%zu target=%u mode=%u "
                            "count=%u-%u base=%u-%u dstate=%u-%u "
                            "cwdecay=%u twconv=%u bstep=%u grow_int=%u ",
                            motor.genome_source_filename, l, r,
                            rule.target_lobe_index,
                            static_cast<unsigned>(rule.connection_mode),
                            rule.connection_count_min, rule.connection_count_max,
                            rule.baseline_weight_min, rule.baseline_weight_max,
                            rule.dendrite_state_min, rule.dendrite_state_max,
                            rule.current_weight_decay_selector,
                            rule.target_weight_convergence_selector,
                            rule.baseline_weight_step_interval,
                            rule.dendrite_growth_interval);
                        std::fprintf(rules, "grow=%s ", tokens(rule.dendrite_growth_expression));
                        std::fprintf(rules, "decay_int=%u decay=%s ",
                                     rule.dendrite_decay_interval,
                                     tokens(rule.dendrite_decay_expression));
                        std::fprintf(rules, "cw=%s ", tokens(rule.current_weight_expression));
                        std::fprintf(rules, "tw=%s\n", tokens(rule.target_weight_expression));
                    }
                }
                std::fclose(rules);
            }
        }
    }

    if (FILE* log = std::fopen("Creatures.motor.log", "a")) {
        std::fprintf(
            log,
            "tick=%u moniker=%08x lobe_af=%s chems=%u "
            "action=%u lobes=%u neurons=%u active=%u "
            "firing=%u decision=%u best=%u@%u dream=%u instincts=%u "
            "stage=%u alive=%d mode=%u flags=%02x "
            "bounds=%d,%d,%d,%d foot=%d,%d down=%d opposite=%zu "
            "legs=%d,%d;%d,%d step=%d pose=%.15s target=%.15s "
            "cursor=%zu anim=%.32s link=%p body=%d\n",
            world_tick, motor.genome_source_filename, per_lobe,
            nonzero_chemicals,
            creature.selected_action_id(), lobe_count, neuron_total,
            active_neurons, firing_neurons, decision_neurons, decision_best,
            decision_best_index,
            creature.instinct_runtime_state().dream_countdown,
            static_cast<unsigned>(
                creature.instinct_runtime_state().instinct_count),
            static_cast<unsigned>(creature.genome_life_stage()),
            creature.death_state() == 0 ? 1 : 0,
            static_cast<unsigned>(motor.bounds_mode()),
            motor.script_bounds_flags(), bounds.min_x, bounds.min_y,
            bounds.max_x, bounds.max_y, motor.down_foot_x, motor.down_foot_y,
            static_cast<int>(motor.down_foot), opposite,
            motor.limb_chain_end_x[1], motor.limb_chain_end_y[1],
            motor.limb_chain_end_x[2], motor.limb_chain_end_y[2],
            step_predicate ? 1 : 0, motor.current_pose.characters.data(),
            motor.target_pose.characters.data(), motor.animation_cursor,
            motor.animation_sequence.data(),
            static_cast<const void*>(motor.motion_link),
            motor.body == nullptr ? 0 : motor.body->world_x());
        std::fclose(log);
    }
}

// Skeleton's move helpers report their old and new sprite rectangles through
// this host; the document forwards both to the renderer's dirty queue.
class SkeletonMoveRedraw final : public creatures1::creatures::SkeletonWorldHost {
public:
    explicit SkeletonMoveRedraw(C1WindowsDocument& document)
        : document_(document) {}

    void move_by(creatures1::creatures::Skeleton& skeleton, int delta_x,
                 int delta_y) override {
        skeleton.translate_by(delta_x, delta_y);
    }
    void queue_dirty_world_rect(
        const creatures1::world::WorldRect& bounds) override {
        document_.queue_renderer_dirty_world_rect(bounds);
    }

private:
    C1WindowsDocument& document_;
};

} // namespace


BOOL C1WindowsDocument::OnOpenDocument(LPCTSTR path) {
    if (path == nullptr) {
        return FALSE;
    }
    if (semantic_document_ == nullptr) {
        semantic_document_ =
            std::make_unique<creatures1::application::Document>();
    }
    const CStringA native_path(path);
    if (!semantic_document_->open_document(
            *this, std::string_view(native_path.GetString(),
                                    native_path.GetLength()))) {
        return FALSE;
    }
    SetModifiedFlag(FALSE);
    return TRUE;
}

BOOL C1WindowsDocument::OnSaveDocument(LPCTSTR path) {
    if (path == nullptr || semantic_document_ == nullptr) {
        log_world_save_failure(nullptr, "missing path or document");
        return FALSE;
    }
    const CStringA native_path(path);
    try {
        return semantic_document_->save(
                   *this,
                   std::string_view(native_path.GetString(),
                                    native_path.GetLength()))
                   ? TRUE
                   : FALSE;
    } catch (const std::exception& error) {
        log_world_save_failure(native_path.GetString(), error.what());
        return FALSE;
    } catch (CException* error) {
        char message[512] = {};
        error->GetErrorMessage(message, sizeof(message));
        log_world_save_failure(native_path.GetString(), message);
        error->Delete();
        return FALSE;
    } catch (...) {
        log_world_save_failure(native_path.GetString(), "unknown exception");
        return FALSE;
    }
}

BOOL C1WindowsDocument::OnNewDocument() {
    // A new world is not backed by the previously opened save directory.
    // Clear this before MFC drains the old document so the next resource-host
    // construction cannot retain the old world's Images/Genetics tree.
    save_world_directory_.clear();
    save_image_directory_.clear();
    if (!CDocument::OnNewDocument()) {
        return FALSE;
    }

    semantic_document_ = std::make_unique<creatures1::application::Document>();
    if (!semantic_document_->on_new_document(*this)) {
        semantic_document_.reset();
        return FALSE;
    }
    std::uint32_t informative_menu = 0;
    if (read_view_setting("InformativeMenu", informative_menu, 0)) {
        semantic_document_->informative_menu_setting = informative_menu != 0;
    }
    rebuild_creature_selection_menu();
    C1MainFrame* frame = DYNAMIC_DOWNCAST(
        C1MainFrame, AfxGetMainWnd());
    if (frame != nullptr) {
        frame->bind_event_bar_document(this);
        frame->refresh_event_bar_status(*this);
    }
    SetModifiedFlag(FALSE);
    return TRUE;
}

void C1WindowsDocument::DeleteContents() {
    if (semantic_document_ == nullptr) {
        CDocument::DeleteContents();
        return;
    }
    semantic_document_->delete_contents(*this);
}

void C1WindowsDocument::OnCloseDocument() {
    if (semantic_document_ == nullptr) {
        CDocument::OnCloseDocument();
        return;
    }
    semantic_document_->close(*this);
}

void C1WindowsDocument::set_full_redraw_pending(bool pending) {
    if (renderer_ != nullptr) {
        renderer_->set_full_redraw_pending(pending);
    }
}

bool C1WindowsDocument::open_framework_document( creatures1::application::Document& /*document*/, std::string_view path) {
    // A saved world's generated creature sprites live beside its archive in
    // <save directory>/Images.  The original secondary resource directory
    // normally points there, but saves copied from another installation do
    // not update the process-wide registry path.  Remember the archive-local
    // fallback before MFC reads the archive; pixel data is intentionally
    // loaded lazily later by Image::get_pixel_data.
    save_world_directory_.clear();
    save_image_directory_.clear();
    const std::size_t separator = path.find_last_of("/\\");
    if (separator != std::string_view::npos) {
        save_world_directory_.assign(path.substr(0, separator + 1));
    }
    if (!save_world_directory_.empty()) {
        save_image_directory_ = save_world_directory_ + "Images\\";
    }
    const CStringA native_path(std::string(path).c_str());
    framework_opening_ = true;
    try {
        const BOOL result = CDocument::OnOpenDocument(native_path);
        framework_opening_ = false;
        return result != FALSE;
    } catch (...) {
        framework_opening_ = false;
        throw;
    }
}

std::string C1WindowsDocument::secondary_resource_directory(
    std::size_t index) const {
    if (!save_world_directory_.empty()) {
        if (index == kMainDirectoryIndex) {
            return save_world_directory_;
        }
        if (index == kImageDirectoryIndex) {
            return save_image_directory_;
        }
        if (index == kGeneticsDirectoryIndex) {
            return save_world_directory_ + "Genetics\\";
        }
    }
    if (g_active_secondary_directories != nullptr &&
        index < g_active_secondary_directories->paths.size() &&
        !g_active_secondary_directories->paths[index].empty()) {
        return g_active_secondary_directories->paths[index];
    }
    return index < resource_paths_.size() ? resource_paths_[index]
                                          : std::string{};
}

creatures1::display::SpriteFileSearchPaths
C1WindowsDocument::sprite_file_search_paths() const {
    return {secondary_resource_directory(kImageDirectoryIndex),
            resource_paths_[kImageDirectoryIndex]};
}

std::size_t C1WindowsDocument::body_sprite_creature_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->creature_count();
}

void C1WindowsDocument::validate_creature_body_sprites(std::size_t index) {
    if (world_runtime_ == nullptr || index >= world_runtime_->creature_count()) {
        return;
    }
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        world_runtime_->creature_at(index));
    if (creature == nullptr) {
        return;
    }
    if (creature->skeleton().body_sprites_are_stale(
            secondary_resource_directory(kImageDirectoryIndex), *this)) {
        // OnOpenDocument @004309a0 checks every loaded creature, and a
        // creature whose sprite file disagrees with its gallery gets the file
        // written again from its genome.
        (void)rebuild_creature_body_sprites(*this, *creature);
    }
}

void C1WindowsDocument::refresh_temporary_world_backup() {
    const std::string secondary_root =
        secondary_resource_directory(kMainDirectoryIndex);
    const std::string secondary_images =
        secondary_resource_directory(kImageDirectoryIndex);
    if (world_runtime_ == nullptr || secondary_root.empty() ||
        secondary_images.empty()) {
        return;
    }
    std::vector<creatures1::archive::GenomeFilenameId> genomes;
    for (std::size_t index = 0; index < world_runtime_->creature_count();
         ++index) {
        auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
            world_runtime_->creature_at(index));
        if (creature != nullptr) {
            genomes.push_back(creature->skeleton().genome_source_filename);
        }
    }
    creatures1::archive::refresh_temporary_world_backup(
        native_backup_files_,
        {secondary_root, secondary_images},
        {genomes.data(), genomes.size()});
}

void C1WindowsDocument::refresh_event_bar_object_display_panes() {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        frame->refresh_event_bar_status(*this);
    }
}

void C1WindowsDocument::update_event_bar_status_panes() {
    refresh_event_bar_object_display_panes();
}

void C1WindowsDocument::update_main_window_title_for_selected_creature() {
    update_main_window_title();
}

std::uint32_t C1WindowsDocument::max_norns_setting() const {
    return g_active_app_state == nullptr
               ? 0
               : g_active_app_state->max_norns_setting;
}

bool C1WindowsDocument::read_max_norns_setting(std::uint32_t& value) {
    return read_view_setting("MaxNorns", value, 0);
}

void C1WindowsDocument::set_max_norns_setting(std::uint32_t value) {
    if (g_active_app_state != nullptr) {
        const_cast<creatures1::application::SfcAppState*>(
            g_active_app_state)->max_norns_setting = value;
    }
}

void C1WindowsDocument::write_max_norns_setting(std::uint32_t value) {
    write_view_setting("MaxNorns", value);
}

std::string C1WindowsDocument::body_sprite_path( std::string_view image_directory, std::uint32_t genome_filename) const {
    return std::string(image_directory) +
           creatures1::display::sprite_file_name(genome_filename);
}

bool C1WindowsDocument::read_sprite_header(std::string_view path, std::uint16_t& image_count, std::uint32_t& first_frame_offset, std::uint16_t& first_frame_width, std::uint16_t& first_frame_height) {
    std::unique_ptr<creatures1::application::ResourceReadFile> file =
        files_.open_for_read(path);
    if (file == nullptr) {
        return false;
    }
    std::array<std::uint8_t, 10> header{};
    if (!file->read_exact(header.data(), header.size())) {
        return false;
    }
    image_count = static_cast<std::uint16_t>(header[0]) |
                  static_cast<std::uint16_t>(header[1]) << 8U;
    first_frame_offset = static_cast<std::uint32_t>(header[2]) |
                         static_cast<std::uint32_t>(header[3]) << 8U |
                         static_cast<std::uint32_t>(header[4]) << 16U |
                         static_cast<std::uint32_t>(header[5]) << 24U;
    first_frame_width = static_cast<std::uint16_t>(header[6]) |
                        static_cast<std::uint16_t>(header[7]) << 8U;
    first_frame_height = static_cast<std::uint16_t>(header[8]) |
                         static_cast<std::uint16_t>(header[9]) << 8U;
    return true;
}

void C1WindowsDocument::persist_and_close_eye_view() {
    // The current maintained lane does not instantiate CEyeView. If a
    // future adapter creates one, the existing selection-cycle boundary
    // supplies the close operation here rather than silently dropping it.
    if (eye_view_exists()) {
        close_eye_view();
    }
}

std::uint32_t C1WindowsDocument::privilege_level() const {
    return g_active_app_state == nullptr
               ? 0
               : static_cast<std::uint32_t>(
                     g_active_app_state->privilege_level);
}

std::size_t C1WindowsDocument::world_object_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->world_object_count();
}

bool C1WindowsDocument::world_object_can_be_destroyed(std::size_t index) const {
    if (world_runtime_ == nullptr || index >= world_runtime_->world_object_count()) {
        return false;
    }
    auto* object = world_runtime_->world_object_at(index);
    return object != nullptr && object->can_be_destroyed(*this);
}

bool C1WindowsDocument::world_object_is_generated(std::size_t index) const {
    // SFCDoc::OnSaveDocument @ 00430f10 picks the generated objects out of the
    // deletion queue with `(classifier & 0xff000000) == 0x04000000` at
    // 00430fb1..00430fb9 -- classifier family 4, a creature.  A creature's
    // body sprites are built from its genome and written to
    // <genome moniker>.spr, so that file is a derived artifact belonging to
    // exactly one creature, and this queue only ever holds objects already on
    // their way out.
    if (world_runtime_ == nullptr ||
        index >= world_runtime_->world_object_count()) {
        return false;
    }
    const creatures1::objects::Object* object =
        world_runtime_->world_object_at(index);
    return object != nullptr &&
           (object->classifier_base() & 0xff000000u) == 0x04000000u;
}

void C1WindowsDocument::initialize_generated_object_runtime(std::size_t index) {
    // SFCDoc's save walk calls the object's vtable slot 16 on a generated
    // object before removing its sprite file and destroying it.  Slot 16 is
    // Object::initialize_runtime_state, and the document is already its host.
    if (world_runtime_ == nullptr ||
        index >= world_runtime_->world_object_count()) {
        throw std::out_of_range("C1 world-object registry index");
    }
    creatures1::objects::Object* object = world_runtime_->world_object_at(index);
    if (object == nullptr) {
        throw std::logic_error("C1 world-object registry contains null");
    }
    object->initialize_runtime_state(*this);
}

std::string C1WindowsDocument::generated_image_filename(std::size_t index) const {
    // The native reads the dword at object+0x50 and stamps it into a buffer
    // pre-seeded with "####.SPR".  Object is exactly eighty bytes, so +0x50 is
    // the first field of the derived record, which for a creature is
    // Skeleton::render_pose_state -- and its first field is
    // genome_source_filename.  body_sprite_path builds that same name, and it
    // is the one Skeleton::validate_body_sprites already reads the sprites
    // back from.
    if (world_runtime_ == nullptr ||
        index >= world_runtime_->world_object_count()) {
        return {};
    }
    creatures1::objects::Object* object =
        world_runtime_->world_object_at(index);
    if (object == nullptr) {
        return {};
    }
    const creatures1::creatures::Creature* creature =
        creature_for_object(*object);
    if (creature == nullptr) {
        return {};
    }
    return body_sprite_path(secondary_resource_directory(kImageDirectoryIndex),
                            creature->skeleton().genome_source_filename);
}

void C1WindowsDocument::remove_generated_image(std::string_view filename) {
    if (!filename.empty()) {
        native_backup_files_.remove_file(filename);
    }
}

void C1WindowsDocument::destroy_world_object(std::size_t index) {
    if (world_runtime_ == nullptr || index >= world_runtime_->world_object_count()) {
        throw std::out_of_range("C1 world-object registry index");
    }
    auto* object = world_runtime_->world_object_at(index);
    if (object == nullptr) {
        throw std::logic_error("C1 world-object registry contains null");
    }
    world_runtime_->destroy_world_object(*object);
}

void C1WindowsDocument::remove_world_object(std::size_t index) {
    if (world_runtime_ == nullptr || index >= world_runtime_->world_object_count()) {
        throw std::out_of_range("C1 world-object registry index");
    }
    auto* object = world_runtime_->world_object_at(index);
    if (object != nullptr) {
        world_runtime_->remove_world_object(*object);
    }
}

void C1WindowsDocument::reset_world_tick_count() { world_tick_count_ = 0; }


void C1WindowsDocument::clear_favourite_place_names( creatures1::application::Document& /*document*/) {
    if (semantic_document_ != nullptr) {
        for (std::size_t index = 0;
             index < semantic_document_->favourite_place_count; ++index) {
            semantic_document_->favourite_places[index].name.clear();
        }
    }
}

void C1WindowsDocument::promote_temporary_world_backup() {
    const std::string secondary_root =
        secondary_resource_directory(kMainDirectoryIndex);
    if (!secondary_root.empty()) {
        creatures1::archive::promote_temporary_world_backup(
            native_backup_files_, secondary_root);
    }
}

void C1WindowsDocument::save_framework_document( creatures1::application::Document& /*document*/, std::string_view path) {
    const CStringA native_path(std::string(path).c_str());
    if (CDocument::OnSaveDocument(native_path) == FALSE) {
        throw std::runtime_error("C1 MFC document save failed");
    }
}

std::size_t C1WindowsDocument::object_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->object_count();
}

creatures1::objects::Object* C1WindowsDocument::object_at(std::size_t index) const {
    if (world_runtime_ == nullptr || index >= world_runtime_->object_count()) {
        return nullptr;
    }
    return world_runtime_->object_at(index);
}

void C1WindowsDocument::report_invalid_index() const {
    throw std::out_of_range("C1 object lifetime registry index");
}

std::size_t C1WindowsDocument::running_macro_count() const {
    return creatures1::scripting::g_running_macros.size();
}

const creatures1::scripting::MacroObjectContext* C1WindowsDocument::running_macro_at( std::size_t index) const {
    if (index >= creatures1::scripting::g_running_macros.size() ||
        creatures1::scripting::g_running_macros[index] == nullptr) {
        return nullptr;
    }
    return &creatures1::scripting::g_running_macros[index]->object_context;
}

std::size_t C1WindowsDocument::immediate_event_count() const {
    return event_scheduler_.immediate_event_count();
}

const creatures1::objects::QueuedObjectEvent* C1WindowsDocument::immediate_event_at( std::size_t index) const {
    return event_scheduler_.immediate_event_at(index);
}

std::size_t C1WindowsDocument::delayed_event_count() const {
    return event_scheduler_.delayed_event_count();
}

const creatures1::objects::QueuedObjectEvent* C1WindowsDocument::delayed_event_at( std::size_t index) const {
    return event_scheduler_.delayed_event_at(index);
}

bool C1WindowsDocument::delayed_event_is_active( const creatures1::objects::QueuedObjectEvent& event) const {
    return event_scheduler_.delayed_event_is_active(event);
}

std::size_t C1WindowsDocument::queued_stimulus_count() const {
    return event_scheduler_.queued_stimulus_count();
}

const creatures1::objects::QueuedCreatureStimulus* C1WindowsDocument::queued_stimulus_at( std::size_t index) const {
    return event_scheduler_.queued_stimulus_at(index);
}

bool C1WindowsDocument::stimulus_targets_object( const creatures1::objects::QueuedCreatureStimulus& stimulus, const creatures1::objects::Object& object) const {
    const auto* creature = creature_for_object(object);
    return stimulus.source_object == &object ||
           (creature != nullptr && stimulus.target_creature == creature);
}

bool C1WindowsDocument::save_for_close( creatures1::application::Document& /*document*/) {
    // SFCDoc::OnCloseDocument @00430e90 calls OnSaveDocument through vtable
    // slot 0x88 with the stored world save path.  It does NOT go through
    // SaveModified: that returns success without writing when the modified
    // flag is clear, and nothing in the port ever sets it, so closing the
    // world silently discarded every change made while playing.
    if (g_active_world_save_path == nullptr ||
        g_active_world_save_path->empty()) {
        return false;
    }
    return OnSaveDocument(
               CStringA(g_active_world_save_path->c_str())) != FALSE;
}

void C1WindowsDocument::report_save_failure() {
    AfxMessageBox("Creatures could not save the world.",
                  MB_ICONWARNING, 0);
}

void C1WindowsDocument::close_framework_document( creatures1::application::Document& /*document*/) {
    CDocument::OnCloseDocument();
}

std::size_t C1WindowsDocument::scenery_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->scenery_count();
}

creatures1::objects::Scenery* C1WindowsDocument::scenery_at(std::size_t index) const {
    return world_runtime_ == nullptr ? nullptr
                                      : world_runtime_->scenery_at(index);
}

void C1WindowsDocument::delete_first_non_scenery_object() {
    if (world_runtime_ != nullptr) {
        world_runtime_->destroy_first_non_scenery_object();
    }
}

void C1WindowsDocument::delete_first_scenery() {
    if (world_runtime_ != nullptr) {
        world_runtime_->destroy_first_scenery_object();
    }
}

bool C1WindowsDocument::map_loaded() const { return world_runtime_ != nullptr; }


void C1WindowsDocument::delete_map() {
    if (world_runtime_ != nullptr) {
        world_runtime_->reset_map_data();
    }
}

void C1WindowsDocument::clear_sprite_file_cache() { sprite_files_.clear(); }


void C1WindowsDocument::clear_charset_glyph_cache() {
    charset_ = {};
    image_cache_.reset_for_charset_load();
}

void C1WindowsDocument::destroy_limb(
    creatures1::creatures::LimbPart& limb) {
    delete &limb;
}

void C1WindowsDocument::stop_continuous_sound(int sound_handle) {
    if (sound_manager_available()) {
        sound_manager().stop_continuous_sound(
            static_cast<std::uint32_t>(sound_handle), false);
    }
}

void C1WindowsDocument::remove_from_renderable_set(
    creatures1::creatures::Skeleton& skeleton) {
    renderables().erase(skeleton);
}

void C1WindowsDocument::remove_from_renderable_set(
    creatures1::objects::CompoundObject& object) {
    renderables().erase(object);
}

void C1WindowsDocument::unregister_from_object_registry(
    creatures1::creatures::Skeleton& skeleton) {
    if (world_runtime_ == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < world_runtime_->object_count();
         ++index) {
        if (world_runtime_->object_at(index) == &skeleton) {
            world_runtime_->remove_object_at(index);
            break;
        }
    }
    world_runtime_->remove_world_object(skeleton);
}

void C1WindowsDocument::unregister_from_object_registry(
    creatures1::objects::CompoundObject& object) {
    if (world_runtime_ == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < world_runtime_->object_count();
         ++index) {
        if (world_runtime_->object_at(index) == &object) {
            world_runtime_->remove_object_at(index);
            break;
        }
    }
    world_runtime_->remove_world_object(object);
}

void C1WindowsDocument::release_gallery(
    creatures1::display::Gallery& gallery) {
    if (world_runtime_ != nullptr) {
        world_runtime_->release_gallery(gallery);
    }
}

std::size_t C1WindowsDocument::gallery_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->gallery_count();
}

void C1WindowsDocument::release_first_gallery() {
    if (world_runtime_ != nullptr && world_runtime_->gallery_count() != 0) {
        world_runtime_->release_gallery(*world_runtime_->gallery_at(0));
    }
}

void C1WindowsDocument::remove_running_macros() {
    creatures1::scripting::clear_running_macros();
}

void C1WindowsDocument::clear_script_definitions() {
    creatures1::scripting::initialize_script_definition_table();
}

void C1WindowsDocument::clear_object_registries() {
    if (world_runtime_ != nullptr) {
        world_runtime_->clear_entity_registry();
        world_runtime_->clear_object_registry();
    }
}

void C1WindowsDocument::clear_renderable_objects() {
    if (world_runtime_ != nullptr) {
        world_runtime_->clear_renderable_registry();
    }
}

void C1WindowsDocument::clear_document_selection() {
    selection_.clear();
    selected_creature_entry_ = nullptr;
}

void C1WindowsDocument::delete_framework_contents( creatures1::application::Document& /*document*/) {
    C1MainFrame* frame = DYNAMIC_DOWNCAST(
        C1MainFrame, AfxGetMainWnd());
    if (frame != nullptr) {
        frame->bind_event_bar_document(nullptr);
    }
    destroy_world_renderer();
    // The registry drain in delete_contents has already destroyed it.
    pointer_tool_ = nullptr;
    if (world_runtime_ != nullptr) {
        // Document::delete_contents has already drained the gallery
        // registry through release_first_gallery(), after destroying
        // the objects that hold gallery references.  Do not clear it a
        // second time here: the MFC framework hook only tears down the
        // remaining native runtime owner.
        world_runtime_.reset();
    }
    // C1ResourceHost owns the galleries and, more importantly, stores the
    // selected world's directory table by value.  Leaving it alive here
    // makes a subsequent Open/New document continue reading the old world's
    // Images and Genetics paths.  The old gallery registry has already been
    // drained above, so these hosts are now safe to rebuild for the next
    // document.
    resources_.reset();
    creature_resources_.reset();
    palette_files_.reset();
    palette_platform_.reset();
    event_scheduler_ = {};
    if (framework_opening_) {
        world_runtime_ = std::make_unique<creatures1::world::WorldRuntime>();
        creatures1::scripting::initialize_script_definition_table();
    } else {
        semantic_document_.reset();
    }
    world_update_timer_interval_ms_ = 0;
    world_tick_count_ = 0;
    world_update_in_progress_ = false;
    creature_update_cohort_ = 0;
    ambient_sound_cooldown_ticks_ = 0;
    manual_navigation_safe_frame_count_ = 0;
    world_tick_phase_ = 0;
    pending_text_input_.clear();
    clear_text_input_buffer();
    reset_text_input_configuration();
    edit_object_ = nullptr;
    viewport_navigation_disabled_ = false;
    CDocument::DeleteContents();
}

const creatures1::application::Document* C1WindowsDocument::semantic_document() const {
    return semantic_document_.get();
}

creatures1::application::Document* C1WindowsDocument::semantic_document_mutable() {
    return semantic_document_.get();
}

std::size_t C1WindowsDocument::creature_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->creature_count();
}

creatures1::creatures::CreatureSelectionEntry* C1WindowsDocument::creature_at( std::size_t index) const {
    return world_runtime_ == nullptr ? nullptr
                                      : world_runtime_->creature_at(index);
}

bool C1WindowsDocument::remove_at(std::size_t index) {
    if (world_runtime_ == nullptr || index >= creature_count()) {
        return false;
    }
    if (selected_creature_entry_ == world_runtime_->creature_at(index)) {
        selected_creature_entry_ = nullptr;
    }
    return world_runtime_->remove_at(index);
}

creatures1::creatures::Creature* C1WindowsDocument::selected_creature() const {
    return dynamic_cast<creatures1::creatures::Creature*>(
        selected_creature_entry_);
}

const creatures1::creatures::Creature* C1WindowsDocument::creature_for_object( const creatures1::objects::Object& object) const {
    for (std::size_t index = 0; index < creature_count(); ++index) {
        const auto* creature = dynamic_cast<const creatures1::creatures::Creature*>(
            creature_at(index));
        if (creature != nullptr && &creature->skeleton() == &object) {
            return creature;
        }
    }
    return nullptr;
}

creatures1::creatures::Creature* C1WindowsDocument::mutable_creature_for_object( creatures1::objects::Object& object) {
    for (std::size_t index = 0; index < creature_count(); ++index) {
        auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
            creature_at(index));
        if (creature != nullptr && &creature->skeleton() == &object) {
            return creature;
        }
    }
    return nullptr;
}

void C1WindowsDocument::append_funeral_state_word(std::uint32_t value) {
    if (semantic_document_ == nullptr ||
        semantic_document_->serialized_document_state_words.size() >= 16) {
        return;
    }
    semantic_document_->serialized_document_state_words.push_back(value);
}

std::size_t C1WindowsDocument::funeral_state_word_count() const {
    return semantic_document_ == nullptr
               ? 0
               : semantic_document_->serialized_document_state_words.size();
}

void C1WindowsDocument::flush_funeral_state() {
    // FlushFuneralKitDocumentStateWords @ 00435c10 delivers each persisted
    // 32-bit state word - the identifier of a Norn that died while still
    // shown in the Event Bar - to the Funeral Kit, then clears the count.
    // This is the mechanism behind a dead Norn getting a Graveyard entry.
    if (semantic_document_ == nullptr) {
        return;
    }
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return;
    }
    constexpr std::size_t kFuneralKitIndex = 9;
    COleDispatchDriver& driver = frame->embedded_kit_dispatch(kFuneralKitIndex);
    if (driver.m_lpDispatch == nullptr) {
        // The native leaves the words queued when the kit is absent, so the
        // Graveyard still receives them once it connects.
        return;
    }
    for (const std::uint32_t word :
         semantic_document_->serialized_document_state_words) {
        invoke_kit_communicate(
            driver,
            {creatures1::application::embedded_kit_message_header(
                 creatures1::application::EmbeddedKitMessageKind::data,
                 creatures1::application::kEmbeddedKitDataCode),
             word});
    }
    semantic_document_->serialized_document_state_words.clear();
}


bool C1WindowsDocument::viewport_navigation_is_disabled() const {
    return viewport_navigation_disabled_;
}

void C1WindowsDocument::disable_viewport_navigation() {
    viewport_navigation_disabled_ = true;
    reset_renderer_navigation();
    invalidate_main_toolbar();
}

void C1WindowsDocument::request_event_bar_viewport_origin(int world_x, int world_y) {
    viewport_navigation_disabled_ = false;
    request_renderer_origin(world_x, world_y);
}

creatures1::objects::Object& C1WindowsDocument::object_for_creature( creatures1::creatures::Creature& creature) const {
    if (world_runtime_ != nullptr) {
        for (std::size_t index = 0; index < world_runtime_->creature_count();
             ++index) {
            auto* candidate = dynamic_cast<creatures1::creatures::Creature*>(
                world_runtime_->creature_at(index));
            if (candidate == &creature) {
                // C1's Creature is represented in the Object hierarchy by
                // its Skeleton.  Keep that recovered identity explicit at
                // the application boundary instead of fabricating a
                // Creature:Object inheritance relationship.
                return candidate->skeleton();
            }
        }
    }
    throw std::out_of_range(
        "C1 creature is not owned by the active world registry");
}

std::size_t C1WindowsDocument::selection_count() const { return selection_.size(); }


creatures1::creatures::CreatureSelectionEntry* C1WindowsDocument::selection_at( std::size_t index) const {
    return index < selection_.size() ? selection_.at(index) : nullptr;
}

void C1WindowsDocument::set_selected_creature( creatures1::creatures::CreatureSelectionEntry* creature) {
    selected_creature_entry_ = creature;
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        frame->rebuild_creature_selector(selection_, creature);
    }
}

void C1WindowsDocument::broadcast_selection_state(std::uint32_t state_code) {
    // The recovered broadcast is the same twenty-slot DISPID 1 walk that
    // broadcast_embedded_control_state already performs; Creature::Die
    // @ 0040dc30 reaches it with state 8.  There was never a second path.
    broadcast_embedded_control_state(state_code);
}

void C1WindowsDocument::persist_informative_menu_setting(bool enabled) {
    write_view_setting("InformativeMenu", enabled ? 1 : 0);
}

void C1WindowsDocument::rebuild_informative_selection_menu(bool enabled) {
    if (semantic_document_ == nullptr) {
        return;
    }
    semantic_document_->informative_menu_setting = enabled;
    rebuild_creature_selection_menu();
}

void C1WindowsDocument::update_main_window_title() {
    // UpdateMainWindowTitleForSelectedCreature @ 0x00422720: title is always
    // "Creatures", with " - <display name>" appended when a creature is
    // selected -- never just the bare name.  This previously dropped the
    // "Creatures" prefix and separator entirely, routing around the
    // already-correct, already-verified policy in ui/main_window.cpp,
    // which nothing called.
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return;
    }
    class FrameTitleAdapter final : public creatures1::ui::MainWindowApi {
    public:
        explicit FrameTitleAdapter(C1MainFrame& frame) : frame_(frame) {}
        void set_window_title(std::string_view title) override {
            frame_.SetWindowTextA(std::string(title).c_str());
        }

    private:
        C1MainFrame& frame_;
    } title_adapter(*frame);

    const creatures1::creatures::Creature* creature = selected_creature();
    class CreatureTitleSource final
        : public creatures1::ui::SelectedCreatureTitleSource {
    public:
        explicit CreatureTitleSource(const creatures1::creatures::Creature& creature)
            : name_(creature.display_name()) {}
        std::string_view display_name() const override { return name_; }

    private:
        // Creature::display_name() returns by value; a string_view can't
        // safely bind to that temporary, so the name is copied here once
        // and the view returned refers to this member's storage instead.
        std::string name_;
    };

    if (creature == nullptr) {
        creatures1::ui::update_main_window_title_for_selected_creature(
            title_adapter, nullptr);
    } else {
        CreatureTitleSource title_source(*creature);
        creatures1::ui::update_main_window_title_for_selected_creature(
            title_adapter, &title_source);
    }
}

bool C1WindowsDocument::eye_view_exists() const {
    return eye_view_ != nullptr;
}


void C1WindowsDocument::close_eye_view() {
    // ~CEyeView_Deleting @ 00417210: the window goes with the view object.
    if (eye_view_ != nullptr) {
        if (eye_view_->GetSafeHwnd() != nullptr) {
            eye_view_->DestroyWindow();
        }
        eye_view_.reset();
    }
}


void C1WindowsDocument::update_eye_view_title() {
    // UpdateWindowTitleForSelectedCreature @ 004172f0.
    if (eye_view_ != nullptr) {
        creatures1::ui::update_eye_view_title(*eye_view_);
    }
}


void C1WindowsDocument::persist_eye_view_position() {
    if (eye_view_ != nullptr) {
        // PersistEyeViewWindowPosition @ 004175e0.
        eye_view_->persist_eye_view_position(eye_view_->window_position());
    }
}

void C1WindowsDocument::destroy_eye_view() { close_eye_view(); }

std::int32_t C1WindowsDocument::selected_creature_sound_source_x() const {
    creatures1::creatures::Creature* creature = selected_creature();
    return creature == nullptr
               ? 0
               : object_for_creature(*creature).sound_source_x();
}

std::int32_t C1WindowsDocument::selected_creature_sound_source_y() const {
    creatures1::creatures::Creature* creature = selected_creature();
    return creature == nullptr
               ? 0
               : object_for_creature(*creature).sound_source_y();
}

std::string C1WindowsDocument::eye_view_title() const {
    // CApplication::ToggleEyeView / CEyeView::UpdateWindowTitleForSelectedCreature
    // (0x00432140 / 0x004172f0) both load string id 0xef26 -- confirmed
    // directly from the reference exe's own STRINGTABLE bundle 3827: it is
    // literally the word "View" (the same string the View menu uses), not
    // a distinct "Eye View" string. This was loading id 0x80, which is not
    // a string resource at all (128 exists only as a BITMAP/ICON), so
    // LoadStringA failed and the window's title was empty.
    CStringA value;
    value.LoadStringA(static_cast<UINT>(0xef26));
    return value.GetString();
}

void C1WindowsDocument::create_eye_view(
    const creatures1::application::EyeViewCreationParameters& parameters,
    std::int32_t initial_viewport_left, std::int32_t initial_viewport_top,
    std::string_view title) {
    if (eye_view_ != nullptr) {
        return;
    }
    auto view = std::make_unique<C1EyeViewWindow>(*this);
    if (!view->create(title, parameters, initial_viewport_left,
                      initial_viewport_top)) {
        return;
    }
    eye_view_ = std::move(view);
    // Native creation starts with the creature's sound-source viewport, then
    // immediately runs the same follow calculation used on world ticks. Do
    // that before the first paint so the eye view is centred on the creature
    // and its overlay is composited into a populated back buffer.
    creatures1::ui::update_selected_creature_follow_viewport(*eye_view_);
    eye_view_->redraw_full_view();
}

void C1WindowsDocument::invalidate_eye_view_follow_position() {
    if (eye_view_ != nullptr) {
        eye_view_->invalidate_follow_position();
    }
}


void C1WindowsDocument::request_viewport_origin_for_selected_creature() {
    // CWorldRenderer::RequestViewportOriginForSelectedCreature: unlike
    // return_viewport_navigation_to_selection, this centres the viewport on the
    // creature and declines to move when its foot is outside the navigation
    // bounds.  The renderer owns both decisions.
    if (renderer_ != nullptr) {
        renderer_->request_viewport_origin_for_selected_creature();
    }
}

void C1WindowsDocument::return_viewport_navigation_to_selection() {
    const creatures1::creatures::Creature* creature = selected_creature();
    if (creature != nullptr) {
        request_renderer_origin(creature->skeleton().down_foot_x,
                                creature->skeleton().down_foot_y);
    }
}

void C1WindowsDocument::refresh_event_bar() {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        frame->refresh_event_bar_status(*this);
    }
}

void C1WindowsDocument::invalidate_main_toolbar() {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        frame->invalidate_main_toolbar();
    }
}

[[noreturn]] void C1WindowsDocument::throw_invalid_selection_argument() {
    throw std::out_of_range("C1 creature selection menu index");
}

void C1WindowsDocument::rebuild_creature_selection_menu() {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr || semantic_document_ == nullptr) {
        return;
    }
    C1NativeCreatureSelectionMenuPlatform platform(*frame);
    (void)creatures1::ui::rebuild_creature_selection_menu(
        selection_, *this, platform,
        semantic_document_->informative_menu_setting);
    frame->rebuild_creature_selector(selection_, selected_creature_entry_);
}

const creatures1::creatures::StimulusContext* C1WindowsDocument::default_stimulus_context( std::size_t index) const {
    if (index >= creatures1::creatures::kDefaultStimulusContexts.size()) {
        return nullptr;
    }
    return &creatures1::creatures::kDefaultStimulusContexts[index];
}

std::string C1WindowsDocument::localized_birthplace() const {
    CStringA value;
    value.LoadStringA(0xef1f);
    return value.GetString();
}

std::string C1WindowsDocument::format_moniker(std::uint32_t identifier) const {
    CStringA value;
    value.Format("%lx", static_cast<unsigned long>(identifier));
    return value.GetString();
}

void C1WindowsDocument::select_creature_by_menu_command(int command_id) {
    creatures1::ui::select_creature_by_menu_index(*this,
                                                   command_id - 40000);
}

std::uint32_t C1WindowsDocument::world_tick_count() const { return world_tick_count_; }


namespace {

// TriggerSelectedCreatureScriptEvent @ 004324c0 runs the script on the
// selected creature with that creature as both owner and source.
class WindowsSelectedCreatureScriptHost final
    : public creatures1::application::SelectedCreatureScriptCommandHost {
public:
    explicit WindowsSelectedCreatureScriptHost(C1WindowsDocument& document)
        : document_(document) {}

    creatures1::objects::Object* selected_creature_object() const override {
        creatures1::creatures::Creature* creature =
            document_.selected_creature();
        return creature == nullptr ? nullptr
                                   : &document_.object_for_creature(*creature);
    }

    void execute_script_for_classifier(
        creatures1::objects::Object& script_owner,
        creatures1::objects::Object& from_object,
        creatures1::scripting::ScriptClassifier classifier,
        bool force_restart) override {
        // The dispatch host takes the classifier packed the way the archive
        // stores it: event, species, genus, family from low byte up.
        const std::uint32_t packed =
            static_cast<std::uint32_t>(classifier.event) |
            (static_cast<std::uint32_t>(classifier.species) << 8) |
            (static_cast<std::uint32_t>(classifier.genus) << 16) |
            (static_cast<std::uint32_t>(classifier.family) << 24);
        WindowsObjectScriptDispatchHost scripts(document_);
        scripts.execute_script_for_classifier(script_owner, &from_object,
                                              packed, force_restart);
    }

private:
    C1WindowsDocument& document_;
};

// The toolbar's creature-name combo, plus the pointer tool the name is spoken
// through.  UpdateCreatureNameComboHistory @ 00432360 drives both.
class WindowsCreatureNameHistoryUi final
    : public creatures1::application::CreatureNameHistoryUi {
public:
    WindowsCreatureNameHistoryUi(C1WindowsDocument& document,
                                 CComboBox& combo)
        : document_(document), combo_(combo) {}

    std::string current_creature_name() const override {
        CStringA text;
        combo_.GetWindowTextA(text);
        return text.GetString();
    }

    std::size_t find_exact_name(std::string_view name) const override {
        const std::string owned(name);
        const int index = combo_.FindStringExact(-1, owned.c_str());
        return index == CB_ERR
                   ? (std::numeric_limits<std::size_t>::max)()
                   : static_cast<std::size_t>(index);
    }

    void remove_name_at(std::size_t index) override {
        combo_.DeleteString(static_cast<UINT>(index));
    }

    void insert_name_at_front(std::string_view name) override {
        const std::string owned(name);
        combo_.InsertString(0, owned.c_str());
    }

    void select_first_name() override { combo_.SetCurSel(0); }

    std::size_t name_count() const override {
        const int count = combo_.GetCount();
        return count < 0 ? 0 : static_cast<std::size_t>(count);
    }

    void update_pointer_tool_name(std::string_view name) override {
        creatures1::objects::Object* tool = document_.pointer_tool();
        auto* pointer = dynamic_cast<creatures1::ui::PointerTool*>(tool);
        if (pointer == nullptr) {
            return;
        }
        C1MainFrame* frame = active_main_frame();
        WindowsPointerToolRuntimeHost runtime(
            document_, frame == nullptr ? nullptr : active_c1_view(*frame));
        pointer->set_text_with_toolbar_update(name, runtime);
    }

private:
    C1WindowsDocument& document_;
    CComboBox& combo_;
};

} // namespace

// SFCDoc's own command map.  The document had none, so every command the
// native routes here -- Play and Pause among them -- reached no handler at
// all and MFC greyed the buttons out.
//
// Neither Play nor Pause calls CCmdUI::Enable in the native: their update
// handlers @ 0x00434f10 and @ 0x00434ef0 call vtable slot 1, SetCheck, so the
// pair reads as a radio showing which state the world is in and both stay
// enabled.
BEGIN_MESSAGE_MAP(C1WindowsDocument, CDocument)
    ON_COMMAND(0x8045, OnWorldPlay)
    ON_COMMAND(0x8046, OnWorldPause)
    ON_UPDATE_COMMAND_UI(0x8045, OnUpdateWorldPlay)
    ON_UPDATE_COMMAND_UI(0x8046, OnUpdateWorldPause)
    ON_COMMAND(0x807d, OnSelectNextCreature)
    ON_COMMAND(0x807f, OnSelectPreviousCreature)
    ON_COMMAND(0xe802, OnSpeakCreatureName)
    // TriggerSelectedCreatureScriptEvent @ 004324c0: the four commands of the
    // "Which is my creature?" family each fire a script on the selected
    // creature, with the command id folded into the classifier. What the game
    // does through a script stays a script -- the port does not reimplement it
    // in C++ and call that equivalent.
    ON_COMMAND_RANGE(0x0071, 0x0074, OnTriggerSelectedCreatureScriptEvent)
    ON_UPDATE_COMMAND_UI_RANGE(0x9c40, 0x9c5e, OnUpdateSelectCreatureByMenuIndex)
    // The nineteen commands sharing the update handler at 004335d0.
    ON_UPDATE_COMMAND_UI(0x0071, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8009, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x800c, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8024, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8025, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8027, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8040, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8048, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI_RANGE(0x8053, 0x8058, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x8060, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0x807e, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0xe145, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0xe802, OnUpdateRequiresRunningWorld)
    ON_UPDATE_COMMAND_UI(0xe803, OnUpdateRequiresRunningWorld)
END_MESSAGE_MAP()

void C1WindowsDocument::OnUpdateRequiresRunningWorld(CCmdUI* command_ui) {
    if (command_ui != nullptr) {
        command_ui->Enable(world_timer_is_armed() ? TRUE : FALSE);
    }
}

void C1WindowsDocument::OnTriggerSelectedCreatureScriptEvent(UINT command_id) {
    // TriggerSelectedCreatureScriptEvent @ 004324c0: the four "Which is my
    // Norn" family commands fire a script on the selected creature, with the
    // command id folded into the classifier.
    WindowsSelectedCreatureScriptHost host(*this);
    creatures1::application::trigger_selected_creature_script_event(
        host, static_cast<int>(command_id));
}

void C1WindowsDocument::OnUpdateSelectCreatureByMenuIndex(CCmdUI* command_ui) {
    // OnUpdateSelectCreatureByMenuIndex_00 @ 00433c00 and its 30 siblings.
    // An index past the end of the selection array makes no CCmdUI call at
    // all, so the item keeps whatever state it had.
    if (command_ui == nullptr) {
        return;
    }
    const std::size_t index =
        static_cast<std::size_t>(command_ui->m_nID - 0x9c40);
    if (index >= creature_count()) {
        return;
    }
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    bool enabled = false;
    if (creature != nullptr && world_timer_is_armed()) {
        enabled = !object_for_creature(*creature)
                       .is_sound_source_below_world_y();
    }
    command_ui->Enable(enabled ? TRUE : FALSE);
    command_ui->SetCheck(
        creature != nullptr && creature == selected_creature() ? 1 : 0);
}

void C1WindowsDocument::OnSpeakCreatureName() {
    // UpdateCreatureNameComboHistory @ 00432360: take what is typed in the
    // toolbar combo, move it to the front of the drop-down history, and speak
    // it through the pointer tool.
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return;
    }
    WindowsCreatureNameHistoryUi ui(*this, frame->creature_selector());
    creatures1::application::update_creature_name_combo_history(
        ui, world_tick_count(), pointer_tool_name_update_deadline_);
}

void C1WindowsDocument::OnWorldPlay() {
    creatures1::application::arm_world_update_timer(*this);
}

void C1WindowsDocument::OnWorldPause() {
    // Pressing Pause while already paused single-steps one tick, at privilege
    // level 2 and above -- that policy lives in service_world_update_timer.
    creatures1::application::service_world_update_timer(*this);
}

void C1WindowsDocument::OnUpdateWorldPlay(CCmdUI* command_ui) {
    if (command_ui != nullptr) {
        command_ui->SetCheck(world_timer_is_armed() ? 1 : 0);
    }
}

void C1WindowsDocument::OnUpdateWorldPause(CCmdUI* command_ui) {
    if (command_ui != nullptr) {
        command_ui->SetCheck(world_timer_is_armed() ? 0 : 1);
    }
}

void C1WindowsDocument::OnSelectNextCreature() {
    creatures1::application::select_next_creature(*this);
}

void C1WindowsDocument::OnSelectPreviousCreature() {
    creatures1::application::select_previous_creature(*this);
}

bool C1WindowsDocument::world_timer_is_armed() const {
    // "Armed" is the port's name for what the native keeps in
    // g_world_update_timer_state: whether the world is actually ticking.  A
    // pause keeps the interval so resume can restore the same speed, so the
    // latch has to be consulted as well as the interval.
    return world_update_timer_interval_ms_ != 0 && !world_update_paused_;
}

bool C1WindowsDocument::world_update_timer_is_running() const {
    return world_timer_is_armed();
}

void C1WindowsDocument::mark_world_update_timer_paused() {
    world_update_paused_ = true;
}

void C1WindowsDocument::mark_world_update_timer_running() {
    world_update_paused_ = false;
}

std::int32_t C1WindowsDocument::object_sound_channel(
    std::size_t index) const {
    creatures1::objects::Object* object = non_scenery_object_at(index);
    return object == nullptr ? -1 : object->continuous_sound_handle();
}

void C1WindowsDocument::release_object_sound_channel(std::size_t index) {
    creatures1::objects::Object* object = non_scenery_object_at(index);
    if (object != nullptr) {
        object->release_continuous_sound_channel();
    }
}

bool C1WindowsDocument::sound_mixer_is_suspended() const {
    return g_active_sound_manager == nullptr ||
           g_active_sound_manager->mixer_suspended();
}

void C1WindowsDocument::clear_sound_channel_active(std::size_t channel) {
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->clear_continuous_channel_marker(
            static_cast<std::uint32_t>(channel));
    }
}

void C1WindowsDocument::stop_sound_channel(std::size_t channel) {
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->stop_channel(
            static_cast<std::uint32_t>(channel));
    }
}

bool C1WindowsDocument::debug_logging_available() const {
    return active_debug_console() != nullptr;
}

void C1WindowsDocument::log_continuous_sound_stop(std::int32_t channel) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 1, "Stop Cnt Sound %d\n",
                                      channel);
    }
}

void C1WindowsDocument::stop_all_sounds() {
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->stop_all_sounds();
    }
}

void C1WindowsDocument::update_world() {
    update_world_tick();
}

void C1WindowsDocument::update_world_tick() {

    if (semantic_document_ == nullptr) {
        return;
    }
    semantic_document_->update_world(*this);
}

void C1WindowsDocument::recover_world_update_after_boundary_failure( const std::exception& error) {
    world_update_in_progress_ = false;
    if (!world_update_boundary_hold_reported_) {
        std::string message =
            "C1 world-update boundary hold: " +
            std::string(error.what()) + "\n";
        OutputDebugStringA(message.c_str());
        world_update_boundary_hold_reported_ = true;
    }
}

bool C1WindowsDocument::world_update_in_progress() const {
    return world_update_in_progress_;
}

void C1WindowsDocument::set_world_update_in_progress(bool in_progress) {
    world_update_in_progress_ = in_progress;
}

bool C1WindowsDocument::has_edit_object() const { return edit_object_ != nullptr; }


void C1WindowsDocument::place_edit_object_at_pointer() {
    // SFCDoc::UpdateWorld: the edit object follows the pointer through the
    // object's MoveToAndRedraw slot, at the mouse's world position with one
    // world-width wrap -- the view owns the client coordinates, exactly as
    // g_SFCView does in the native.
    if (edit_object_ == nullptr) {
        return;
    }
    int world_x = mouse_world_x();
    if (world_x >= creatures1::world::kWorldWidth) {
        world_x -= creatures1::world::kWorldWidth;
    }
    move_to_and_redraw(*edit_object_, world_x, mouse_world_y());
}

bool C1WindowsDocument::pending_right_button() const {
    // Document::update_world (application/document.cpp) polls this every
    // tick while an object is being carried (has_edit_object()), to detect
    // a right-click as the drop signal and release it. This was hardcoded
    // to false, so a picked-up object -- anything grabbed with shift+left,
    // e.g. a coffee pot -- could never be dropped again: the flag it
    // needs to read (SfcViewPendingInputFlag::right_button) already
    // exists and is set correctly by on_right_button_down; this just never
    // read it.
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view == nullptr) {
        return false;
    }
    constexpr std::uint32_t kRightButton = static_cast<std::uint32_t>(
        creatures1::ui::SfcViewPendingInputFlag::right_button);
    return (view->view_state().pending_input_flags & kRightButton) != 0;
}


void C1WindowsDocument::clear_pending_input() {
    pending_text_input_.clear();
}

void C1WindowsDocument::finalize_edit_object() {
    // Vtable slot 13, called by SFCDoc::UpdateWorld on the edit object when a
    // right-click drops it.  Native has six bodies, resolved most-derived
    // first: Lift snaps to the floor of the room it was dropped in (0042c3e0),
    // Vehicle resyncs its fixed-point position so the next tick does not pull
    // it back to where it was picked up (0042bb80), CallButton registers its
    // floor with its Lift (00429a70), SimpleObject and its
    // Bubble/PointerTool subclasses finish the placement (00426ee0), Creature
    // re-plants its feet (0040da20), and CompoundObject, Blackboard and
    // Scenery take the Object body, which updates the movement bounds and
    // queues event 8 (00425b00).
    if (edit_object_ == nullptr) {
        return;
    }
    WindowsCallButtonRuntimeHost world(*this);
    if (auto* lift = dynamic_cast<creatures1::objects::Lift*>(edit_object_)) {
        lift->update_bounds_and_queue_redraw(world);
        return;
    }
    if (auto* vehicle =
            dynamic_cast<creatures1::objects::Vehicle*>(edit_object_)) {
        vehicle->synchronize_fixed_point_position_and_queue_redraw(world,
                                                                    world);
        return;
    }
    if (auto* button =
            dynamic_cast<creatures1::objects::CallButton*>(edit_object_)) {
        button->update_lift_state_and_queue_redraw(world);
        return;
    }
    if (auto* simple =
            dynamic_cast<creatures1::objects::SimpleObject*>(edit_object_)) {
        WindowsSimpleObjectInteractionHost host(*this);
        simple->finalize_object_edit(host);
        return;
    }
    if (auto* creature = mutable_creature_for_object(*edit_object_)) {
        creature->queue_event_8_after_bounds_update(world, *this, world);
        return;
    }
    edit_object_->queue_event_8_after_bounds_update(world, world);
}

bool C1WindowsDocument::non_scenery_object_tick_enabled(std::size_t index) const {
    creatures1::objects::Object* object = non_scenery_object_at(index);
    if (object == nullptr) {
        throw std::out_of_range("C1 non-scenery object registry index");
    }
    return object->tick_enabled();
}

void C1WindowsDocument::tick_non_scenery_object(std::size_t index) {
    // SFCDoc::UpdateWorld calls Object vtable slot 41, Tick, on every
    // tick-enabled non-scenery object.  The port has that body on each
    // concrete type instead of one virtual, so the slot is resolved here --
    // most-derived first, because Lift overrides Vehicle overrides
    // CompoundObject and Bubble overrides SimpleObject, exactly as the native
    // vtables do.
    creatures1::objects::Object* object = non_scenery_object_at(index);
    if (object == nullptr) {
        throw std::out_of_range("C1 non-scenery object registry index");
    }

    // Creature is not itself an Object subtype in this port (it holds its
    // Skeleton -- which IS an Object -- as a member, per Skeleton's own
    // class comment); the registry stores &creature.skeleton(), so the
    // reverse lookup below is how a ticked Skeleton is recognized as
    // belonging to a live Creature.  See WindowsCreatureUpdateHost's class
    // comment for why this case was missing entirely.
    if (auto* creature = mutable_creature_for_object(*object)) {
        log_creature_step_probe(*creature, world_tick_count_);
        WindowsCreatureUpdateHost host(*this);
        creature->update(host, *this);
        return;
    }

    if (auto* lift = dynamic_cast<creatures1::objects::Lift*>(object)) {
        WindowsCallButtonRuntimeHost host(*this);
        lift->tick(host);
        return;
    }
    if (auto* vehicle = dynamic_cast<creatures1::objects::Vehicle*>(object)) {
        WindowsCallButtonRuntimeHost host(*this);
        vehicle->tick(host);
        return;
    }
    // Blackboard does not override slot 41: its vtable (0x00457554) keeps
    // CompoundObject::Tick @ 0x0042b6c0 there, so the board's timer and part
    // animations advance like any compound object.  Blackboard::Tick is slot
    // 42, run by the drive-threshold phase (run_creature_drive_threshold_phase).
    if (auto* compound =
            dynamic_cast<creatures1::objects::CompoundObject*>(object)) {
        WindowsCallButtonRuntimeHost host(*this);
        compound->tick(host);
        return;
    }
    if (auto* bubble = dynamic_cast<creatures1::objects::Bubble*>(object)) {
        WindowsSimpleObjectInteractionHost host(*this);
        bubble->tick(host);
        return;
    }
    if (auto* simple =
            dynamic_cast<creatures1::objects::SimpleObject*>(object)) {
        WindowsSimpleObjectInteractionHost host(*this);
        simple->tick(host);
        return;
    }
    // Object's slot 41 is UpdateSound @ 00426190.
    WindowsSimpleObjectInteractionHost host(*this);
    object->update_sound(host);
}

bool C1WindowsDocument::enqueue_text_input(char character) {
    // Native ring: 16 bytes, one always left empty to tell full from empty.
    constexpr std::size_t kTextInputRingCapacity = 15;
    if (pending_text_input_.size() >= kTextInputRingCapacity) {
        return false;
    }
    pending_text_input_.push_back(character);
    return true;
}

bool C1WindowsDocument::pop_text_input_character(char& character) {
    if (pending_text_input_.empty()) {
        return false;
    }
    character = pending_text_input_.front();
    pending_text_input_.pop_front();
    return true;
}

std::size_t C1WindowsDocument::text_input_length() const {
    return text_input_buffer_length_;
}

std::size_t C1WindowsDocument::text_input_max_length() const {
    return text_input_target_ == nullptr
               ? creatures1::ui::PointerTool::kTextInputMaximumLength
               : text_input_max_length_;
}

std::uint32_t C1WindowsDocument::text_input_allowed_character_flags() const {
    return text_input_target_ == nullptr
               ? creatures1::ui::PointerTool::kTextInputAllowedCharacters
               : text_input_allowed_flags_;
}

void C1WindowsDocument::configure_text_input(
    creatures1::objects::Object* target, std::size_t maximum_length,
    std::uint32_t allowed_characters) {
    // Blackboard::set_edit_mode configures the pointer tool back as the
    // target when leaving edit mode, which is the same as "not configured".
    if (target == nullptr || target == pointer_tool_) {
        reset_text_input_configuration();
        return;
    }
    text_input_target_ = target;
    text_input_max_length_ = maximum_length;
    text_input_allowed_flags_ = allowed_characters;
}

void C1WindowsDocument::reset_text_input_configuration() {
    text_input_target_ = nullptr;
    text_input_max_length_ = 0;
    text_input_allowed_flags_ = 0;
}

void C1WindowsDocument::clear_text_input_buffer() {
    text_input_buffer_[0] = '\0';
    text_input_buffer_length_ = 0;
}

bool C1WindowsDocument::text_input_character_allowed( char character, std::uint32_t allowed_flags) const {
    // SFCDoc::UpdateWorld @ 0x00432850: each flag admits one class, tested
    // with strchr against the executable's own character sets.
    const auto in_set = [character](const char* set) {
        return character != '\0' && std::strchr(set, character) != nullptr;
    };
    if ((allowed_flags & 0x01u) != 0 &&
        in_set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")) {
        return true;
    }
    if ((allowed_flags & 0x02u) != 0 && character == ' ') {
        return true;
    }
    if ((allowed_flags & 0x04u) != 0 && character == '?') {
        return true;
    }
    if ((allowed_flags & 0x08u) != 0 && character == '!') {
        return true;
    }
    if ((allowed_flags & 0x10u) != 0 && in_set("?!.,:/\\\x9c$%&*")) {
        return true;
    }
    return (allowed_flags & 0x20u) != 0 && in_set("0123456789");
}

void C1WindowsDocument::commit_text_input() {
    // SFCDoc::UpdateWorld @ 0x0043262b: Return calls vtable slot 43 on the
    // text-input target with the typed buffer, then clears the buffer.
    // PointerTool's slot 43 is SetTextWithToolbarUpdate @ 0x00429860 (say
    // it); Blackboard's is FinalizeTextInput @ 0x0042cb00.
    const std::string text(text_input_buffer_.data(), text_input_buffer_length_);
    if (auto* blackboard =
            dynamic_cast<creatures1::brain::Blackboard*>(text_input_target_)) {
        WindowsBlackboardHost host(*this);
        blackboard->finalize_text_input(text, host, host);
    } else if (pointer_tool_ != nullptr) {
        C1MainFrame* frame = active_main_frame();
        C1WindowsView* view =
            frame == nullptr ? nullptr : active_c1_view(*frame);
        WindowsPointerToolRuntimeHost runtime(*this, view);
        pointer_tool_->set_text_with_toolbar_update(text, runtime);
    }
    clear_text_input_buffer();
}

void C1WindowsDocument::erase_last_text_input_character() {
    if (text_input_buffer_length_ != 0) {
        --text_input_buffer_length_;
        text_input_buffer_[text_input_buffer_length_] = '\0';
    }
}

void C1WindowsDocument::append_text_input_character(char character) {
    // Native @ 0x004328d8 bounds the store at 0x50; the caller has already
    // checked the target's maximum length.
    if (text_input_buffer_length_ + 1 < text_input_buffer_.size()) {
        text_input_buffer_[text_input_buffer_length_++] = character;
        text_input_buffer_[text_input_buffer_length_] = '\0';
    }
}

void C1WindowsDocument::update_text_input_target() {
    // SFCDoc::UpdateWorld @ 0x00432679: after every edit, vtable slot 44 on
    // the target with the typed buffer.  PointerTool's slot 44 is
    // SetPersistentBubbleText @ 0x00429960; Blackboard's is
    // SetCurrentWordText @ 0x0042cb70.
    const std::string_view text(text_input_buffer_.data(),
                                text_input_buffer_length_);
    if (auto* blackboard =
            dynamic_cast<creatures1::brain::Blackboard*>(text_input_target_)) {
        WindowsBlackboardHost host(*this);
        blackboard->set_current_word_text(text, host);
        return;
    }
    if (pointer_tool_ == nullptr) {
        return;
    }
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view =
        frame == nullptr ? nullptr : active_c1_view(*frame);
    WindowsPointerToolRuntimeHost runtime(*this, view);
    pointer_tool_->set_persistent_bubble_text(text, runtime);
}


void C1WindowsDocument::update_selected_creature_follow_viewport() {
    // UpdateSelectedCreatureFollowViewport @ 004176e0 is CEyeView-owned; the
    // document only forwards the tick to the view that exists.
    if (eye_view_ != nullptr) {
        creatures1::ui::update_selected_creature_follow_viewport(*eye_view_);
    }
}


void C1WindowsDocument::execute_running_macro(std::size_t index) {
    // SFCDoc::UpdateWorld @ 00432770 walks g_running_macro_slots and calls
    // Macro::ExecuteInterpreter on each one, re-reading the count every
    // iteration so a macro that removes itself is handled.  Until this was
    // bound, a script only ever got the commands it could reach inside its
    // one start_execution call: the interpreter returns to its caller
    // whenever a command yields (for example `wait` or `over`), and
    // nothing ever resumed it. Conditional branch skips do not yield.
    if (index >= creatures1::scripting::g_running_macros.size()) {
        return;
    }
    creatures1::scripting::Macro* macro =
        creatures1::scripting::g_running_macros[index];
    if (macro == nullptr) {
        return;
    }
    WindowsMacroHost host(*this);
    macro->execute_interpreter(host.interpreter_bindings());
}

std::uint32_t C1WindowsDocument::creature_update_cohort() const {
    return creature_update_cohort_;
}

void C1WindowsDocument::set_creature_update_cohort(std::uint32_t cohort) {
    creature_update_cohort_ = cohort;
}

bool C1WindowsDocument::creature_tick_enabled(std::size_t index) const {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr) {
        throw std::out_of_range("C1 creature registry index");
    }
    return creature->tick_enabled();
}

bool C1WindowsDocument::creature_is_dreaming(std::size_t index) const {
    auto* creature = dynamic_cast<const creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr) {
        throw std::out_of_range("C1 creature registry index");
    }
    return creature->instinct_runtime_state().dream_countdown != 0;
}

bool C1WindowsDocument::creature_is_alive(std::size_t index) const {
    auto* creature = dynamic_cast<const creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr) {
        throw std::out_of_range("C1 creature registry index");
    }
    return creature->life_state() ==
           creatures1::creatures::CreatureLifeState::alive;
}

namespace {

// SFCDoc::UpdateWorld reads the selected action as `(int)(char)` before
// comparing it with the Decision lobe's neuron count, so the id is a signed
// byte at this boundary even though the Creature keeps it widened.
int native_selected_action_index(
    const creatures1::creatures::Creature& creature) {
    return static_cast<signed char>(
        static_cast<std::uint8_t>(creature.selected_action_id()));
}

} // namespace

bool C1WindowsDocument::selected_action_is_in_range(std::size_t index) const {
    const auto* creature = dynamic_cast<const creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr || creature->brain() == nullptr) {
        return false;
    }
    return native_selected_action_index(*creature) <
           static_cast<int>(creature->brain()
                                ->lobe(static_cast<std::uint32_t>(
                                    creatures1::brain::StandardLobeIndex::
                                        decision))
                                .neuron_count_value());
}

void C1WindowsDocument::boost_selected_action_activation(std::size_t index) {
    // Native SFCDoc::UpdateWorld loads the byte boost, doubles it with
    // `add al, al`, then adds the result to the selected Decision-lobe
    // neuron. Lobe::add_neuron_activation supplies the final 0xff saturation.
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr || creature->brain() == nullptr) {
        return;
    }
    const int selected = native_selected_action_index(*creature);
    if (selected < 0) {
        // The native range check is a signed `<`, so a negative id passes it
        // and then indexes the neuron array from below.  That out-of-bounds
        // write is a native defect, not behaviour worth reproducing.
        return;
    }
    const std::uint8_t native_boost = static_cast<std::uint8_t>(
        creature->action_activation_boost() +
        creature->action_activation_boost());
    creature->brain()
        ->lobe(static_cast<std::uint32_t>(
            creatures1::brain::StandardLobeIndex::decision))
        .add_neuron_activation(static_cast<std::uint32_t>(selected),
                               native_boost);
}

void C1WindowsDocument::update_creature_brain(std::size_t index) {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr || creature->brain() == nullptr) {
        return;
    }
    creature->brain()->update(creature->biochemistry_tick());
}

void C1WindowsDocument::update_creature_biochemistry(std::size_t index) {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr || creature->biochemistry() == nullptr) {
        return;
    }
    creature->biochemistry()->update(creature->biochemistry_tick());
}

void C1WindowsDocument::update_creature_action_selection(std::size_t index) {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureAttentionHost attention(*this);
    WindowsCreatureSpeechHost speech(*this);
    WindowsCreatureSpeechPhraseHost phrase(*this);
    creature->update_action_selection(attention, *this, phrase, speech);
}

void C1WindowsDocument::increment_creature_biochemistry_tick(std::size_t index) {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr) {
        throw std::out_of_range("C1 creature registry index");
    }
    creature->increment_biochemistry_tick();
}

void C1WindowsDocument::process_creature_dreaming(std::size_t index) {
    auto* creature = dynamic_cast<creatures1::creatures::Creature*>(
        creature_at(index));
    if (creature == nullptr) {
        throw std::out_of_range("C1 creature registry index");
    }
    creature->process_dreaming(creature == selected_creature());
}

bool C1WindowsDocument::sound_manager_available() const {
    return g_active_sound_manager != nullptr;
}

creatures1::world::ViewportBounds C1WindowsDocument::sound_viewport() const {
    if (renderer_ == nullptr) {
        return {0, 0, creatures1::world::kWorldWidth,
                creatures1::world::kWorldHeight};
    }
    return {renderer_->viewport_left(), renderer_->viewport_top(),
            renderer_->viewport_right(), renderer_->viewport_bottom()};
}

bool C1WindowsDocument::sounds_muted() const { return sound_muted(); }


creatures1::sound::SoundManager& C1WindowsDocument::sound_manager() {
    if (g_active_sound_manager == nullptr) {
        throw std::logic_error("C1 sound manager is not initialized");
    }
    return *g_active_sound_manager;
}

bool C1WindowsDocument::debug_console_available() const { return false; }


void C1WindowsDocument::log_sound_event( creatures1::objects::ObjectSoundPlaybackHost::LogEvent event, creatures1::sound::SoundId sound_id, int value) {
    static_cast<void>(event);
    static_cast<void>(sound_id);
    static_cast<void>(value);
}

const char* C1WindowsDocument::find_character(const char* text, unsigned char character) const {
    return creatures1::platform::c1_multibyte_find_character(text,
                                                              character);
}

const char* C1WindowsDocument::find_substring(const char* text, const char* fragment) const {
    return creatures1::platform::c1_multibyte_find_substring(text,
                                                               fragment);
}

const char* C1WindowsDocument::next_character(const char* text) const {
    return creatures1::platform::c1_multibyte_next_character(text);
}

int C1WindowsDocument::compare_strings(const char* left, const char* right) const {
    return creatures1::platform::c1_multibyte_compare_strings(left, right);
}

void C1WindowsDocument::copy_string(char* destination, std::size_t destination_capacity, const char* source) const {
    creatures1::platform::c1_multibyte_copy_string(
        destination, destination_capacity, source);
}

void C1WindowsDocument::append_string(char* destination, std::size_t destination_capacity, const char* source) const {
    creatures1::platform::c1_multibyte_append_string(
        destination, destination_capacity, source);
}

bool C1WindowsDocument::contains(const creatures1::platform::Rectangle& rectangle, std::int32_t x, std::int32_t y) const {
    return x >= rectangle.left && x < rectangle.right &&
           y >= rectangle.top && y < rectangle.bottom;
}

void C1WindowsDocument::update_sound_system() {
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->update();
    }
}

std::uint32_t C1WindowsDocument::ambient_sound_cooldown_ticks() const {
    return ambient_sound_cooldown_ticks_;
}

void C1WindowsDocument::set_ambient_sound_cooldown_ticks(std::uint32_t ticks) {
    ambient_sound_cooldown_ticks_ = ticks;
}

bool C1WindowsDocument::sound_muted() const {
    return semantic_document_ != nullptr && semantic_document_->mute_setting;
}

std::uint32_t C1WindowsDocument::random_ambient_sound_index() {
    return static_cast<std::uint32_t>(std::rand());
}

std::uint32_t C1WindowsDocument::ambient_sound_descriptor_override() const {
    return g_active_sound_manager == nullptr
               ? 0
               : g_active_sound_manager->sound_descriptor_override();
}

bool C1WindowsDocument::sound_mixer_ready() const {
    return g_active_sound_manager != nullptr &&
           g_active_sound_manager->backend_ready() &&
           !g_active_sound_manager->mixer_suspended();
}

bool C1WindowsDocument::load_ambient_sound(std::uint32_t sound_id) {
    return g_active_sound_manager != nullptr &&
           g_active_sound_manager->find_or_load_cache_entry(sound_id) !=
               nullptr;
}

void C1WindowsDocument::start_ambient_sound(std::uint32_t sound_id) {
    if (g_active_sound_manager == nullptr) {
        return;
    }
    auto* cached = g_active_sound_manager->find_or_load_cache_entry(sound_id);
    if (cached != nullptr) {
        g_active_sound_manager->start_channel(cached, 0, 0, false);
    }
}

bool C1WindowsDocument::advance_smooth_scroll() {
    if (renderer_ == nullptr) {
        return false;
    }
    return renderer_->advance_smooth_scroll();
}

bool C1WindowsDocument::selected_creature_in_safe_area() const {
    return renderer_ != nullptr &&
           renderer_->is_selected_creature_within_safe_area();
}

void C1WindowsDocument::set_manual_navigation_safe_frame_count( std::uint32_t frame_count) {
    manual_navigation_safe_frame_count_ = frame_count;
    set_world_view_safe_frame(world_view_, frame_count);
}

void C1WindowsDocument::follow_selected_creature_viewport() {
    if (renderer_ != nullptr) {
        renderer_->follow_selected_creature_viewport();
    }
}

std::uint32_t C1WindowsDocument::world_tick_phase() const { return world_tick_phase_; }


void C1WindowsDocument::set_world_tick_phase(std::uint32_t phase) {
    world_tick_phase_ = phase;
}

void C1WindowsDocument::dispatch_world_tick_phase(std::uint32_t phase) {
    // SFCDoc::UpdateWorld @ 004324e0 dispatches through the sixteen-entry
    // table at 0x00454640 with `call [phase*4 + 0x454640]`, unconditionally
    // and with no null check.  Four of the sixteen slots hold the no-op
    // `guard_check_icall` thunk, so those phases genuinely do nothing.
    //
    //   0, 4, 8, 12 -> ProcessQueuedObjectEventsAndStimuli @ 00432d20
    //   1, 5, 9, 13 -> UpdateAllCreatureBrainInputs         @ 00432ee0
    //   2, 10       -> UpdateAllCreaturePerceptionAndAttention @ 00433020
    //   7           -> UpdateAllCreatureDriveThresholdStates   @ 00432ce0
    //   15          -> AdvanceBacteriumServicePhase            @ 00433310
    //   3, 6, 11, 14 -> idle
    switch (phase) {
    case 0:
    case 4:
    case 8:
    case 12: {
        WindowsObjectEventRuntime runtime(*this);
        event_scheduler_.process_queued_events(
            runtime, static_cast<std::int32_t>(world_tick_count()));
        return;
    }
    case 1:
    case 5:
    case 9:
    case 13: {
        WindowsCreatureWorldUpdateHost host(*this);
        creatures1::creatures::update_all_creature_brain_inputs(host);
        return;
    }
    case 2:
    case 10: {
        WindowsCreatureWorldUpdateHost host(*this);
        creatures1::creatures::update_all_creature_perception_and_attention(
            host);
        return;
    }
    case 7:
        run_creature_drive_threshold_phase(*this);
        return;
    case 15: {
        WindowsBacteriumServiceHost host(*this);
        creatures1::world::advance_bacterium_service_phase(host);
        return;
    }
    default:
        // Phases 3, 6, 11 and 14 are the table's no-op slots.
        return;
    }
}

void C1WindowsDocument::begin_deferred_dirty_rectangles() {
    if (renderer_ != nullptr) {
        renderer_->begin_deferred_dirty_rectangles();
    }
}

void C1WindowsDocument::flush_deferred_dirty_rectangles() {
    // This used to Invalidate the whole view, which made every world tick
    // repaint the entire viewport and rendered the dirty-rectangle machinery
    // pointless.  Native presents only the rectangles the tick actually
    // queued.
    if (renderer_ != nullptr) {
        renderer_->flush_deferred_dirty_rectangles();
    }
}

std::size_t C1WindowsDocument::selected_creature_count() const {
    return selection_.size();
}

void C1WindowsDocument::set_world_tick_count(std::uint32_t count) {
    world_tick_count_ = count;
}

void C1WindowsDocument::publish_periodic_score_to_embedded_control(
    const creatures1::application::DocumentScore& score) {
    // NotifyDDEScoreChanged @ 0042f740 is the same call SFCDoc::UpdateWorld
    // @ 004324e0 makes on the periodic tick.  It is a notification, not a
    // payload: embedded record 8 is invoked at DISPID 1 with a VT_BOOL return
    // and two fixed I4 arguments, and the kit reads the score itself.  The
    // score parameter is therefore deliberately unused here.
    static_cast<void>(score);
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return;
    }
    constexpr std::size_t kScoreNotifyKitIndex = 8;
    COleDispatchDriver& driver =
        frame->embedded_kit_dispatch(kScoreNotifyKitIndex);
    if (driver.m_lpDispatch == nullptr) {
        return;
    }
    // The call shape is shared with FlushFuneralKitDocumentStateWords
    // @ 00435c10: a data-kind header and one payload.  Here the native's
    // payload is the address of the literal string "Dummy" at 00458170, passed
    // as a VT_I4 the kit never dereferences, so zero carries the same meaning.
    invoke_kit_communicate(
        driver,
        {creatures1::application::embedded_kit_message_header(
             creatures1::application::EmbeddedKitMessageKind::data,
             creatures1::application::kEmbeddedKitDataCode),
         0});
}


std::uint32_t C1WindowsDocument::current_time_ms() const { return GetTickCount(); }


std::uint32_t C1WindowsDocument::autosave_interval_ms() const {
    return g_active_app_state == nullptr
               ? 360000
               : g_active_app_state->autosave_interval_ms;
}

void C1WindowsDocument::broadcast_embedded_control_state(std::uint32_t state) {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return;
    }
    // The recovered broadcast walks all twenty slots and invokes DISPID 1
    // only where the slot holds an IDispatch.  The walk and the null guard
    // are application policy; the COM invoke is this boundary.
    class FrameKitControl final
        : public creatures1::application::EmbeddedKitControlApi {
    public:
        explicit FrameKitControl(C1MainFrame& frame) : frame_(frame) {}

        bool has_dispatch(std::size_t tool_index) const override {
            return frame_.embedded_kit_dispatch(tool_index).m_lpDispatch !=
                   nullptr;
        }

        bool send_kit_message(
            std::size_t tool_index,
            const creatures1::application::EmbeddedKitMessage& message)
            override {
            return invoke_kit_communicate(
                frame_.embedded_kit_dispatch(tool_index), message);
        }

    private:
        C1MainFrame& frame_;
    } control(*frame);

    creatures1::application::broadcast_embedded_control_state(
        control, static_cast<std::uint8_t>(state));
}


std::string C1WindowsDocument::capture_main_window_title() const {
    CWnd* main_window = AfxGetMainWnd();
    if (main_window == nullptr || main_window->GetSafeHwnd() == nullptr) {
        return {};
    }
    CStringA title;
    main_window->GetWindowTextA(title);
    return std::string(title.GetString(), title.GetLength());
}

void C1WindowsDocument::set_temporary_main_window_title() {
    CWnd* main_window = AfxGetMainWnd();
    if (main_window != nullptr && main_window->GetSafeHwnd() != nullptr) {
        main_window->SetWindowTextA("Creatures");
    }
}

void C1WindowsDocument::set_application_busy(bool busy) {
    if (busy) {
        AfxGetApp()->BeginWaitCursor();
    } else {
        AfxGetApp()->EndWaitCursor();
    }
}

bool C1WindowsDocument::save_for_autosave( creatures1::application::Document& document) {
    if (semantic_document_.get() != &document) {
        return false;
    }
    const CString path = GetPathName();
    if (path.IsEmpty()) {
        return false;
    }
    const CStringA native_path(path);
    return document.save(
        *this,
        std::string_view(native_path.GetString(), native_path.GetLength()));
}

void C1WindowsDocument::refresh_temporary_world_backup_for_update() {
    refresh_temporary_world_backup();
}

void C1WindowsDocument::restore_main_window_title(std::string_view title) {
    CWnd* main_window = AfxGetMainWnd();
    if (main_window != nullptr && main_window->GetSafeHwnd() != nullptr) {
        main_window->SetWindowTextA(CStringA(std::string(title).c_str()));
    }
}

void C1WindowsDocument::kill_world_update_timer() {
    CWnd* main_window = AfxGetMainWnd();
    if (main_window != nullptr && main_window->GetSafeHwnd() != nullptr) {
        main_window->KillTimer(kWorldUpdateTimerId);
    }
}

bool C1WindowsDocument::initialize_framework_new_document( creatures1::application::Document& /*document*/) {
    SetModifiedFlag(FALSE);
    return true;
}

bool C1WindowsDocument::initialize_game_palette() {
    ensure_resource_hosts();
    if (palette_platform_ == nullptr || palette_files_ == nullptr) {
        return false;
    }
    return creatures1::display::initialize_game_palette(
        game_palette_, *palette_files_, *palette_platform_,
        resource_paths_[kPaletteDirectoryIndex]);
}

void C1WindowsDocument::seed_random_from_current_time() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

void C1WindowsDocument::load_charset_data() {
    ensure_resource_hosts();
    if (resources_ != nullptr) {
        creatures1::display::load_charset_data(
            charset_, image_cache_, *resources_,
            resource_paths_[kImageDirectoryIndex]);
    }
}

void C1WindowsDocument::create_legacy_world() {
    ensure_resource_hosts();
    world_runtime_ = std::make_unique<creatures1::world::WorldRuntime>();
    creatures1::scripting::initialize_script_definition_table();

    world_runtime_->map_data().initialize_new_world(
        creatures1::world::kInitialWorldRooms.data(),
        creatures1::world::kInitialWorldRooms.size(), nullptr);

    if (resources_ != nullptr) {
        creatures1::display::Gallery* background =
            creatures1::display::acquire_gallery(
                0x6b636142, 0, 0x1d0, false,
                secondary_resource_directory(kImageDirectoryIndex),
                resource_paths_[kImageDirectoryIndex], *resources_,
                *world_runtime_);
        world_runtime_->map_data().set_background_gallery(background);
    }
}

void C1WindowsDocument::create_pointer_tool( const creatures1::application::PointerToolInitialization& init) {
    if (world_runtime_ == nullptr) {
        return;
    }
    // SFCDoc::DeleteContents deletes every non-scenery registry entry, the
    // pointer tool included, so the world runtime owns it exactly like any
    // other object and the document only keeps a borrowed view.
    auto owned_tool = std::make_unique<creatures1::ui::PointerTool>(
        init.sprite_file_id, init.header_record_index, init.image_count,
        init.cache_protected, init.initial_x, init.initial_y,
        init.render_plane, init.bounds_flags, init.classifier,
        init.click_event_selector, init.reserved_word_0,
        init.reserved_word_1, init.interaction_event_flags, *this);
    pointer_tool_ = owned_tool.get();
    world_runtime_->adopt_non_scenery_object(std::move(owned_tool));
    pointer_tool_->cursor_hotspot_offset_x = 2;
    pointer_tool_->cursor_hotspot_offset_y = 2;
    pointer_tool_->initialize_unbounded_object_placement(*this);
}

void C1WindowsDocument::install_builtin_script( creatures1::scripting::ScriptClassifier classifier, std::string_view text) {
    (void)creatures1::scripting::install_script_text_for_classifier(
        classifier, text, false, *this);
}

std::uint32_t C1WindowsDocument::world_update_timer_interval_ms() const {
    return world_update_timer_interval_ms_;
}

void C1WindowsDocument::set_world_update_timer_interval_ms( std::uint32_t interval_ms) {
    world_update_timer_interval_ms_ = interval_ms;
}

bool C1WindowsDocument::has_main_frame() const {
    // SFCDoc::OnOpenDocument @ 0x004309a0 tests g_CMainFrame, which the frame
    // publishes when it is constructed.  AfxGetMainWnd reads CWinApp's
    // m_pMainWnd, and MFC only sets that later, in InitialUpdateFrame -- so
    // asking it here reports "no frame" for the whole of document open, and
    // the world update timer is never armed at all.
    const C1MainFrame* frame = active_main_frame();
    return frame != nullptr && frame->GetSafeHwnd() != nullptr;
}

void C1WindowsDocument::arm_world_update_timer(std::uint32_t interval_ms) {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr && frame->GetSafeHwnd() != nullptr) {
        frame->SetTimer(kWorldUpdateTimerId, interval_ms, nullptr);
    }
}

void C1WindowsDocument::report_archive_not_loading() {
    // Not a debug assertion: DeserializeScriptsForClassifier @ 0041a8e0 calls
    // AfxThrowArchiveException with cause 4, CArchiveException::writeOnly,
    // naming the archive file.  A read attempted on a write-only archive is a
    // hard error in the shipped image, so it stays one here.
    AfxThrowArchiveException(CArchiveException::writeOnly, GetPathName());
}


bool C1WindowsDocument::confirm_script_replacement( creatures1::scripting::ScriptClassifier /*classifier*/, std::string_view /*old_text*/, std::string_view /*new_text*/) {
    return false;
}

void C1WindowsDocument::report_script_table_full() {
    // Recovered InstallScriptTextForClassifier @ 0041a440: when the 2000-entry
    // script table is full it loads string 0xef34 and shows it with MB_OK.
    // 61236 is "The Scriptorium is full, no more object scripts can be added."
    AfxMessageBox(static_cast<UINT>(61236), MB_OK, 0);
}


creatures1::display::Gallery* C1WindowsDocument::acquire_gallery( std::uint32_t sprite_file_id, int header_record_index, std::uint32_t image_count, bool cache_protected) {
    ensure_resource_hosts();
    if (resources_ == nullptr || world_runtime_ == nullptr) {
        return nullptr;
    }
    return creatures1::display::acquire_gallery(
        sprite_file_id, header_record_index, image_count, cache_protected,
        secondary_resource_directory(kImageDirectoryIndex),
        resource_paths_[kImageDirectoryIndex], *resources_,
        *world_runtime_);
}

creatures1::objects::EntityRegistryHost& C1WindowsDocument::entity_registry() {
    return *world_runtime_;
}

void C1WindowsDocument::update_movement_bounds( creatures1::objects::SimpleObject& object) {
    object.update_movement_bounds(*this);
}

creatures1::objects::ObjectRenderableSetHost& C1WindowsDocument::renderables() {
    return *world_runtime_;
}

// --- SimpleObjectBubbleHost -----------------------------------------------

int C1WindowsDocument::viewport_left() const {
    return renderer_viewport_left();
}

int C1WindowsDocument::viewport_right() const {
    return renderer_viewport_left() + renderer_viewport_width();
}

creatures1::objects::BubbleConstructionHost&
C1WindowsDocument::bubble_construction() {
    return *this;
}

void C1WindowsDocument::adopt_speech_bubble(
    std::unique_ptr<creatures1::objects::Bubble> bubble) {
    if (world_runtime_ != nullptr && bubble != nullptr) {
        world_runtime_->adopt_non_scenery_object(std::move(bubble));
    }
}

void C1WindowsDocument::adopt_sleep_indicator(
    std::unique_ptr<creatures1::objects::SimpleObject> indicator) {
    if (world_runtime_ != nullptr && indicator != nullptr) {
        world_runtime_->adopt_non_scenery_object(std::move(indicator));
    }
}

bool C1WindowsDocument::burble_is_enabled() const {
    HKEY key = nullptr;
    std::uint32_t value = 0;
    if (creatures1::platform::open_c1_secondary_registry(key, KEY_READ)) {
        creatures1::platform::read_registry_dword(key, "Burble", value);
        RegCloseKey(key);
    }
    return value != 0;
}

// --- EntityRasterHost -----------------------------------------------------

std::uint8_t* C1WindowsDocument::current_image_pixels(
    creatures1::objects::Entity& entity, int& out_width, int& out_height) {
    out_width = 0;
    out_height = 0;
    creatures1::display::Gallery* gallery = entity.gallery();
    if (gallery == nullptr || resources_ == nullptr) {
        return nullptr;
    }
    if (gallery->images == nullptr ||
        entity.current_image_index() >= gallery->image_count) {
        return nullptr;
    }

    creatures1::display::Image& image =
        gallery->images[entity.current_image_index()];
    std::uint8_t* pixels = image.get_pixel_data(
        pixel_cache_, sprite_files_,
        sprite_file_search_paths(),
        *resources_);
    if (pixels == nullptr) {
        return nullptr;
    }
    out_width = image.width();
    out_height = image.height();
    return pixels;
}

void C1WindowsDocument::preload_image(
    const creatures1::display::Image& image) {
    if (resources_ == nullptr) {
        return;
    }

    // Entity's preload walk is const because it only selects images.  The
    // cache operation itself is mutable state owned by the document.
    const_cast<creatures1::display::Image&>(image).get_pixel_data(
        pixel_cache_, sprite_files_,
        sprite_file_search_paths(),
        *resources_);
}

const std::uint8_t* C1WindowsDocument::charset_glyph_rows(
    std::uint8_t character_code) const {
    // 128 glyphs of 12 rows by 6 columns fill the 0x2400-byte raster block.
    constexpr std::size_t kGlyphStride = 12 * 6;
    const std::size_t offset =
        static_cast<std::size_t>(character_code) * kGlyphStride;
    if (offset + kGlyphStride > charset_.glyph_raster_data.size()) {
        return nullptr;
    }
    return charset_.glyph_raster_data.data() + offset;
}

int C1WindowsDocument::charset_glyph_advance_width(
    std::uint8_t character_code) const {
    return character_code < charset_.glyph_advance_widths.size()
               ? static_cast<int>(
                     charset_.glyph_advance_widths[character_code])
               : 0;
}

// --- BubbleRedrawHost / BubbleTextHost ------------------------------------

void C1WindowsDocument::invoke_bubble_deleting(
    creatures1::objects::Bubble& bubble, std::uint32_t deletion_flags) {
    // Bubble::Tick @ 0042a4c0 calls the deleting destructor
    // (~Bubble_Deleting @ 00429d60, flags 1): the bubble is freed on the spot
    // and ~Object drops it from the object registries.  Leaving this empty
    // kept every expired bubble alive, so speech boxes piled up in the world
    // and were saved with it.  A bubble still under construction is not yet
    // owned by the runtime and has nothing to release.
    static_cast<void>(deletion_flags);
    if (world_runtime_ != nullptr &&
        world_runtime_->owns_non_scenery_object(bubble)) {
        world_runtime_->destroy_world_object(bubble);
    }
}

void C1WindowsDocument::redraw_after_bubble_deleting(
    const creatures1::world::WorldRect& bubble_bounds) {
    queue_renderer_dirty_world_rect(bubble_bounds);
}

std::uint16_t C1WindowsDocument::measure_bubble_text_width(
    std::string_view text) const {
    // The native walks the multibyte string adding each glyph advance plus a
    // one-pixel gap, which is what Entity's own glyph loop then steps by.
    unsigned int width = 0;
    for (const char character : text) {
        if (character == '\0') {
            break;
        }
        width += static_cast<unsigned int>(charset_glyph_advance_width(
                     static_cast<std::uint8_t>(character))) +
                 1u;
    }
    return static_cast<std::uint16_t>(width);
}

void C1WindowsDocument::clear_bubble_text_band(
    creatures1::objects::Bubble& bubble) {
    creatures1::objects::Entity* entity = bubble.entity();
    if (entity == nullptr) {
        return;
    }
    // Rows 3 through 14 inclusive, 0x90 pixels wide from x=6: the exact strip
    // the native memsets to palette index 0xf2 before drawing.
    entity->fill_current_image_rect(0xf2, 6, 3, 6 + 0x90, 0x0f, *this);
}

void C1WindowsDocument::draw_bubble_text(
    creatures1::objects::Bubble& bubble, int x, int y, std::string_view text,
    std::uint8_t background, std::uint8_t foreground, std::uint8_t shadow) {
    creatures1::objects::Entity* entity = bubble.entity();
    if (entity == nullptr) {
        return;
    }
    const std::string terminated(text);
    entity->draw_text_to_current_image(x, y, terminated.c_str(), background,
                                       foreground, shadow, *this);
}

void C1WindowsDocument::redraw_after_bubble_text(
    const creatures1::world::WorldRect& bubble_bounds) {
    queue_renderer_dirty_world_rect(bubble_bounds);
}

void C1WindowsDocument::redraw_after_simple_object_move( creatures1::objects::SimpleObject& /*object*/, const creatures1::world::WorldRect& old_bounds, const creatures1::world::WorldRect& new_bounds) {
    // SimpleObject::MoveToAndRedraw @ 0x00426f90 dirties the rectangle the
    // object left and the one it arrived at, unioning them when they overlap.
    // queue_dirty_world_rect already unions overlapping queued rectangles, so
    // offering both gives the same coverage.
    queue_renderer_dirty_world_rect(old_bounds);
    queue_renderer_dirty_world_rect(new_bounds);
}

void C1WindowsDocument::redraw_after_compound_object_move(
    creatures1::objects::CompoundObject& /*object*/,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    queue_renderer_dirty_world_rect(old_bounds);
    queue_renderer_dirty_world_rect(new_bounds);
}

void C1WindowsDocument::queue_compound_object_dirty_rect(
    creatures1::objects::CompoundObject& /*object*/,
    const creatures1::world::WorldRect& primary_bounds) {
    queue_renderer_dirty_world_rect(primary_bounds);
}

int C1WindowsDocument::mouse_client_x() const {
    // g_SFCView->mouse_client_x: the view owns the pointer position, updated
    // on WM_MOUSEMOVE.  It reads zero until the mouse first enters the view,
    // which is what the native starts from too.
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    return view == nullptr ? 0 : view->view_state().mouse_client_x;
}

int C1WindowsDocument::mouse_client_y() const {
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    return view == nullptr ? 0 : view->view_state().mouse_client_y;
}

// Every native site that wants the pointer in world space adds the viewport
// to the view's client position, and wraps at the world width separately
// where it needs to; these do the sum and leave the wrap to the caller, as
// SimpleObject::UpdateUnboundedPositionAndRedraw @ 00427fd0 does.  These used
// to answer a copy of the pointer tool's initial position that nothing ever
// updated, so the pointer never moved as far as any of their callers could
// tell.
int C1WindowsDocument::mouse_world_x() const {
    return mouse_client_x() + renderer_viewport_left();
}


int C1WindowsDocument::mouse_world_y() const {
    return mouse_client_y() + renderer_viewport_top();
}


void C1WindowsDocument::clear_edit_object() { edit_object_ = nullptr; }

void C1WindowsDocument::set_edit_object(
    creatures1::objects::Object* object) {
    edit_object_ = object;
}

std::int32_t C1WindowsDocument::map_ground_height(
    std::uint32_t x_block) const {
    if (world_runtime_ == nullptr) {
        return 0;
    }
    const creatures1::world::MapData& map = world_runtime_->map_data();
    return map.ground_height(static_cast<std::size_t>(x_block));
}

std::uint32_t C1WindowsDocument::map_room_count() const {
    return world_runtime_ == nullptr
               ? 0u
               : static_cast<std::uint32_t>(
                     world_runtime_->map_data().room_count_value());
}

std::int32_t C1WindowsDocument::map_room_value(std::uint32_t room_index,
                                               std::uint32_t field) const {
    if (world_runtime_ == nullptr) {
        return 0;
    }
    const creatures1::world::MapRoomTable rooms =
        world_runtime_->map_data().room_table();
    if (room_index >= rooms.room_count) {
        return 0;
    }
    const creatures1::world::MapRoom& room = rooms.rooms[room_index];
    // The recovered `room` command reads the four bounds words then the type.
    switch (field) {
    case 0: return room.bounds.left;
    case 1: return room.bounds.top;
    case 2: return room.bounds.right;
    case 3: return room.bounds.bottom;
    case 4: return static_cast<std::int32_t>(room.room_type);
    default: return 0;
    }
}

creatures1::objects::ObjectRegistryHost&
C1WindowsDocument::object_registry() {
    return *world_runtime_;
}

void C1WindowsDocument::set_renderer_viewport_origin(int world_x,
                                                     int world_y) {

    if (renderer_ != nullptr) {
        renderer_->set_viewport_origin(world_x, world_y);
    }
}

creatures1::scripting::DdeSystemInfoSnapshot
C1WindowsDocument::system_info_snapshot() const {
    creatures1::scripting::DdeSystemInfoSnapshot info{};
    info.non_scenery_object_count =
        static_cast<std::int32_t>(non_scenery_object_count());
    info.entity_count = static_cast<std::int32_t>(entity_count());
    info.creature_count = static_cast<std::int32_t>(creature_count());
    info.script_definition_count = static_cast<std::int32_t>(
        creatures1::scripting::g_script_definition_count);
    info.running_macro_count =
        static_cast<std::int32_t>(running_macro_count());
    info.gallery_count = static_cast<std::int32_t>(gallery_count());
    // g_image_cache_entry_count and g_image_cache_bytes_used are the sprite
    // pixel cache's counters -- the ones LoadPixelData maintains.  image_cache_
    // is the charset load's state and is empty for the whole run, so reading
    // it reported nought resident however many sprites were.
    info.image_cache_entry_count =
        static_cast<std::int32_t>(pixel_cache_.entry_count);
    info.image_cache_bytes =
        static_cast<std::int32_t>(pixel_cache_.bytes_used);
    info.room_count = static_cast<std::int32_t>(map_room_count());
    if (world_runtime_ != nullptr) {
        info.ambient_environment_index = static_cast<std::int32_t>(
            world_runtime_->map_data().ambient_environment_index_value());
    }
    // The report's last four values.  The native reads them straight off
    // g_selected_creature and g_smoothed_idle_cycle_index, without checking
    // that a creature is selected; with none selected there is nothing to
    // read, so they stay zero here rather than fault.
    if (const creatures1::creatures::Creature* creature = selected_creature()) {
        // Both are printed through %d after a cast to signed char, so a
        // boost above 0x7f reports negative exactly as the native does.
        info.selected_action_id = static_cast<std::int32_t>(
            static_cast<signed char>(creature->selected_action_id() & 0xffu));
        info.selected_action_activation_boost = static_cast<std::int32_t>(
            static_cast<signed char>(creature->action_activation_boost()));
        // The native prints the motion link pointer itself through %d: for a
        // debug readout what matters is which object it is, or none at all.
        info.selected_creature_motion_link = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(creature->motion_link()));
    }
    if (g_active_app_state != nullptr) {
        info.smoothed_idle_cycle_index =
            g_active_app_state->idle_cadence.smoothed_idle_cycle;
    }
    return info;
}

std::int32_t C1WindowsDocument::map_room_index_at(int world_x,
                                                  int world_y) const {
    if (world_runtime_ == nullptr) {
        return -1;
    }
    const creatures1::world::MapRoomTable rooms =
        world_runtime_->map_data().room_table();
    for (std::size_t index = 0; index < rooms.room_count; ++index) {
        const creatures1::world::MapRectangle& bounds = rooms.rooms[index].bounds;
        if (world_x >= bounds.left && world_x < bounds.right &&
            world_y >= bounds.top && world_y < bounds.bottom) {
            return static_cast<std::int32_t>(index);
        }
    }
    return -1;
}

void C1WindowsDocument::queue_immediate_object_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    event_scheduler_.queue_object_event(
        &source, &target, event_id, argument, 0, 0,
        static_cast<std::int32_t>(world_tick_count()));
}

void C1WindowsDocument::queue_object_event_with_delay(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument,
    std::int32_t delay_world_ticks) {
    event_scheduler_.queue_object_event(
        &source, &target, event_id, argument, 0, delay_world_ticks,
        static_cast<std::int32_t>(world_tick_count()));
}

void C1WindowsDocument::queue_creature_stimulus(
    const creatures1::objects::QueuedCreatureStimulus& stimulus) {
    event_scheduler_.queue_creature_stimulus(stimulus);
}

void C1WindowsDocument::adjust_document_score(
    creatures1::scripting::DdeScoreCounter counter, std::int32_t delta) {
    if (semantic_document_ == nullptr) {
        return;
    }
    creatures1::application::DocumentScore& score = semantic_document_->score;
    std::uint32_t* field = nullptr;
    switch (counter) {
    case creatures1::scripting::DdeScoreCounter::living_norns:
        field = &score.living_norns;
        break;
    case creatures1::scripting::DdeScoreCounter::dead_norns:
        field = &score.dead_norns;
        break;
    case creatures1::scripting::DdeScoreCounter::natural_eggs_laid:
        field = &score.natural_eggs_laid;
        break;
    case creatures1::scripting::DdeScoreCounter::hatchery_eggs_used:
        field = &score.hatchery_eggs_used;
        break;
    }
    if (field == nullptr) {
        return;
    }
    const std::int64_t updated = static_cast<std::int64_t>(*field) + delta;
    *field = static_cast<std::uint32_t>(updated < 0 ? 0 : updated);
}

void C1WindowsDocument::increment_living_norn_score() {
    if (semantic_document_ != nullptr) {
        ++semantic_document_->score.living_norns;
    }
}

void C1WindowsDocument::decrement_living_norn_score() {
    // Native decrements unconditionally; the unsigned score is kept from
    // wrapping to four billion if it is already zero.
    if (semantic_document_ != nullptr &&
        semantic_document_->score.living_norns > 0) {
        --semantic_document_->score.living_norns;
    }
}

std::uint32_t C1WindowsDocument::document_score_value(
    std::uint32_t index) const {
    if (semantic_document_ == nullptr) {
        return 0;
    }
    const creatures1::application::DocumentScore& score =
        semantic_document_->score;
    switch (index) {
    case 0: return score.hatchery_eggs_used;
    case 1: return score.natural_eggs_laid;
    case 2: return score.dead_norns;
    case 3: return score.living_norns;
    case 4: return score.population_time_accumulator;
    default: return 0;
    }
}


bool C1WindowsDocument::is_edit_object( const creatures1::objects::Object& object) const {
    return edit_object() == &object;
}

void C1WindowsDocument::remove_from_event_bar( creatures1::objects::Object& object, bool remove_all_entries) {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        frame->remove_event_bar_object(*this, object, remove_all_entries);
    }
}

void C1WindowsDocument::add_to_event_bar(
    creatures1::objects::Object* object) {
    C1MainFrame* frame = active_main_frame();
    if (frame != nullptr) {
        frame->add_event_bar_object(*this, object);
    }
}

void C1WindowsDocument::purge_destroy_when_finished_macros( creatures1::objects::Object& object) {
    creatures1::scripting::purge_destroy_when_finished_macros_for_owner(
        &object);
}

namespace {

// Opt-in placement probe (C1_TRACE_CREATURE).  The hatch script places a
// newborn with `mvto <egg centre X> <egg posb>` then `slim`, and nothing
// afterwards snaps the foot down: UpdateAnchorAndBounds only pulls a creature
// DOWN through a floor, never up onto one.  So the Y handed to mvto has to
// equal the room floor exactly.  Log every move-to with the object's own
// movement bounds so the requested Y and the floor can be compared directly.
void log_placement(const char* kind, const creatures1::objects::Object& object,
                   int world_x, int world_y) {
    if (creature_trace_disabled()) {
        return;
    }
    static std::size_t rows = 0;
    if (rows >= 400) {
        return;
    }
    ++rows;
    const std::uint32_t classifier = object.classifier_base();
    const auto bounds = object.movement_bounds();
    if (FILE* log = std::fopen("Creatures.place.log", "a")) {
        std::fprintf(log,
                     "moveto kind=%s family=%u genus=%u species=%u "
                     "requested=%d,%d bounds=%d,%d,%d,%d floor=%d "
                     "delta_to_floor=%d\n",
                     kind, (classifier >> 24) & 0xffu,
                     (classifier >> 16) & 0xffu, (classifier >> 8) & 0xffu,
                     world_x, world_y, bounds.min_x, bounds.min_y,
                     bounds.max_x, bounds.max_y, bounds.max_y,
                     world_y - bounds.max_y);
        std::fclose(log);
    }
}

}  // namespace

void C1WindowsDocument::move_to_and_redraw(creatures1::objects::Object& object, int world_x, int world_y) {
    // This stands in for vtable slot 23, MoveToAndRedraw, so it has to cover
    // every class that overrides it: SimpleObject @00426f90, CompoundObject's
    // own, Scenery's, and -- for a creature -- Skeleton's
    // SetDownFootPositionAndInvalidateBounds @0043c2f0, which places a
    // creature by the foot it stands on.  The creature import path also
    // reaches this adapter with a skeleton, so keep that native foot-based
    // operation distinct from ordinary object placement.
    if (auto* simple = dynamic_cast<creatures1::objects::SimpleObject*>(
            &object);
        simple != nullptr) {
        log_placement("simple", object, world_x, world_y);
        simple->move_to_and_redraw(world_x, world_y, *this);
        return;
    }
    if (auto* compound = dynamic_cast<creatures1::objects::CompoundObject*>(
            &object);
        compound != nullptr) {
        compound->move_to_and_redraw(world_x, world_y, *this);
        return;
    }
    if (auto* scenery = dynamic_cast<creatures1::objects::Scenery*>(
            &object);
        scenery != nullptr) {
        scenery->move_to_and_redraw(world_x, world_y, *this);
        return;
    }
    if (auto* skeleton = dynamic_cast<creatures1::creatures::Skeleton*>(
            &object);
        skeleton != nullptr) {
        // A creature's mvto sets the down foot, so world_y IS the foot Y.
        log_placement("creature-foot", object, world_x, world_y);
        SkeletonMoveRedraw move_host(*this);
        skeleton->set_down_foot_position_and_invalidate_bounds(
            world_x, world_y, move_host, *this);
        return;
    }
    // Object's slot-23 body is the same no-op movement stub as its other
    // base movement slots.  Preserve that behavior for an unrecognised plain
    // Object instead of converting a valid CAOS target into a host exception.
}

void C1WindowsDocument::move_by_and_redraw(
    creatures1::objects::Object& object, int delta_x, int delta_y) {
    // Slot 22, MoveByAndRedraw.  Scenery and plain Object retain the native
    // Object::MoveBy body at this slot (a no-op), even though Scenery does
    // override slot 20 for direct movement.  Keep that distinction here;
    // routing scenery through its slot-20 move would move it when native
    // `mvby` deliberately does nothing.
    if (auto* simple = dynamic_cast<creatures1::objects::SimpleObject*>(
            &object);
        simple != nullptr) {
        simple->move_by_and_redraw(delta_x, delta_y, *this);
        return;
    }
    if (auto* compound = dynamic_cast<creatures1::objects::CompoundObject*>(
            &object);
        compound != nullptr) {
        compound->move_by_and_redraw(delta_x, delta_y, *this);
        return;
    }
    if (dynamic_cast<creatures1::objects::Scenery*>(&object) != nullptr) {
        return;
    }
    if (auto* skeleton = dynamic_cast<creatures1::creatures::Skeleton*>(
            &object);
        skeleton != nullptr) {
        SkeletonMoveRedraw move_host(*this);
        skeleton->update_and_invalidate_bounds(delta_x, delta_y, move_host);
        return;
    }
    // Object's own slot-22 body is also a no-op.  Unknown registry entries
    // therefore preserve the native result instead of turning a valid CAOS
    // `mvby` into a host exception.
}

bool C1WindowsDocument::is_selected_creature( const creatures1::objects::Object& object) const {
    const auto* creature = creature_for_object(object);
    return creature != nullptr && creature == selected_creature();
}

void C1WindowsDocument::clear_selected_creature(bool notify) {
    if (!notify || selected_creature() == nullptr) {
        set_selected_creature(nullptr);
        return;
    }
    creatures1::application::apply_selected_creature(*this, nullptr, true);
}

void C1WindowsDocument::report_creature_base_function_misuse() {
    ::OutputDebugStringA(
        "C1 object initialized with a creature base function classifier\n");
}

bool C1WindowsDocument::selected_creature_exists() const {
    return selected_creature() != nullptr;
}

void C1WindowsDocument::clear_references_from_other_object(
    creatures1::objects::Object& object,
    creatures1::objects::Object& deleted_object) {
    object.clear_references_to(&deleted_object);
}

void C1WindowsDocument::remove_from_selection(
    creatures1::creatures::Creature& creature) {
    selection_.remove(&creature);
}

void C1WindowsDocument::remove_from_creature_selection(
    creatures1::objects::Object& object) {
    if (creatures1::creatures::Creature* creature =
            mutable_creature_for_object(object)) {
        remove_from_selection(*creature);
    }
}

bool C1WindowsDocument::remove_from_creature_registry(
    creatures1::objects::Object& object) {
    for (std::size_t index = 0; index < creature_count(); ++index) {
        auto* creature =
            dynamic_cast<creatures1::creatures::Creature*>(creature_at(index));
        if (creature != nullptr && &creature->skeleton() == &object) {
            return remove_at(index);
        }
    }
    return false;
}

void C1WindowsDocument::delete_object(creatures1::objects::Object& object) {
    // The deleting virtual: the world runtime owns the Creature that holds
    // this Skeleton, and destroying it releases both.
    if (world_runtime_ != nullptr) {
        world_runtime_->destroy_world_object(object);
    }
}

void C1WindowsDocument::delete_creature(
    creatures1::creatures::Creature& creature) {
    creatures1::objects::delete_object_and_purge_runtime_references(
        creature.skeleton(), *this, event_scheduler_, object_registry());
}

void C1WindowsDocument::add_to_world_object_registry( creatures1::objects::Object& object) {
    if (world_runtime_ == nullptr) {
        throw std::logic_error(
            "C1 runtime initialization has no active WorldRuntime");
    }
    world_runtime_->add_world_object(object);
}

void C1WindowsDocument::move_scenery_to_and_redraw( creatures1::objects::Scenery& /*scenery*/, creatures1::objects::Entity& entity, int world_x, int world_y) {
    int wrapped_x = world_x;
    if (wrapped_x < 0) {
        wrapped_x += creatures1::world::kWorldWidth;
    } else if (wrapped_x >= creatures1::world::kWorldWidth) {
        wrapped_x -= creatures1::world::kWorldWidth;
    }
    entity.set_world_x(wrapped_x);
    entity.set_world_y(world_y);
    invalidate_renderer_view();
}

void C1WindowsDocument::find_nearest_room_bounds_at_point( int world_x, int world_y, creatures1::world::WorldRect& out_bounds) const {
    creatures1::world::MapRectangle map_bounds{};
    if (world_runtime_ != nullptr) {
        creatures1::world::find_nearest_room_bounds_at_point(
            world_runtime_->map_data().room_table(), world_x, world_y,
            map_bounds);
    }
    out_bounds = {map_bounds.left, map_bounds.top, map_bounds.right,
                  map_bounds.bottom};
}

creatures1::world::WorldRect C1WindowsDocument::vehicle_local_bounds( const creatures1::objects::Object& object) const {
    const auto* vehicle = dynamic_cast<const creatures1::objects::Vehicle*>(&object);
    return vehicle == nullptr ? creatures1::world::WorldRect{}
                              : vehicle->creature_event_bounds_local;
}

int C1WindowsDocument::vehicle_primary_entity_x( const creatures1::objects::Object& object) const {
    const auto* vehicle = dynamic_cast<const creatures1::objects::Vehicle*>(&object);
    return vehicle == nullptr || vehicle->part_count() == 0 || vehicle->part(0).entity == nullptr
               ? 0 : vehicle->part(0).entity->world_x();
}

int C1WindowsDocument::vehicle_primary_entity_y( const creatures1::objects::Object& object) const {
    const auto* vehicle = dynamic_cast<const creatures1::objects::Vehicle*>(&object);
    return vehicle == nullptr || vehicle->part_count() == 0 || vehicle->part(0).entity == nullptr
               ? 0 : vehicle->part(0).entity->world_y();
}

creatures1::objects::Object* C1WindowsDocument::edit_object() const {
    return edit_object_;
}

void C1WindowsDocument::draw_view(CWnd& view, CDC& device_context) {
    ensure_renderer(view);
    resize_view(view);
    if (renderer_ != nullptr) {
        renderer_->redraw_full_view(&device_context);
    } else {
        CRect client_rect;
        view.GetClientRect(&client_rect);
        device_context.FillSolidRect(&client_rect, RGB(0, 0, 0));
    }
}

void C1WindowsDocument::bind_renderer_view(CWnd& view) { renderer_view_ = &view; }


void C1WindowsDocument::bind_world_view(C1WindowsView* view) { world_view_ = view; }


void C1WindowsDocument::create_world_renderer_for_view(bool smooth_scrolling_enabled) {
    if (renderer_view_ != nullptr) {
        ensure_renderer(*renderer_view_, smooth_scrolling_enabled);
    }
}

void C1WindowsDocument::destroy_world_renderer() {
    renderer_.reset();
    gdi_host_.reset();
    renderer_view_ = nullptr;
}

void C1WindowsDocument::reset_renderer_navigation() {
    if (renderer_ != nullptr) {
        renderer_->reset_navigation();
    }
}

void C1WindowsDocument::scroll_renderer_viewport(int& delta_x, int& delta_y) {
    if (renderer_ != nullptr) {
        renderer_->scroll_viewport(delta_x, delta_y);
    }
}

int C1WindowsDocument::renderer_viewport_left() const {
    return renderer_ == nullptr ? 0 : renderer_->viewport_left();
}

int C1WindowsDocument::renderer_viewport_top() const {
    return renderer_ == nullptr ? 0 : renderer_->viewport_top();
}

int C1WindowsDocument::renderer_viewport_width() const {
    return renderer_ == nullptr
               ? 0
               : renderer_->viewport_right() - renderer_->viewport_left();
}

int C1WindowsDocument::renderer_viewport_height() const {
    return renderer_ == nullptr
               ? 0
               : renderer_->viewport_bottom() - renderer_->viewport_top();
}

void C1WindowsDocument::request_renderer_origin(int world_x, int world_y) {
    if (renderer_ != nullptr) {
        renderer_->request_viewport_origin(world_x, world_y);
        return;
    }
    // Native constructs CWorldRenderer in the SFCView constructor (0x00436440),
    // so SFCDoc::Serialize's RequestViewportOrigin always reaches a live
    // renderer and the world opens at its saved camera position.  This port
    // creates the renderer later (initial update / first draw); hold the
    // request and apply it when the renderer is created instead of dropping it
    // and opening every world at 0,0.
    pending_renderer_origin_ = std::make_pair(world_x, world_y);
}

void C1WindowsDocument::set_renderer_debug_highlight_rect(int left, int top,
                                                          int right,
                                                          int bottom) {
    if (renderer_ != nullptr) {
        renderer_->set_debug_highlight_rect(left, top, right, bottom);
    }
}

void C1WindowsDocument::queue_renderer_dirty_world_rect(
    const creatures1::world::WorldRect& rect) {
    if (renderer_ != nullptr) {
        renderer_->queue_dirty_world_rect(rect.min_x, rect.min_y, rect.max_x,
                                          rect.max_y);
    }
}

void C1WindowsDocument::set_renderer_smooth_scrolling(bool enabled) {
    if (renderer_ != nullptr) {
        renderer_->set_smooth_scrolling_enabled(enabled);
    }
}

void C1WindowsDocument::present_renderer_view(void* device_context) {
    if (renderer_ != nullptr && device_context != nullptr) {
        renderer_->redraw_full_view(device_context);
    }
}

void C1WindowsDocument::fill_view_background_black(void* device_context) const {
    if (renderer_view_ == nullptr || device_context == nullptr) {
        return;
    }
    CDC* dc = CDC::FromHandle(static_cast<HDC>(device_context));
    if (dc == nullptr) {
        return;
    }
    CRect client_rect;
    renderer_view_->GetClientRect(&client_rect);
    dc->FillSolidRect(&client_rect, RGB(0, 0, 0));
}

bool C1WindowsDocument::is_live_object(
    const creatures1::objects::Object* object) const {
    return world_runtime_ != nullptr && world_runtime_->is_live_object(object);
}

std::size_t C1WindowsDocument::non_scenery_object_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->object_count();
}

creatures1::objects::Object* C1WindowsDocument::non_scenery_object_at( std::size_t index) const {
    return world_runtime_ == nullptr ? nullptr
                                      : world_runtime_->object_at(index);
}

creatures1::objects::Object* C1WindowsDocument::pointer_tool() const {
    return pointer_tool_;
}

bool C1WindowsDocument::has_favourite_place(std::size_t index) const {
    return semantic_document_ != nullptr &&
           index < semantic_document_->favourite_place_count;
}

std::size_t C1WindowsDocument::favourite_place_count() const {
    return semantic_document_ == nullptr
               ? 0
               : semantic_document_->favourite_place_count;
}

std::string C1WindowsDocument::favourite_place_name(std::size_t index) const {
    return has_favourite_place(index)
               ? semantic_document_->favourite_places[index].name
               : std::string();
}

namespace {

// DocumentFavouritePlaceHost for the Add command.  The name comes from the
// recovered dialog 142; the viewport origin and the menu append are the
// document's and the frame's existing operations.
class AddFavouritePlaceHost final
    : public creatures1::application::DocumentFavouritePlaceHost {
public:
    explicit AddFavouritePlaceHost(C1WindowsDocument& document)
        : document_(document) {}

    bool prompt_for_favourite_place_name(std::string& out_name) override {
        C1AddFavouritePlaceDialog dialog;
        if (dialog.DoModal() != IDOK || dialog.place_name().IsEmpty()) {
            return false;
        }
        out_name = dialog.place_name().GetString();
        return true;
    }

    std::int16_t current_viewport_origin_x() const override {
        return static_cast<std::int16_t>(document_.renderer_viewport_left());
    }

    std::int16_t current_viewport_origin_y() const override {
        return static_cast<std::int16_t>(document_.renderer_viewport_top());
    }

    bool append_favourite_place_menu(std::uint32_t command_id,
                                     const std::string& name) override {
        C1MainFrame* frame = active_main_frame();
        if (frame == nullptr) {
            return false;
        }
        // Both operations are MainFrameActivationPlatform members; reach them
        // through that interface rather than widening the frame's access.
        auto& activation =
            static_cast<creatures1::application::MainFrameActivationPlatform&>(
                *frame);
        creatures1::ui::MenuHandle* menu = activation.favourite_places_menu();
        if (menu == nullptr) {
            return false;
        }
        activation.append_favourite_place_menu_item(*menu, command_id, name);
        return true;
    }

private:
    C1WindowsDocument& document_;
};

} // namespace

void C1WindowsDocument::add_favourite_place_from_viewport() {
    if (semantic_document_ == nullptr) {
        return;
    }
    AddFavouritePlaceHost host(*this);
    semantic_document_->add_favourite_place_from_current_viewport(host);
}

void C1WindowsDocument::remove_favourite_place_at(std::size_t index) {
    // The recovered removal compacts the bounded array so the menu command
    // ids stay contiguous from the base.
    if (semantic_document_ == nullptr ||
        index >= semantic_document_->favourite_place_count) {
        return;
    }
    auto& places = semantic_document_->favourite_places;
    for (std::size_t slot = index;
         slot + 1 < semantic_document_->favourite_place_count; ++slot) {
        places[slot] = places[slot + 1];
    }
    places[semantic_document_->favourite_place_count - 1] = {};
    --semantic_document_->favourite_place_count;
}

void C1WindowsDocument::request_favourite_place(std::size_t index) {
    if (!has_favourite_place(index)) {
        return;
    }
    const creatures1::application::FavouritePlace& place =
        semantic_document_->favourite_places[index];
    request_renderer_origin(place.viewport_origin_x,
                            place.viewport_origin_y);
}

bool C1WindowsDocument::read_view_setting(std::string_view name, std::uint32_t& value, std::uint32_t default_value) const {
    HKEY key = nullptr;
    if (!open_c1_secondary_registry(key, KEY_READ)) {
        value = default_value;
        return false;
    }
    const std::string value_name(name);
    const bool present = read_registry_dword(
        key, value_name.c_str(), value);
    RegCloseKey(key);
    if (!present) {
        value = default_value;
    }
    return present;
}

void C1WindowsDocument::write_view_setting(std::string_view name, std::uint32_t value) {
    HKEY key = nullptr;
    if (open_c1_secondary_registry(key, KEY_SET_VALUE)) {
        const std::string value_name(name);
        write_registry_dword(key, value_name.c_str(), value);
        RegCloseKey(key);
    }
}

void C1WindowsDocument::resize_view(CWnd& view) {
    ensure_renderer(view);
    if (renderer_ == nullptr || gdi_host_ == nullptr) {
        return;
    }
    CRect client_rect;
    view.GetClientRect(&client_rect);
    renderer_->resize_back_buffer_for_viewport(
        view.GetSafeHwnd(), client_rect.Width(), client_rect.Height());
}

void C1WindowsDocument::resize_renderer_for_view(CWnd& view, int client_width, int client_height) {
    ensure_renderer(view);
    if (renderer_ != nullptr) {
        renderer_->resize_back_buffer_for_viewport(
            view.GetSafeHwnd(), client_width, client_height);
    }
}

void C1WindowsDocument::invalidate_renderer_view() {
    if (renderer_view_ != nullptr && renderer_view_->GetSafeHwnd() != nullptr) {
        renderer_view_->Invalidate(FALSE);
    }
}

bool C1WindowsDocument::world_renderer_exists() const { return renderer_ != nullptr; }


std::uint32_t C1WindowsDocument::query_new_palette() {
    return renderer_ == nullptr ? 1 : renderer_->realize_palette();
}

void C1WindowsDocument::realize_world_renderer_palette() {
    if (renderer_ != nullptr) {
        static_cast<void>(renderer_->realize_palette());
    }
}

bool C1WindowsDocument::palette_focus_is_view(const CWnd* palette_focus_window) const {
    return palette_focus_window != nullptr &&
           renderer_view_ != nullptr &&
           palette_focus_window->GetSafeHwnd() ==
               renderer_view_->GetSafeHwnd();
}

void* C1WindowsDocument::game_palette() const {
    return creatures1::platform::WindowsPalettePlatform::native_handle(
        game_palette_.native_palette.get());
}

std::uint32_t C1WindowsDocument::realize_palette(void* owner_window, void* palette) {
    return gdi_host_ == nullptr
               ? 0
               : gdi_host_->realize_palette(owner_window, palette);
}

void C1WindowsDocument::invalidate_window(void* owner_window) {
    if (gdi_host_ != nullptr) {
        gdi_host_->invalidate_window(owner_window);
    }
}

bool C1WindowsDocument::owner_is_minimized(void* owner_window) const {
    return gdi_host_ != nullptr &&
           gdi_host_->owner_is_minimized(owner_window);
}

creatures1::display::Gallery* C1WindowsDocument::background_gallery() const {
    return world_runtime_ == nullptr
               ? nullptr
               : world_runtime_->map_data().background_gallery();
}

std::size_t C1WindowsDocument::entity_count() const {
    return world_runtime_ == nullptr ? 0 : world_runtime_->entity_count();
}

const creatures1::objects::Entity* C1WindowsDocument::entity_at( std::size_t index) const {
    return world_runtime_ == nullptr ? nullptr
                                      : world_runtime_->entity_at(index);
}

void C1WindowsDocument::report_invalid_render_registry_index() const {
    // The render policy plate @ 00412aa0 names invalid-index/MFC exception
    // behaviour as a host contract; same convention as the other registries.
    throw std::out_of_range("C1 render entity registry index");
}


void C1WindowsDocument::blit_image_to_dib( creatures1::display::Image& image, std::uint8_t* dib_pixels, int world_x, int world_y, const creatures1::world::WorldRect& clip_rect, const creatures1::world::WorldRect& view_rect, bool direct_copy) {
    if (resources_ == nullptr || dib_pixels == nullptr) {
        return;
    }
    image.blit_to_dib(
        dib_pixels, world_x, world_y, clip_rect, view_rect, direct_copy,
        pixel_cache_, sprite_files_,
        sprite_file_search_paths(),
        *resources_);
}

creatures1::display::Gallery* C1WindowsDocument::acquire_overlay_gallery( std::uint32_t gallery_identifier) {
    return acquire_gallery(gallery_identifier, 0, 1, false);
}

void* C1WindowsDocument::create_memory_dc() {
    return gdi_host_ == nullptr ? nullptr : gdi_host_->create_memory_dc();
}

void* C1WindowsDocument::create_indexed_dib(void* memory_dc, int width, int height, std::uint8_t*& pixels) {
    return gdi_host_ == nullptr
               ? nullptr
               : gdi_host_->create_indexed_dib(memory_dc, width, height,
                                               pixels);
}

void* C1WindowsDocument::select_bitmap(void* memory_dc, void* bitmap) {
    return gdi_host_ == nullptr
               ? nullptr
               : gdi_host_->select_bitmap(memory_dc, bitmap);
}

void C1WindowsDocument::delete_object(void* object) {
    if (gdi_host_ != nullptr) {
        gdi_host_->delete_object(object);
    }
}

void C1WindowsDocument::delete_dc(void* memory_dc) {
    if (gdi_host_ != nullptr) {
        gdi_host_->delete_dc(memory_dc);
    }
}

void C1WindowsDocument::bit_blt(void* target_context, int destination_x, int destination_y, int width, int height, void* source_context, int source_x, int source_y, std::uint32_t raster_operation) {
    if (gdi_host_ != nullptr) {
        gdi_host_->bit_blt(target_context, destination_x, destination_y,
                           width, height, source_context, source_x,
                           source_y, raster_operation);
    }
}

void C1WindowsDocument::draw_dirty_world_outline( void* target_context, const creatures1::world::WorldRect& dirty_world_rect, const creatures1::world::WorldRect& viewport_rect) {
    if (gdi_host_ != nullptr) {
        gdi_host_->draw_dirty_world_outline(target_context,
                                            dirty_world_rect,
                                            viewport_rect);
    }
}

std::array<creatures1::display::RendererPaletteEntry, 0x100> C1WindowsDocument::read_palette_entries(void* palette) const {
    return gdi_host_ == nullptr
               ? std::array<creatures1::display::RendererPaletteEntry,
                            0x100>{}
               : gdi_host_->read_palette_entries(palette);
}

void C1WindowsDocument::set_dib_colour_table( void* memory_dc, const std::array<creatures1::display::RendererDibColour, 0x100>& colours) {
    if (gdi_host_ != nullptr) {
        gdi_host_->set_dib_colour_table(memory_dc, colours);
    }
}

void C1WindowsDocument::fill_client_background_black(void* device_context) {
    if (gdi_host_ != nullptr) {
        gdi_host_->fill_client_background_black(device_context);
    }
}

void C1WindowsDocument::present_dirty_world_rect(
    void* owner_window, const creatures1::world::WorldRect& world_rect,
    const creatures1::world::WorldRect& /*viewport_rect*/) {
    // A document can own both the main world renderer and the creature eye
    // renderer.  Dirty rectangles are emitted by WorldRenderer, so retain
    // that owner identity all the way to presentation; otherwise an eye-view
    // follow tick repaints the main view with the eye renderer's coordinates.
    if (eye_view_ != nullptr &&
        owner_window == static_cast<void*>(eye_view_->GetSafeHwnd())) {
        eye_view_->present_world_rect(world_rect);
        return;
    }
    present_renderer_rect(world_rect);
}

void C1WindowsDocument::present_current_view(
    void* owner_window, const creatures1::world::WorldRect& viewport_rect) {
    // The eye view owns a second, separate WorldRenderer (windows_views_
    // host.cpp), but it too is constructed with this document as its
    // WorldRendererHost -- the same interface the main view's renderer
    // uses. Without checking owner_window, every present_current_view
    // callback (from either renderer) always drew into the MAIN view's
    // DC via present_renderer_rect()'s hardcoded renderer_/gdi_host_,
    // regardless of which window's WorldRenderer actually triggered it.
    // The eye view's own window therefore never received a single real
    // paint -- it stayed black -- while the main view silently absorbed
    // extra redraws it didn't ask for.
    if (eye_view_ != nullptr &&
        owner_window == static_cast<void*>(eye_view_->GetSafeHwnd())) {
        eye_view_->present_world_rect(viewport_rect);
        return;
    }
    present_renderer_rect(viewport_rect);
}

void C1WindowsDocument::move_renderable_objects(int delta_x, int delta_y) {
    // ScrollViewport @ 00412f30 shifts every renderable object by the same
    // effective delta after the viewport origin moves.
    if (world_runtime_ == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < world_runtime_->renderable_count();
         ++index) {
        creatures1::objects::Object* object =
            world_runtime_->renderable_at(index);
        if (object != nullptr) {
            object->move_by(delta_x, delta_y);
        }
    }
}


namespace {

// UpdateViewAnchoredObjects @ 00413950 works through the object's own
// bounds mode, classifier and slot-21 move operation. Creature's Skeleton
// additionally needs the document's layout hosts to reposition its feet.
class ObjectViewAnchor final : public creatures1::objects::ViewAnchorObject {
public:
    ObjectViewAnchor(creatures1::objects::Object& object,
                     C1WindowsDocument& document)
        : object_(object), document_(document) {}

    bool is_view_unbounded() const override {
        return object_.uses_unbounded_world_position();
    }
    bool is_creature() const override {
        return ((object_.classifier_base() >> 24) & 0xff) == 4;
    }
    int current_visual_height() const override {
        return object_.current_visual_height();
    }
    void move_to(int world_x, int world_y) override {
        if (auto* compound =
                dynamic_cast<creatures1::objects::CompoundObject*>(&object_)) {
            compound->move_to(world_x, world_y);
            return;
        }
        if (auto* simple =
                dynamic_cast<creatures1::objects::SimpleObject*>(&object_)) {
            simple->move_to(world_x, world_y);
            return;
        }
        if (auto* scenery =
                dynamic_cast<creatures1::objects::Scenery*>(&object_)) {
            scenery->move_to(world_x, world_y);
            return;
        }
        if (auto* skeleton =
                dynamic_cast<creatures1::creatures::Skeleton*>(&object_)) {
            // Native slot 21 @ 0043c070, including held/edit creatures.
            skeleton->set_down_foot_position_and_recompute_layout(
                world_x, world_y, document_, document_);
        }
    }

private:
    creatures1::objects::Object& object_;
    C1WindowsDocument& document_;
};

} // namespace

void C1WindowsDocument::update_view_anchored_objects() {
    if (world_runtime_ == nullptr) {
        return;
    }

    // ViewAnchorInput's first two fields are the CLIENT position;
    // update_view_anchored_objects adds the viewport itself.  Passing the
    // world position here counted the viewport twice.
    const creatures1::objects::ViewAnchorInput view{
        mouse_client_x(), mouse_client_y(), renderer_viewport_left(),
        renderer_viewport_top()};

    std::vector<ObjectViewAnchor> anchors;
    std::vector<creatures1::objects::ViewAnchorObject*> anchor_pointers;
    anchors.reserve(world_runtime_->renderable_count());
    anchor_pointers.reserve(world_runtime_->renderable_count());
    for (std::size_t index = 0; index < world_runtime_->renderable_count();
         ++index) {
        creatures1::objects::Object* object =
            world_runtime_->renderable_at(index);
        if (object != nullptr) {
            anchors.emplace_back(*object, *this);
        }
    }
    for (ObjectViewAnchor& anchor : anchors) {
        anchor_pointers.push_back(&anchor);
    }

    creatures1::objects::Object* pointer = pointer_tool();
    creatures1::objects::Object* edit = edit_object();
    std::optional<ObjectViewAnchor> pointer_anchor;
    std::optional<ObjectViewAnchor> edit_anchor;
    if (pointer != nullptr) {
        pointer_anchor.emplace(*pointer, *this);
    }
    if (edit != nullptr) {
        edit_anchor.emplace(*edit, *this);
    }
    creatures1::objects::update_view_anchored_objects(
        &view, {anchor_pointers.data(), anchor_pointers.size()},
        pointer_anchor ? &*pointer_anchor : nullptr,
        edit_anchor ? &*edit_anchor : nullptr);
}


bool C1WindowsDocument::selected_creature_is_edit_object() const {
    const creatures1::creatures::Creature* creature = selected_creature();
    if (creature == nullptr || edit_object_ == nullptr) {
        return false;
    }

    // The application stores a Creature separately from the Object registry;
    // its Skeleton is the Object identity used by the edit-object state.
    // Native follow-selection skips viewport tracking while that same
    // Skeleton is being carried, so compare through the existing ownership
    // adapter rather than comparing unrelated Creature/Object addresses.
    return edit_object_ == &creature->skeleton();
}


bool C1WindowsDocument::selected_creature_is_bounded() const {
    const creatures1::creatures::Creature* creature = selected_creature();
    return creature != nullptr &&
           creature->skeleton().bounds_mode() !=
               creatures1::objects::Object::BoundsMode::unbounded_1;
}


bool C1WindowsDocument::selected_creature_down_foot(int& world_x, int& world_y) const {
    const creatures1::creatures::Creature* creature = selected_creature();
    if (creature == nullptr) {
        return false;
    }
    world_x = creature->skeleton().down_foot_x;
    world_y = creature->skeleton().down_foot_y;
    return true;
}

creatures1::world::WorldRect C1WindowsDocument::navigation_bounds() const {
    return {0, 0, creatures1::world::kWorldWidth,
            creatures1::world::kWorldHeight};
}

bool C1WindowsDocument::contains_point(const creatures1::world::WorldRect& bounds, int world_x, int world_y) const {
    if (world_y < bounds.min_y || world_y >= bounds.max_y) {
        return false;
    }
    int left = bounds.min_x;
    int right = bounds.max_x;
    if (right - left >= creatures1::world::kWorldWidth) {
        return true;
    }
    if (right > creatures1::world::kWorldWidth) {
        return world_x >= left ||
               world_x < right - creatures1::world::kWorldWidth;
    }
    return world_x >= left && world_x < right;
}

void C1WindowsDocument::ensure_resource_hosts() {
    if (resources_ != nullptr) {
        return;
    }

    const std::string fallback = current_directory_with_separator();
    for (std::size_t index = 0; index < resource_paths_.size(); ++index) {
        if (g_active_primary_directories != nullptr &&
            !g_active_primary_directories->paths[index].empty()) {
            resource_paths_[index] =
                g_active_primary_directories->paths[index];
        } else {
            resource_paths_[index] = fallback;
        }
    }

    creatures1::application::C1ResourceDirectories directories;
    directories.primary_resource_directories = resource_paths_;
    for (std::size_t index = 0; index < resource_paths_.size(); ++index) {
        directories.secondary_resource_directories[index] =
            secondary_resource_directory(index);
    }
    directories.body_data_directory =
        resource_paths_[kBodyDataDirectoryIndex];
    creature_resources_ = std::make_unique<
        creatures1::platform::C1CreatureResourceHost>(
            files_,
            secondary_resource_directory(kGeneticsDirectoryIndex),
            resource_paths_[kGeneticsDirectoryIndex],
            resource_paths_[kMainDirectoryIndex]);
    resources_ = std::make_unique<
        creatures1::application::C1ResourceHost>(
        std::move(directories), files_);
    palette_files_ = std::make_unique<
        creatures1::application::C1PaletteDtaHost>(files_);
    palette_platform_ = std::make_unique<
        creatures1::platform::WindowsPalettePlatform>(nullptr);
}

void C1WindowsDocument::ensure_renderer(CWnd& view, bool smooth_scrolling_enabled) {
    if (renderer_ != nullptr) {
        return;
    }
    CRect client_rect;
    view.GetClientRect(&client_rect);
    gdi_host_ = std::make_unique<
        creatures1::platform::WindowsWorldRendererGdiHost>(
            view.GetSafeHwnd());
    renderer_view_ = &view;
    renderer_ = std::make_unique<creatures1::display::WorldRenderer>(
        *this, view.GetSafeHwnd(), view.GetScrollPos(SB_HORZ),
        view.GetScrollPos(SB_VERT), (std::max)(1, client_rect.Width()),
        (std::max)(1, client_rect.Height()), nullptr,
        smooth_scrolling_enabled, 0);
    renderer_->resize_back_buffer_for_viewport(
        view.GetSafeHwnd(), (std::max)(1, client_rect.Width()),
        (std::max)(1, client_rect.Height()));
    renderer_->realize_palette();
    if (pending_renderer_origin_.has_value()) {
        const auto [origin_x, origin_y] = *pending_renderer_origin_;
        pending_renderer_origin_.reset();
        renderer_->request_viewport_origin(origin_x, origin_y);
    }
}

void C1WindowsDocument::present_renderer_rect(const creatures1::world::WorldRect& rect) {
    if (renderer_ == nullptr || gdi_host_ == nullptr) {
        return;
    }
    void* device_context = gdi_host_->acquire_client_context();
    if (device_context != nullptr) {
        renderer_->present_world_rect(device_context, rect);
        gdi_host_->release_client_context(device_context);
    }
}

C1WindowsDocument::ArchiveHost::ArchiveHost(C1WindowsDocument& document, CArchive& archive) : document_(document), stream_(archive) {
    objects_ = std::make_unique<creatures1::platform::MfcDynamicObjectTable>(
        stream_,
        [this](std::string_view name, std::uint16_t schema) {
            return create_object(name, schema);
        },
        [this](void* object, std::string_view name,
               creatures1::platform::MfcObjectArchive& archive) {
            read_object(object, name, archive);
        },
        [this](const void* object, std::string_view name,
               creatures1::platform::MfcObjectArchive& archive) {
            write_object(object, name, archive);
        },
        [this](const void* object, std::string_view requested) {
            return runtime_class(object, requested);
        },
        [](std::string_view name) { return class_schema(name); },
        [](std::string_view actual, std::string_view requested) {
            return compatible_class(actual, requested);
        });
    document_.active_archive_stream_ = &stream_;
    document_.active_object_table_ = objects_.get();
}

C1WindowsDocument::ArchiveHost::~ArchiveHost() {
    document_.active_object_table_ = nullptr;
    document_.active_archive_stream_ = nullptr;
}

bool C1WindowsDocument::ArchiveHost::loading() const { return stream_.loading(); }


void C1WindowsDocument::ArchiveHost::serialize_map_data() {
    require_runtime();
    // MapData is the document root object and is streamed as a dynamic MFC
    // object, not as inline content: World.sfc opens with the class record
    // ffff 0001 0007 "MapData".  Reading its payload directly consumed that
    // header as map bytes and desynchronised every later object reference.
    if (loading()) {
        objects_->read_object_reference("MapData");
        return;
    }
    objects_->write_object_reference(&document_.world_runtime_->map_data(),
                                     "MapData");
}

void C1WindowsDocument::ArchiveHost::serialize_map_data_payload() {
    creatures1::platform::MfcMapDataArchive archive(
        stream_,
        [this]() {
            return static_cast<creatures1::display::Gallery*>(
                objects_->read_object_reference("CGallery"));
        },
        [this](creatures1::display::Gallery* gallery) {
            objects_->write_object_reference(gallery, "CGallery");
        });
    creatures1::world::serialize_map_data(
        document_.world_runtime_->map_data(), archive);
}

std::size_t C1WindowsDocument::ArchiveHost::non_scenery_object_count() const {
    return document_.world_runtime_ == nullptr
               ? 0
               : document_.world_runtime_->object_count();
}

std::int32_t C1WindowsDocument::ArchiveHost::read_non_scenery_object_count() {
    return stream_.read_int32();
}

void C1WindowsDocument::ArchiveHost::write_non_scenery_object_count(std::int32_t count) {
    stream_.write_int32(count);
}

void C1WindowsDocument::ArchiveHost::serialize_non_scenery_object(std::size_t index) {
    require_runtime();
    if (loading()) {
        // The registry position is NOT the archive slot.  An object first
        // reached as a nested reference inside an earlier record is
        // constructed -- and so registered -- at that point, and its own slot
        // later in the list is only a back-reference; worldpopulated has two.
        // The load simply reads each slot and lets construction populate the
        // registry, exactly as the native does.
        // SFCDoc::Serialize @ 0x004311a0 reads the count and then calls
        // ReadObject(Object) that many times, discarding every result: the
        // objects register themselves as they are constructed.  A slot can
        // legitimately be null, because the save loop writes whatever the
        // registry holds and the registry can carry holes -- rejecting one
        // turns a world the original loads into a startup failure.
        (void)objects_->read_object_reference("Object");
        (void)index;
        return;
    }
    objects_->write_object_reference(
        document_.world_runtime_->object_at(index), "Object");
}

std::size_t C1WindowsDocument::ArchiveHost::scenery_object_count() const {
    return document_.world_runtime_ == nullptr
               ? 0
               : document_.world_runtime_->scenery_count();
}

std::int32_t C1WindowsDocument::ArchiveHost::read_scenery_object_count() {
    return stream_.read_int32();
}

void C1WindowsDocument::ArchiveHost::write_scenery_object_count(std::int32_t count) {
    stream_.write_int32(count);
}

void C1WindowsDocument::ArchiveHost::serialize_scenery_object(std::size_t index) {
    require_runtime();
    if (loading()) {
        // Same rule as the non-scenery list: the registry position is the
        // construction order, not the archive slot.
        // The scenery loop has the same shape in the native: read, discard,
        // and let construction do the registering.  It asserts nothing about
        // what came back.
        (void)objects_->read_object_reference("Object");
        (void)index;
        return;
    }
    objects_->write_object_reference(
        document_.world_runtime_->scenery_at(index), "Object");
}

void C1WindowsDocument::ArchiveHost::serialize_classifier_scripts() {
    if (loading()) {
        creatures1::scripting::deserialize_all_scripts(*object_archive(),
                                                        document_);
    } else {
        creatures1::scripting::serialize_all_scripts(*object_archive());
    }
}

std::int32_t C1WindowsDocument::ArchiveHost::read_viewport_origin_x() {
    return stream_.read_int32();
}

std::int32_t C1WindowsDocument::ArchiveHost::read_viewport_origin_y() {
    return stream_.read_int32();
}

std::int32_t C1WindowsDocument::ArchiveHost::viewport_origin_x() const {
    return document_.renderer_viewport_left();
}

std::int32_t C1WindowsDocument::ArchiveHost::viewport_origin_y() const {
    return document_.renderer_viewport_top();
}

void C1WindowsDocument::ArchiveHost::write_viewport_origin(std::int32_t x, std::int32_t y) {
    stream_.write_int32(x);
    stream_.write_int32(y);
}

void C1WindowsDocument::ArchiveHost::serialize_selected_creature() {
    if (loading()) {
        document_.selected_creature_entry_ = creature_for_archive_object(
            objects_->read_object_reference("Creature"));
        return;
    }
    objects_->write_object_reference(
        archive_object_for_creature(document_.selected_creature()),
        "Creature");
}

void C1WindowsDocument::ArchiveHost::serialize_favourite_place( creatures1::application::FavouritePlace& place) {
    creatures1::platform::MfcFavouritePlaceArchive archive(stream_);
    place.serialize(archive);
}

void C1WindowsDocument::ArchiveHost::serialize_main_toolbar() {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        throw std::logic_error("C1 document archive has no main frame");
    }
    creatures1::platform::MfcToolbarArchive archive(stream_);
    frame->serialize_main_toolbar(archive);
}

std::size_t C1WindowsDocument::ArchiveHost::running_macro_count() const {
    return creatures1::scripting::g_running_macros.size();
}

std::int32_t C1WindowsDocument::ArchiveHost::read_running_macro_count() {
    return stream_.read_int32();
}

void C1WindowsDocument::ArchiveHost::write_running_macro_count(std::int32_t count) {
    stream_.write_int32(count);
}

void C1WindowsDocument::ArchiveHost::serialize_running_macro(std::size_t index) {
    // Each running macro is a dynamic MFC object record (class tag, schema and
    // name), not inline content: reading its fields directly consumed the tag
    // as macro data and desynchronised the rest of the archive.
    if (loading()) {
        objects_->read_object_reference("Macro");
        return;
    }
    if (index >= running_macro_count() ||
        creatures1::scripting::g_running_macros[index] == nullptr) {
        throw std::logic_error("C1 running macro table index mismatch");
    }
    objects_->write_object_reference(
        creatures1::scripting::g_running_macros[index], "Macro");
}

std::size_t C1WindowsDocument::ArchiveHost::world_object_count() const {
    return document_.world_runtime_ == nullptr
               ? 0
               : document_.world_runtime_->world_object_count();
}

std::int32_t C1WindowsDocument::ArchiveHost::read_world_object_count() {
    return stream_.read_int32();
}

void C1WindowsDocument::ArchiveHost::write_world_object_count(std::int32_t count) {
    stream_.write_int32(count);
}

void C1WindowsDocument::ArchiveHost::serialize_world_object(std::size_t index) {
    require_runtime();
    if (loading()) {
        auto* object = static_cast<creatures1::objects::Object*>(
            objects_->read_object_reference("Object"));
        if (object == nullptr) {
            throw std::logic_error("C1 archive has a null world object");
        }
        document_.world_runtime_->add_world_object(*object);
        return;
    }
    objects_->write_object_reference(
        document_.world_runtime_->world_object_at(index), "Object");
}

void C1WindowsDocument::ArchiveHost::disable_loaded_world_object_ticks(std::size_t index) {
    require_runtime();
    if (index >= document_.world_runtime_->world_object_count()) {
        throw std::out_of_range("C1 loaded world-object index");
    }
    document_.world_runtime_->world_object_at(index)->disable_ticking();
}

void C1WindowsDocument::ArchiveHost::serialize_event_bar() {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        throw std::logic_error("C1 document archive has no main frame");
    }
    creatures1::platform::MfcEventBarArchive archive(stream_, *objects_);
    frame->serialize_event_bar(archive, event_bar_words_);
}

void C1WindowsDocument::ArchiveHost::serialize_score(
    creatures1::application::DocumentScore& score) {
    creatures1::ui::Score value;
    value.hatchery_eggs_used = static_cast<int>(score.hatchery_eggs_used);
    value.natural_eggs_laid = static_cast<int>(score.natural_eggs_laid);
    value.dead_norns = static_cast<int>(score.dead_norns);
    value.living_norns = static_cast<int>(score.living_norns);
    value.population_time_accumulator =
        static_cast<int>(score.population_time_accumulator);
    creatures1::platform::MfcScoreArchive archive(stream_);
    value.serialize(archive);
    score.hatchery_eggs_used = static_cast<std::uint32_t>(
        value.hatchery_eggs_used);
    score.natural_eggs_laid = static_cast<std::uint32_t>(
        value.natural_eggs_laid);
    score.dead_norns = static_cast<std::uint32_t>(value.dead_norns);
    score.living_norns = static_cast<std::uint32_t>(value.living_norns);
    score.population_time_accumulator = static_cast<std::uint32_t>(
        value.population_time_accumulator);
}

std::uint32_t C1WindowsDocument::ArchiveHost::world_tick_count() const {
    return document_.world_tick_count_;
}

void C1WindowsDocument::ArchiveHost::write_world_tick_count(std::uint32_t count) {
    stream_.write_uint32(count);
}

std::uint32_t C1WindowsDocument::ArchiveHost::read_world_tick_count() {
    return stream_.read_uint32();
}

void C1WindowsDocument::ArchiveHost::set_world_tick_count(std::uint32_t count) {
    document_.world_tick_count_ = count;
}

std::uint32_t C1WindowsDocument::ArchiveHost::read_document_state_word_count() {
    return stream_.read_uint32();
}

void C1WindowsDocument::ArchiveHost::write_document_state_word_count(std::uint32_t count) {
    stream_.write_uint32(count);
}

std::uint32_t C1WindowsDocument::ArchiveHost::read_document_state_word() {
    return stream_.read_uint32();
}

void C1WindowsDocument::ArchiveHost::write_document_state_word(std::uint32_t word) {
    stream_.write_uint32(word);
}

bool C1WindowsDocument::ArchiveHost::viewport_navigation_requires_manual_reset() const {
    return document_.viewport_navigation_disabled_;
}

void C1WindowsDocument::ArchiveHost::reset_viewport_navigation_after_load() {
    document_.viewport_navigation_disabled_ = false;
    document_.reset_renderer_navigation();
}

void C1WindowsDocument::ArchiveHost::request_viewport_origin(std::int32_t x, std::int32_t y) {
    document_.request_renderer_origin(x, y);
}

void C1WindowsDocument::ArchiveHost::load_default_first_favourite_place_name(
    creatures1::application::FavouritePlace& place) {
    // SFCDoc::Serialize @ 004311a0 pushes string id 0xef31 exactly once in the
    // whole image, at 004318ad, right after the favourite-place loop.  That
    // string is "The Incubator", the fixed name of the first place.
    CStringA value;
    value.LoadStringA(0xef31);
    place.name = value.GetString();
}


void C1WindowsDocument::ArchiveHost::dispatch_loaded_non_scenery_bounds_update(std::size_t index) {
    require_runtime();
    if (index >= document_.world_runtime_->object_count()) {
        throw std::out_of_range("C1 loaded object bounds index");
    }
    document_.world_runtime_->object_at(index)->update_movement_bounds(
        document_);
}

void C1WindowsDocument::ArchiveHost::rebuild_missing_unbounded_renderable_entry(
    std::size_t index) {
    // The renderable object set is not serialized, so SFCDoc::Serialize @
    // 004311a0 rebuilds the entry for any object whose position is relative
    // rather than absolute -- BOTH unbounded modes, not just the first -- and
    // only when the set has no node for it yet.  It restores membership and
    // nothing else: the object keeps its loaded bounds mode, bounds reference,
    // render plane and position.
    require_runtime();
    if (index >= document_.world_runtime_->object_count()) {
        throw std::out_of_range("C1 loaded object renderable index");
    }
    creatures1::objects::Object* object =
        document_.world_runtime_->object_at(index);
    if (object == nullptr ||
        !object->uses_relative_world_position()) {
        return;
    }
    if (document_.world_runtime_->contains(*object)) {
        return;
    }
    document_.world_runtime_->insert(*object);
}


bool C1WindowsDocument::ArchiveHost::has_unbounded_stage_one_object() const {
    // SFCDoc::Serialize @ 004311a0 scans the non-scenery registry once the
    // state words are read and abandons the pointer-tool event when any object
    // is still in bounds mode UNBOUNDED_1.  Such an object is mid-placement and
    // is following the mouse; handing the pointer tool back its
    // viewport-relative position while one is outstanding would strand it.
    if (document_.world_runtime_ == nullptr) {
        return false;
    }
    const std::size_t count = document_.world_runtime_->object_count();
    for (std::size_t index = 0; index < count; ++index) {
        const creatures1::objects::Object* object =
            document_.world_runtime_->object_at(index);
        if (object != nullptr && object->uses_unbounded_world_position()) {
            return true;
        }
    }
    return false;
}


void C1WindowsDocument::ArchiveHost::queue_pointer_tool_event_four() {
    // The recovered load path queues EVENT_4 on the pointer tool once the
    // world has no unbounded stage-one object; PointerTool's event-4 handler
    // @ 00429620 is what makes the tool viewport-relative again.
    creatures1::objects::Object* pointer_tool = document_.pointer_tool();
    if (pointer_tool == nullptr) {
        return;
    }
    document_.event_scheduler_.queue_object_event(
        pointer_tool, pointer_tool, creatures1::objects::ObjectEventId::event_4,
        0, 0, 0, static_cast<std::int32_t>(document_.world_tick_count()));
}


void C1WindowsDocument::ArchiveHost::rebuild_creature_selection_menu() {
    document_.rebuild_creature_selection_menu();
}

creatures1::creatures::Creature*
C1WindowsDocument::ArchiveHost::creature_for_archive_object(void* object) const {
    return object == nullptr
               ? nullptr
               : document_.mutable_creature_for_object(
                     *static_cast<creatures1::objects::Object*>(object));
}

const void* C1WindowsDocument::ArchiveHost::archive_object_for_creature(
    const creatures1::creatures::Creature* creature) const {
    return creature == nullptr ? nullptr : &creature->skeleton();
}

void C1WindowsDocument::ArchiveHost::require_runtime() const {
    if (document_.world_runtime_ == nullptr) {
        throw std::logic_error("C1 document archive has no WorldRuntime");
    }
}

creatures1::platform::MfcObjectArchive* C1WindowsDocument::ArchiveHost::object_archive() {
    object_archive_ = std::make_unique<creatures1::platform::MfcObjectArchive>(
        stream_,
        [this](std::string_view name) {
            return objects_->read_object_reference(name);
        },
        [this](const void* object, std::string_view name) {
            objects_->write_object_reference(object, name);
        });
    return object_archive_.get();
}

void* C1WindowsDocument::ArchiveHost::create_object(std::string_view name, std::uint16_t schema) {
    if (schema != 1) {
        throw std::logic_error("unsupported C1 MFC runtime-class schema");
    }
    require_runtime();
    if (name == "CGallery") {
        auto gallery = std::make_unique<creatures1::display::Gallery>();
        return document_.world_runtime_->add_gallery(std::move(gallery));
    }
    if (name == "Creature") {
        // Creature is not an Object in this port -- it owns a Skeleton, and
        // the Skeleton is what the non-scenery registry holds.  The archive
        // therefore identifies a Creature record by its Skeleton, so a
        // back-reference from any Object slot resolves to a real Object.
        return &document_.world_runtime_
                    ->adopt_creature(
                        std::make_unique<creatures1::creatures::Creature>())
                    .skeleton();
    }
    if (name == "CBrain") {
        auto* brain = new (std::nothrow) creatures1::brain::Brain();
        if (brain == nullptr) {
            throw std::bad_alloc();
        }
        return brain;
    }
    if (name == "CBiochemistry") {
        auto* biochemistry =
            new (std::nothrow) creatures1::biochemistry::Biochemistry();
        if (biochemistry == nullptr) {
            throw std::bad_alloc();
        }
        return biochemistry;
    }
    if (name == "CInstinct") {
        auto instinct = std::make_unique<creatures1::brain::Instinct>();
        auto* raw = instinct.get();
        archive_instincts_.push_back(std::move(instinct));
        return raw;
    }
    if (name == "MapData") {
        // The world runtime already owns the single MapData instance; the
        // archive only needs its identity.
        return &document_.world_runtime_->map_data();
    }
    if (name == "PointerTool") {
        // The document owns exactly one pointer tool for the world's life, so
        // the archive record restores that instance rather than adding a
        // second one to the registry.
        if (document_.pointer_tool_ == nullptr) {
            auto tool = std::make_unique<creatures1::ui::PointerTool>();
            document_.pointer_tool_ = tool.get();
            document_.world_runtime_->adopt_non_scenery_object(
                std::move(tool));
        }
        return document_.pointer_tool_;
    }
    if (name == "Bubble") {
        auto bubble = std::unique_ptr<creatures1::objects::Bubble>(
            new (std::nothrow) creatures1::objects::Bubble());
        auto* raw = bubble.get();
        if (raw != nullptr) {
            document_.world_runtime_->adopt_non_scenery_object(
                std::move(bubble));
        }
        return raw;
    }
    if (name == "COwner") {
        // COwner::CreateObject @ 0042d890 is reached only through MFC dynamic
        // creation from the archive stream, so the object is constructed here
        // rather than by an explicit call anywhere in startup.
        auto owner = std::make_unique<creatures1::creatures::Owner>();
        auto* raw = owner.get();
        archive_owners_.push_back(std::move(owner));
        return raw;
    }
    if (name == "Entity") {
        return new (std::nothrow) creatures1::objects::Entity(
            document_.world_runtime_.get());
    }
    if (name == "Body") {
        // Skeleton::serialize adopts the Body it reads back, so the factory
        // only has to allocate one against the entity registry.
        return new (std::nothrow) creatures1::creatures::Body(
            document_.world_runtime_.get());
    }
    if (name == "Limb") {
        return new (std::nothrow) creatures1::creatures::LimbPart(
            document_.world_runtime_.get());
    }
    if (name == "Macro") {
        // SFCDoc::Serialize streams the running-macro list through
        // CArchive::ReadObject, so each Macro arrives as a dynamic object
        // record and is registered here rather than by the list loop.
        auto* macro = new (std::nothrow) creatures1::scripting::Macro();
        if (macro == nullptr) {
            throw std::bad_alloc();
        }
        creatures1::scripting::g_running_macros.push_back(macro);
        return macro;
    }

    std::unique_ptr<creatures1::objects::Object> object;
    if (name == "Scenery") {
        return &document_.world_runtime_->adopt_scenery_object(
            std::make_unique<creatures1::objects::Scenery>());
    } else if (name == "CallButton") {
        object = std::make_unique<creatures1::objects::CallButton>();
    } else if (name == "Lift") {
        object = std::make_unique<creatures1::objects::Lift>();
    } else if (name == "Vehicle") {
        object = std::make_unique<creatures1::objects::Vehicle>();
    } else if (name == "Blackboard") {
        object = std::make_unique<creatures1::brain::Blackboard>();
    } else if (name == "CompoundObject") {
        object = std::make_unique<creatures1::objects::CompoundObject>();
    } else if (name == "SimpleObject") {
        object = std::make_unique<creatures1::objects::SimpleObject>();
    } else if (name == "Object") {
        object = std::make_unique<creatures1::objects::Object>();
    } else {
        throw std::logic_error("unsupported C1 MFC runtime class: " +
                               std::string(name));
    }
    return &document_.world_runtime_->adopt_non_scenery_object(
        std::move(object));
}

std::uint16_t C1WindowsDocument::ArchiveHost::class_schema(std::string_view name) {
    if (name == "Object" || name == "SimpleObject" ||
        name == "CompoundObject" || name == "Vehicle" ||
        name == "Lift" || name == "CallButton" || name == "Scenery" ||
        name == "Entity" || name == "CGallery" || name == "Creature" ||
        name == "CBrain" || name == "CBiochemistry" ||
        name == "CInstinct" || name == "Blackboard" || name == "COwner" ||
        name == "MapData" || name == "PointerTool" ||
        name == "Bubble" || name == "Macro" || name == "Body" ||
        name == "Limb") {
        return 1;
    }
    throw std::logic_error("unknown C1 MFC runtime class: " +
                           std::string(name));
}

bool C1WindowsDocument::ArchiveHost::compatible_class(std::string_view actual, std::string_view requested) {
    if (requested == actual) {
        return true;
    }
    if (requested == "Object") {
        // Every concrete class the archive can store in a world object slot.
        // PointerTool and Bubble are SimpleObject subclasses and Skeleton is
        // a direct Object subclass; omitting them rejected a real World.sfc
        // at the first PointerTool record.
        return actual == "SimpleObject" || actual == "CompoundObject" ||
               actual == "Vehicle" || actual == "Lift" ||
               actual == "CallButton" || actual == "Scenery" ||
               actual == "Blackboard" || actual == "PointerTool" ||
               actual == "Bubble" || actual == "Skeleton" ||
               actual == "Creature";
    }
    if (requested == "SimpleObject") {
        // CallButton, PointerTool, and Bubble all carry the SimpleObject
        // base record.  MFC's IsKindOf accepts any of those leaf records when
        // a SimpleObject reference is read; rejecting CallButton here loses
        // the lift/button relationship during archive load.
        return actual == "CallButton" || actual == "PointerTool" ||
               actual == "Bubble";
    }
    if (requested == "CompoundObject") {
        // Vehicle and Lift extend CompoundObject, while Blackboard is the
        // other concrete compound leaf.  The requested class is a base-class
        // constraint, not an exact runtime-class selector.
        return actual == "Vehicle" || actual == "Lift" ||
               actual == "Blackboard";
    }
    if (requested == "Vehicle") {
        // Lift is the only concrete class below Vehicle.  Keep this explicit
        // because MFC reference validation is used for more than the current
        // call-button path.
        return actual == "Lift";
    }
    return false;
}

std::string C1WindowsDocument::ArchiveHost::runtime_class(const void* object, std::string_view requested) const {
    if (object == nullptr) {
        return {};
    }
    if (requested == "CGallery") {
        return "CGallery";
    }
    if (requested == "Creature") {
        return "Creature";
    }
    if (requested == "CBrain") {
        return "CBrain";
    }
    if (requested == "CBiochemistry") {
        return "CBiochemistry";
    }
    if (requested == "CInstinct") {
        return "CInstinct";
    }
    if (requested == "COwner") {
        return "COwner";
    }
    if (requested == "MapData") {
        return "MapData";
    }
    if (requested == "Entity") {
        return "Entity";
    }
    if (requested == "Macro") {
        return "Macro";
    }
    if (requested == "Body") {
        return "Body";
    }
    if (requested == "Limb") {
        return "Limb";
    }
    // "Object" and "SimpleObject" are both base-class requests: the record has
    // to be written under the reference's ACTUAL leaf class, which the dynamic
    // resolution below works out.  Creature::Serialize asks for the sleep
    // indicator by "SimpleObject", so leaving it out threw on every creature
    // write -- which meant every autosave threw, and update_world aborted
    // before its closing set_world_tick_count.  The world tick never advanced.
    if (requested != "Object" && requested != "SimpleObject") {
        throw std::logic_error("unsupported C1 MFC reference request: " +
                               std::string(requested));
    }
    const auto* value = static_cast<const creatures1::objects::Object*>(
        object);
    if (document_.creature_for_object(*value) != nullptr) {
        return "Creature";
    }
    if (dynamic_cast<const creatures1::objects::Scenery*>(value) != nullptr)
        return "Scenery";
    if (dynamic_cast<const creatures1::objects::CallButton*>(value) != nullptr)
        return "CallButton";
    if (dynamic_cast<const creatures1::objects::Lift*>(value) != nullptr)
        return "Lift";
    if (dynamic_cast<const creatures1::objects::Vehicle*>(value) != nullptr)
        return "Vehicle";
    // Every leaf class has to be named before the base it derives from, or the
    // record is written under the base name and the class is lost on reload:
    // Blackboard degraded to CompoundObject, and PointerTool and Bubble to
    // SimpleObject, so a saved world came back with no pointer tool, no
    // bubbles and a blackboard stripped of its own state.
    if (dynamic_cast<const creatures1::brain::Blackboard*>(value) != nullptr)
        return "Blackboard";
    if (dynamic_cast<const creatures1::objects::CompoundObject*>(value) != nullptr)
        return "CompoundObject";
    if (dynamic_cast<const creatures1::ui::PointerTool*>(value) != nullptr)
        return "PointerTool";
    if (dynamic_cast<const creatures1::objects::Bubble*>(value) != nullptr)
        return "Bubble";
    if (dynamic_cast<const creatures1::objects::SimpleObject*>(value) != nullptr)
        return "SimpleObject";
    return "Object";
}

void C1WindowsDocument::ArchiveHost::read_object(void* object, std::string_view name, creatures1::platform::MfcObjectArchive& archive) {
    if (name == "Creature") {
        creatures1::creatures::Creature* creature =
            creature_for_archive_object(object);
        if (creature == nullptr) {
            throw std::logic_error(
                "C1 archive Creature record has no owning Creature");
        }
        creatures1::platform::MfcCreatureArchive creature_archive(archive);
        creature->serialize(creature_archive);
        return;
    }
    if (name == "CBrain") {
        creatures1::platform::MfcBrainArchive brain_archive(archive);
        static_cast<creatures1::brain::Brain*>(object)->serialize(
            brain_archive);
        return;
    }
    if (name == "CBiochemistry") {
        auto* value =
            static_cast<creatures1::biochemistry::Biochemistry*>(object);
        creatures1::platform::MfcBiochemistryArchive biochemistry_archive(
            archive,
            [this](void* owner) { return creature_for_archive_object(owner); },
            [this](const creatures1::creatures::Creature* creature) {
                return archive_object_for_creature(creature);
            });
        creatures1::platform::MfcBiochemistryLocusHost locus_host(*value);
        value->serialize(biochemistry_archive, locus_host);
        return;
    }
    if (name == "CInstinct") {
        creatures1::platform::MfcInstinctArchive instinct_archive(archive);
        static_cast<creatures1::brain::Instinct*>(object)->serialize(
            instinct_archive);
        return;
    }
    if (name == "COwner") {
        // COwner::Serialize @ 0042da60 streams exactly six MFC strings.
        creatures1::platform::MfcStringArchive string_archive(archive);
        static_cast<creatures1::creatures::Owner*>(object)->serialize(
            string_archive);
        return;
    }
    if (name == "Macro") {
        creatures1::platform::MfcMacroArchive macro_archive(stream_, *objects_);
        static_cast<creatures1::scripting::Macro*>(object)->serialize(
            macro_archive);
        return;
    }
    if (name == "MapData") {
        static_cast<void>(object);
        serialize_map_data_payload();
        return;
    }
    if (name == "CGallery") {
        creatures1::platform::MfcMapDataArchive gallery_archive(
            stream_,
            [this]() { return static_cast<creatures1::display::Gallery*>(
                objects_->read_object_reference("CGallery")); },
            [this](creatures1::display::Gallery* gallery) {
                objects_->write_object_reference(gallery, "CGallery");
            });
        creatures1::display::serialize_gallery(
            *static_cast<creatures1::display::Gallery*>(object),
            gallery_archive);
        return;
    }
    if (name == "Body" || name == "Limb") {
        // Body and Limb are BodyParts, which are Entities: their records use
        // the same entity archive as a plain Entity record.
        creatures1::platform::MfcEntityArchive entity_archive(
            stream_,
            [this](std::string_view requested) {
                return objects_->read_object_reference(requested);
            },
            [this](const void* value, std::string_view requested) {
                objects_->write_object_reference(value, requested);
            });
        static_cast<creatures1::creatures::BodyPart*>(object)->serialize(
            entity_archive);
        return;
    }
    if (name == "Entity") {
        creatures1::platform::MfcEntityArchive entity_archive(
            stream_,
            [this](std::string_view requested) {
                return objects_->read_object_reference(requested);
            },
            [this](const void* value, std::string_view requested) {
                objects_->write_object_reference(value, requested);
            });
        static_cast<creatures1::objects::Entity*>(object)->serialize(
            entity_archive);
        return;
    }
    serialize_object_payload(object, name, archive);
}

void C1WindowsDocument::ArchiveHost::write_object(const void* object, std::string_view name, creatures1::platform::MfcObjectArchive& archive) {
    if (name == "Creature") {
        creatures1::creatures::Creature* creature =
            creature_for_archive_object(const_cast<void*>(object));
        if (creature == nullptr) {
            throw std::logic_error(
                "C1 archive Creature record has no owning Creature");
        }
        creatures1::platform::MfcCreatureArchive creature_archive(archive);
        creature->serialize(creature_archive);
        return;
    }
    if (name == "CBrain") {
        creatures1::platform::MfcBrainArchive brain_archive(archive);
        const_cast<creatures1::brain::Brain*>(
            static_cast<const creatures1::brain::Brain*>(object))
            ->serialize(brain_archive);
        return;
    }
    if (name == "CBiochemistry") {
        auto* value = const_cast<creatures1::biochemistry::Biochemistry*>(
            static_cast<const creatures1::biochemistry::Biochemistry*>(
                object));
        creatures1::platform::MfcBiochemistryArchive biochemistry_archive(
            archive,
            [this](void* owner) { return creature_for_archive_object(owner); },
            [this](const creatures1::creatures::Creature* creature) {
                return archive_object_for_creature(creature);
            });
        creatures1::platform::MfcBiochemistryLocusHost locus_host(*value);
        value->serialize(biochemistry_archive, locus_host);
        return;
    }
    if (name == "CInstinct") {
        creatures1::platform::MfcInstinctArchive instinct_archive(archive);
        const_cast<creatures1::brain::Instinct*>(
            static_cast<const creatures1::brain::Instinct*>(object))
            ->serialize(instinct_archive);
        return;
    }
    if (name == "COwner") {
        creatures1::platform::MfcStringArchive string_archive(archive);
        const_cast<creatures1::creatures::Owner*>(
            static_cast<const creatures1::creatures::Owner*>(object))
            ->serialize(string_archive);
        return;
    }
    if (name == "Macro") {
        creatures1::platform::MfcMacroArchive macro_archive(stream_, *objects_);
        const_cast<creatures1::scripting::Macro*>(
            static_cast<const creatures1::scripting::Macro*>(object))
            ->serialize(macro_archive);
        return;
    }
    if (name == "MapData") {
        static_cast<void>(object);
        serialize_map_data_payload();
        return;
    }
    if (name == "CGallery") {
        creatures1::platform::MfcMapDataArchive gallery_archive(
            stream_,
            [this]() { return static_cast<creatures1::display::Gallery*>(
                objects_->read_object_reference("CGallery")); },
            [this](creatures1::display::Gallery* gallery) {
                objects_->write_object_reference(gallery, "CGallery");
        });
        creatures1::display::serialize_gallery(
            *const_cast<creatures1::display::Gallery*>(
                static_cast<const creatures1::display::Gallery*>(object)),
            gallery_archive);
        return;
    }
    if (name == "Entity" || name == "Body" || name == "Limb") {
        // Entity/BodyPart use one serialize() for both directions, so writing
        // needs a non-const object exactly like every other class here; the
        // archive hands writers a const void* and each branch casts it back.
        // Throwing instead meant no world containing an Entity -- that is,
        // any world -- could ever be saved.
        creatures1::platform::MfcEntityArchive entity_archive(
            stream_,
            [this](std::string_view requested) {
                return objects_->read_object_reference(requested);
            },
            [this](const void* value, std::string_view requested) {
                objects_->write_object_reference(value, requested);
            });
        if (name == "Entity") {
            const_cast<creatures1::objects::Entity*>(
                static_cast<const creatures1::objects::Entity*>(object))
                ->serialize(entity_archive);
        } else {
            const_cast<creatures1::creatures::BodyPart*>(
                static_cast<const creatures1::creatures::BodyPart*>(object))
                ->serialize(entity_archive);
        }
        return;
    }
    serialize_object_payload(const_cast<void*>(object), name, archive);
}

void C1WindowsDocument::ArchiveHost::serialize_object_payload( void* object, std::string_view name, creatures1::platform::MfcObjectArchive& archive) {
    auto* value = static_cast<creatures1::objects::Object*>(object);
    if (name == "Scenery") {
        static_cast<creatures1::objects::Scenery*>(value)->serialize(archive);
    } else if (name == "CallButton") {
        static_cast<creatures1::objects::CallButton*>(value)->serialize(archive);
    } else if (name == "Lift") {
        static_cast<creatures1::objects::Lift*>(value)->serialize(archive);
    } else if (name == "Vehicle") {
        static_cast<creatures1::objects::Vehicle*>(value)->serialize(archive);
    } else if (name == "Blackboard") {
        static_cast<creatures1::brain::Blackboard*>(value)->serialize(archive);
    } else if (name == "CompoundObject") {
        static_cast<creatures1::objects::CompoundObject*>(value)->serialize(archive);
    } else if (name == "SimpleObject") {
        static_cast<creatures1::objects::SimpleObject*>(value)->serialize(archive);
    } else if (name == "PointerTool") {
        // PointerTool's record carries its text buffer, so its archive host
        // also owns the text-input activation the recovered load performs.
        class PointerToolArchive final
            : public creatures1::ui::PointerToolArchiveHost {
        public:
            explicit PointerToolArchive(C1WindowsDocument& document)
                : document_(document) {}

            creatures1::objects::ObjectRenderableSetHost& renderables()
                override {
                return document_.renderables();
            }
            int mouse_world_x() const override {
                return document_.mouse_world_x();
            }
            int mouse_world_y() const override {
                return document_.mouse_world_y();
            }
            void redraw_after_simple_object_move(
                creatures1::objects::SimpleObject& object,
                const creatures1::world::WorldRect& old_bounds,
                const creatures1::world::WorldRect& new_bounds) override {
                document_.redraw_after_simple_object_move(object, old_bounds,
                                                          new_bounds);
            }
            void activate_pointer_tool_text_input(
                creatures1::ui::PointerTool& tool,
                std::uint32_t maximum_length,
                std::uint32_t allowed_characters) override {
                static_cast<void>(tool);
                static_cast<void>(maximum_length);
                static_cast<void>(allowed_characters);
                // The saved world can restore a pointer tool mid-text-entry.
                // The port's text input is driven from the view's key ring,
                // which is not live during load, so nothing is armed here.
            }

        private:
            C1WindowsDocument& document_;
        };

        PointerToolArchive runtime(document_);
        static_cast<creatures1::ui::PointerTool*>(value)->serialize(archive,
                                                                    runtime);
    } else if (name == "Bubble") {
        static_cast<creatures1::objects::Bubble*>(value)->serialize(archive);
    } else if (name == "Object") {
        value->serialize(archive);
    } else {
        throw std::logic_error("unsupported C1 object payload: " +
                               std::string(name));
    }
}

void C1WindowsDocument::Serialize(CArchive& archive) {
    if (semantic_document_ == nullptr) {
        throw std::logic_error("C1 document serialization has no semantic document");
    }
    ArchiveHost host(*this, archive);
    semantic_document_->serialize(host);
}

void C1WindowsDocument::pan_view_to_selected_creature() {
    // `dde: panc` sets viewport navigation to manual and then centres on the
    // selected creature only when it is inside the pan region -- it is not the
    // unconditional origin request return_viewport_navigation_to_selection
    // performs.
    if (world_view_ != nullptr) {
        world_view_->set_navigation_mode(
            creatures1::ui::ViewportNavigationMode::manual);
    }
    if (renderer_ != nullptr) {
        renderer_->center_viewport_on_selected_creature_if_in_pan_region();
    }
}

void C1WindowsDocument::apply_view_sound_policy(
    creatures1::ui::SfcViewSoundPolicy policy) {
    if (world_view_ != nullptr) {
        world_view_->apply_sound_policy(policy);
    }
}

void C1WindowsDocument::update_keyboard_scroll() {
    if (world_view_ != nullptr) {
        world_view_->update_keyboard_scroll_for_world_tick();
    }
}

bool C1WindowsDocument::manual_viewport_navigation() const {
    return world_view_ != nullptr &&
           world_view_->manual_navigation_for_world_tick();
}

void C1WindowsDocument::reset_manual_navigation_safe_frame_count() {
    manual_navigation_safe_frame_count_ = 0;
    if (world_view_ != nullptr) {
        world_view_->set_manual_navigation_safe_frame_count_for_world_tick(0);
    }
}

std::uint32_t C1WindowsDocument::manual_navigation_safe_frame_count() const {
    return world_view_ == nullptr
               ? manual_navigation_safe_frame_count_
               : world_view_->manual_navigation_safe_frame_count_for_world_tick();
}

void C1WindowsDocument::reset_world_scrollbars() {
    reset_renderer_navigation();
    if (world_view_ != nullptr) {
        world_view_->reset_world_scrollbars_for_world_tick();
    }
}

IMPLEMENT_DYNCREATE(C1WindowsDocument, CDocument)

} // namespace creatures1::platform

namespace creatures1::platform {

creatures1::creatures::SkeletonSpriteBuildServices
C1WindowsDocument::skeleton_services(
    creatures1::creatures::SkeletonLifetimeHost& /*lifetime_host*/) {
    if (world_runtime_ == nullptr) {
        throw std::logic_error(
            "C1 skeleton sprite build has no active WorldRuntime");
    }
    return creatures1::creatures::SkeletonSpriteBuildServices{
        *resources_,      // body_resources
        *this,            // gallery_lifetime_host; document lifetime
                          // outlives every world-owned Skeleton
        *resources_,      // gallery_host
        // The registry must be the one the lifetime host releases into.  This
        // used to be *resources_, whose C1ResourceHost keeps a second,
        // unrelated gallery vector -- so a creature's body sprites were
        // registered there and then released through WorldRuntime, which threw
        // "gallery release for unknown gallery" and killed the application the
        // moment anything built a creature sprite.
        *world_runtime_,  // gallery_registry
        *resources_,      // output_files
        *resources_,      // binary_files
        pixel_cache_,
        sprite_files_,
        game_palette_.dta_buffers[0],
        palette_build_count_,
        secondary_resource_directory(kImageDirectoryIndex),
        resource_paths_[kImageDirectoryIndex],
        &entity_registry(),
        100};
}

} // namespace creatures1::platform
