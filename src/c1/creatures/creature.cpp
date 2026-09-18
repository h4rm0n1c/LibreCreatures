#include "creature.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "../../../include/C1GoalDirectionActionCandidateScoreTable.hpp"
#include "../../../include/C1PoseAnimationData.hpp"
#include "../world/geometry.hpp"
#include "../objects/events.hpp"

namespace creatures1::creatures {
namespace {

// Exact C1 .data strings recovered at 004667e0 and 004667b4.
constexpr char kLearnedWordVowels[] = "aeiouAEIOU";
constexpr char kLearnedWordConsonants[] =
    "bcdfghjklmnpqrstvwxyzBCDFGHJKLMNPQRSTVWXYZ";

// Exact C1 .data tables recovered at 00454304 and 00454314. The selected
// action is read as the native signed byte before indexing these 16 entries;
// valid genome/runtime action selections are the non-negative table slots.
constexpr std::array<std::uint8_t, 16> kActionPhraseNeedsTarget = {
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr std::array<std::uint8_t, 16> kActionPhraseWordIndices = {
    0x4a, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x0a, 0xff, 0xff, 0xff, 0xff,
    0xff};

// These are the three pointer tables at 00454330, 004543d0, and 00454410
// in the executable. They are source-owned data, not an export-time dump:
// the empty entries in group 1 are real and are skipped by the original
// initializer.
constexpr const char* kDefaultVocabularyGroup1[40] = {
    "",       "hand",   "button", "nature", "herb",   "egg",    "food",
    "drink",  "dispenser", "music", "animal", "heat", "comfort", "toy",
    "bigtoy", "weed",   "",       "",       "",       "",       "",       "",
    "",       "",       "",       "",       "vehicle", "lift", "computer",
    "gadget", "cannon", "",      "",       "",       "",       "",       "",
    "Norn",   "Grendel", "",
};

constexpr const char* kDefaultVocabularyGroup2[16] = {
    "pain", "sad", "hungry", "cold", "hot", "tired", "sleepy", "lonely",
    "crowded", "scared", "bored", "angry", "friendly", "blib", "wib", "blub",
};

constexpr const char* kDefaultVocabularyGroup3[16] = {
    "stay", "push", "pull", "stop", "come", "run", "get", "drop",
    "why", "rest", "west", "east", "aaa", "bbb", "ccc", "ddd",
};

// The pointer-held render plane is a native sentinel, not a normal C1 layer.
constexpr int kPointerHeldRenderPlane = 9999;

// Native C1 returns one shared writable byte for an unrecognised creature
// locus.  The biochemistry loader can therefore retain its exact pointer
// contract while the diagnostic/reporting decision remains an application
// host concern.
std::uint8_t g_unresolved_creature_locus_sentinel = 0;

void copy_default_vocabulary_word(LearnedWordRecord& record,
                                  const char* word) {
    const std::size_t length = std::strlen(word);
    std::memcpy(record.recognized_word, word, length + 1);
    std::memcpy(record.response_word, word, length + 1);
    record.reinforcement = 0xff;
}

std::uint32_t random_vocabulary_bank_index() {
    std::uint32_t index = static_cast<std::uint32_t>(std::rand()) &
                          0x80000003u;
    // rand() is non-negative on the Microsoft CRT, so this branch is not
    // taken in the shipped runtime. Preserve the observed normalization.
    if (static_cast<std::int32_t>(index) < 0) {
        index = ((index - 1u) | 0xfffffffcu) + 1u;
    }
    return index;
}

void copy_randomized_word(LearnedWordRecord& record,
                          const char* word,
                          const MultibyteTextApi& text_api) {
    text_api.copy_string(record.recognized_word,
                          kLearnedWordTextCapacity,
                          word);
    text_api.copy_string(record.response_word,
                          kLearnedWordTextCapacity,
                          word);
}

const char* next_or_stop(const MultibyteTextApi& text_api,
                         const char* cursor,
                         const char* limit) {
    const char* next = text_api.next_character(cursor);
    if (next <= cursor || next > limit) {
        return limit;
    }
    return next;
}

const char* find_prefix_end(const char* recognized_word,
                            const MultibyteTextApi& text_api) {
    const char* const limit = recognized_word + kLearnedWordTextCapacity - 1;
    const bool starts_with_vowel =
        *recognized_word != '\0' &&
        text_api.find_character(
            kLearnedWordVowels,
            static_cast<unsigned char>(*recognized_word)) != nullptr;
    const char* const selected_table = starts_with_vowel
                                           ? kLearnedWordConsonants
                                           : kLearnedWordVowels;
    const char* cursor = recognized_word;

    while (*cursor != '\0') {
        const bool is_vowel =
            text_api.find_character(
                kLearnedWordVowels,
                static_cast<unsigned char>(*cursor)) != nullptr;
        if (is_vowel != starts_with_vowel) {
            break;
        }
        const char* next = next_or_stop(text_api, cursor, limit);
        if (next == limit && *next == '\0') {
            cursor = next;
            break;
        }
        if (next == limit) {
            cursor = next;
            break;
        }
        cursor = next;
    }

    // The executable includes the first character in the opposite class.
    if (*cursor != '\0' &&
        text_api.find_character(
            selected_table, static_cast<unsigned char>(*cursor)) != nullptr) {
        cursor = next_or_stop(text_api, cursor, limit);
    }
    return cursor;
}

void copy_prefix_preserving_existing_first_byte(
    LearnedWordRecord& record,
    const char* prefix_end,
    const MultibyteTextApi& text_api) {
    const char* source_cursor = record.recognized_word;
    char* response_cursor = record.response_word;

    // This order is observable in the binary: both pointers are advanced
    // before each store.  response_word[0] is therefore preserved, while
    // recognized_word[1..] is copied into response_word[1..].
    while (source_cursor < prefix_end) {
        source_cursor = text_api.next_character(source_cursor);
        const char* const next_response_cursor =
            text_api.next_character(response_cursor);
        response_cursor += next_response_cursor - response_cursor;
        if (source_cursor > prefix_end) {
            break;
        }
        *response_cursor = *source_cursor;
    }
    *response_cursor = '\0';
}

} // namespace

namespace {

constexpr int kPointerCandidateDistanceLimit = 500;
constexpr int kPointerActivationDistance = 150;

int horizontal_center(const world::WorldRect& bounds) {
    return bounds.min_x + (bounds.max_x - bounds.min_x) / 2;
}

bool contains_point(const world::WorldRect& bounds, int x, int y) {
    // This is the Win32 PtInRect convention used by the executable: the
    // right and bottom edges are outside the rectangle.
    return bounds.min_x <= x && x < bounds.max_x &&
           bounds.min_y <= y && y < bounds.max_y;
}

std::uint32_t classifier_to_attention_index(
    const AttentionClassifier& classifier) {
    return get_attention_record_index(classifier);
}

std::string decimal(int value) {
    return std::to_string(value);
}

std::uint8_t scaled_byte(std::uint8_t value, std::uint32_t magnitude) {
    return static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(value) * (magnitude & 0xffu)) >> 8);
}

std::uint8_t normalize_modulo(std::uint8_t value, std::uint32_t modulus) {
    return modulus == 0 ? 0
                        : static_cast<std::uint8_t>(value % modulus);
}

void append_gait_byte(std::array<char, kGaitAnimationSequenceCapacity>& out,
                      std::size_t index,
                      std::uint8_t value) {
    out[index * 2] = static_cast<char>('0' + value / 10);
    out[index * 2 + 1] = static_cast<char>('0' + value % 10);
}

void clear_lobe_activity(brain::Lobe& lobe, std::size_t maximum_neurons) {
    const std::size_t count = std::min<std::size_t>(
        maximum_neurons, static_cast<std::size_t>(lobe.neuron_count_value()));
    for (std::size_t index = 0; index < count; ++index) {
        brain::LobeNeuron& neuron = lobe.neuron(
            static_cast<std::uint32_t>(index));
        neuron.firing_strength = 0;
        neuron.activation = 0;
    }
}

std::uint8_t saturating_add_byte(std::uint8_t value,
                                 std::uint32_t increment) {
    const std::uint32_t sum = static_cast<std::uint32_t>(value) + increment;
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(sum, 0xffu));
}

std::uint32_t normalize_action_trigger_roll(std::uint32_t random_value) {
    std::uint32_t roll = random_value & 0x800000ffu;
    if (static_cast<std::int32_t>(roll) < 0) {
        roll = ((roll - 1u) | 0xffffff00u) + 1u;
    }
    return roll;
}

} // namespace

std::uint8_t* Creature::resolve_genome_locus(
    GenomeLocusKind kind, std::uint8_t tissue_index,
    std::uint8_t locus_index) {
    if (tissue_index >= 6) {
        return &g_unresolved_creature_locus_sentinel;
    }

    ControlState& controls = instinct_runtime_state_.control_state;
    if (kind == GenomeLocusKind::receptor) {
        switch (tissue_index) {
        case 0:
            return locus_index < 7
                       ? &controls.life_stage_advance_signals[locus_index]
                       : &g_unresolved_creature_locus_sentinel;
        case 1:
            return locus_index < controls.floating_loci.size()
                       ? &controls.floating_loci[locus_index]
                       : &g_unresolved_creature_locus_sentinel;
        case 2:
            if (locus_index == 0) {
                return &controls.ovulation_signal;
            }
            if (locus_index == 1) {
                return &controls.conception_probability;
            }
            return &g_unresolved_creature_locus_sentinel;
        case 3:
            return locus_index == 0 ? &controls.death_signal
                                    : &g_unresolved_creature_locus_sentinel;
        case 4:
            if (locus_index < controls.involuntary_actions.size()) {
                return &controls.involuntary_actions[locus_index]
                            .trigger_probability;
            }
            if (locus_index >= 8 && locus_index < 16) {
                return &controls.gait_locus_levels[locus_index - 8];
            }
            return &g_unresolved_creature_locus_sentinel;
        case 5:
            return locus_index < controls.goal_direction_drive_levels.size()
                           && locus_index != 10
                       ? &controls.goal_direction_drive_levels[locus_index]
                       : &g_unresolved_creature_locus_sentinel;
        }
    } else {
        switch (tissue_index) {
        case 0:
            return locus_index == 0
                       ? &skeleton_.pose_transition_component_count
                       : &g_unresolved_creature_locus_sentinel;
        case 1:
            return locus_index < controls.floating_loci.size()
                       ? &controls.floating_loci[locus_index]
                       : &g_unresolved_creature_locus_sentinel;
        case 2:
            if (locus_index == 0) {
                return &controls.ambient_light_signal;
            }
            if (locus_index == 1) {
                return &controls.crowdedness_signal;
            }
            return &g_unresolved_creature_locus_sentinel;
        case 3:
            return locus_index == 0 ? &death_state_
                                    : &g_unresolved_creature_locus_sentinel;
        case 4:
            switch (locus_index) {
            case 0: return &controls.always_on_signal;
            case 1: return &controls.asleep_signal;
            case 2: return &controls.air_coldness_signal;
            case 3: return &controls.air_hotness_signal;
            case 4: return &controls.ambient_light_signal;
            case 5: return &controls.crowdedness_signal;
            default: break;
            }
            return &g_unresolved_creature_locus_sentinel;
        case 5:
            return locus_index < controls.goal_direction_drive_levels.size()
                           && locus_index != 10
                       ? &controls.goal_direction_drive_levels[locus_index]
                       : &g_unresolved_creature_locus_sentinel;
        }
    }
    return &g_unresolved_creature_locus_sentinel;
}

Creature::Creature(GenomeFilenameId genome_source_filename,
                   CreatureConstructionSex construction_sex,
                   CreatureConstructionHost& host)
    : Creature() {
    // Skeleton's native constructor has already established Object and pose
    // state.  The game constructor then replaces only the genome identity
    // before the common runtime reset and genome load.
    skeleton_.genome_source_filename = genome_source_filename;

    for (AttentionRecord& record : attention_records_) {
        record = {};
        record.target_neuron_index = 0xff;
        record.world_x = -1;
        record.world_y = -1;
        record.visible = true;
    }
    for (LearnedWordRecord& record : learned_word_records_) {
        record.recognized_word[0] = '\0';
        record.response_word[0] = '\0';
        record.reinforcement = 0;
    }

    // The native RANDOM path counts living males and females in the global
    // selection array, then chooses proportionally.  The decompiler aliases
    // those two counters with constructor scratch values; these named
    // counters are the recovered dataflow after removing that alias noise.
    if (construction_sex == CreatureConstructionSex::random) {
        std::size_t male_count = 0;
        std::size_t female_count = 0;
        const std::size_t population_size = host.living_creature_count();
        for (std::size_t index = 0; index < population_size; ++index) {
            const Creature* candidate = host.living_creature_at(index);
            if (candidate == nullptr) {
                host.report_invalid_living_creature_index();
                continue;
            }
            if (candidate->death_state() != 0) {
                continue;
            }
            if (candidate->genome_sex() == GenomeSex::female) {
                ++female_count;
            } else {
                ++male_count;
            }
        }

        const std::size_t population_gender_count = male_count + female_count;
        if (population_gender_count == 0) {
            construction_sex =
                (std::rand() & 1) == 0
                    ? CreatureConstructionSex::male
                    : CreatureConstructionSex::female;
        } else {
            const std::size_t selected =
                static_cast<std::size_t>(std::rand()) %
                population_gender_count;
            construction_sex = selected < female_count
                                   ? CreatureConstructionSex::female
                                   : CreatureConstructionSex::male;
        }
    }

    initialize_runtime_state(host.initialization_host());
    genome_sex_ = construction_sex == CreatureConstructionSex::female
                      ? GenomeSex::female
                      : GenomeSex::male;

    brain_ = std::make_unique<brain::Brain>();
    biochemistry_ = std::make_unique<biochemistry::Biochemistry>();
    biochemistry_->set_owner(this);
    initialize_from_genome(host.genome_initialization_host());

    // The native constructor refreshes these three monikers after genome
    // loading, because Skeleton may have supplied parental IDs from the file.
    register_state_.history().genome_moniker =
        host.initialization_host().format_moniker(
            skeleton_.genome_source_filename);
    register_state_.history().father_moniker =
        host.initialization_host().format_moniker(skeleton_.father_moniker);
    register_state_.history().mother_moniker =
        host.initialization_host().format_moniker(skeleton_.mother_moniker);

    skeleton_.set_bounds_mode(
        static_cast<std::uint32_t>(objects::Object::BoundsMode::unbounded_2),
        host.renderable_set());
    skeleton_.update_movement_bounds(host.movement_bounds());
    skeleton_.set_down_foot_position_and_recompute_layout(
        1000, 2000, host.movement_bounds(), host.sound_playback());
    skeleton_.apply_pose_string("211111111111111", host.sound_playback());
    initialize_learned_word_records(skeleton_.classifier_base(),
                                    host.text_api());
    biochemistry_->update(biochemistry_tick_);
    update_environment_and_life_stage(host.environment());
    instinct_runtime_state_.dream_countdown = 1;
    skeleton_.enable_ticking();
    host.append_to_creature_registry(*this);
    host.rebuild_creature_selection_menu();
}

void Creature::ControlState::reset_to_initial_state() {
    *this = {};
    always_on_signal = 0xff;
    ambient_light_signal = 0x80;
}

std::uint32_t resolve_pointer_attention_target(PointerAttentionApi& api) {
    const int creature_x = api.creature_sound_source_x();
    const int pointer_x = api.pointer_tool_sound_source_x();
    const world::WorldRect movement_bounds = api.creature_movement_bounds();

    int nearest_distance = std::numeric_limits<int>::max();
    std::size_t selected_index = 0;
    bool selected = false;

    for (std::size_t index = 0; index < api.non_scenery_object_count(); ++index) {
        PointerAttentionCandidate candidate;
        if (!api.read_non_scenery_object(index, candidate)) {
            api.report_registry_bounds_failure();
            continue;
        }
        if (candidate.is_pointer_tool || candidate.is_this_creature ||
            candidate.blocks_pointer_attention) {
            continue;
        }

        const int center_x = horizontal_center(candidate.bounds);
        const int center_y = candidate.bounds.min_y +
                             (candidate.bounds.max_y - candidate.bounds.min_y) / 2;
        const bool candidate_between_pointer_and_creature =
            (pointer_x < creature_x && center_x < creature_x) ||
            (creature_x < pointer_x && creature_x < center_x);
        const int creature_distance = std::abs(center_x - creature_x);
        const int pointer_distance = std::abs(pointer_x - center_x);

        if (candidate_between_pointer_and_creature &&
            creature_distance < kPointerCandidateDistanceLimit &&
            contains_point(movement_bounds, center_x, center_y) &&
            pointer_distance < nearest_distance) {
            nearest_distance = pointer_distance;
            selected_index = index;
            selected = true;
        }
    }

    const std::uint32_t pointer_tool_index =
        classifier_to_attention_index(api.pointer_tool_classifier());
    if (!selected || nearest_distance >= kPointerActivationDistance) {
        return pointer_tool_index;
    }

    api.clear_pointer_tool_lobe_neuron(pointer_tool_index);

    PointerAttentionCandidate selected_candidate;
    if (!api.read_non_scenery_object(selected_index, selected_candidate)) {
        api.report_registry_bounds_failure();
        return pointer_tool_index;
    }
    return classifier_to_attention_index(selected_candidate.classifier);
}

std::string format_status_for_external_query(
    const CreatureStatusSnapshot& creature,
    const CreatureStatusTextApi& text,
    const CreatureStatusWorldApi& world) {
    const int age_hours = static_cast<int>(creature.age_ticks / 36000u);
    const int age_minutes =
        static_cast<int>((creature.age_ticks / 600u) % 60u);

    std::string result;
    result.reserve(128);
    result.append(creature.display_name);
    result.push_back('|');
    result.append(creature.genome_moniker);
    result.push_back('|');
    result.append(creature.gender == CreatureGender::male ? "1" : "2");
    result.push_back('|');
    result += decimal(age_hours);
    result.push_back(':');
    if (age_minutes < 10) {
        result.push_back('0');
    }
    result += decimal(age_minutes);
    result.push_back('|');

    if (creature.gender == CreatureGender::male) {
        result.append(text.text(StatusTextKey::male_life_stage));
    } else if (!creature.pregnant) {
        result.append(text.text(StatusTextKey::not_pregnant));
    } else {
        const int pregnancy_stage =
            std::min(8, static_cast<int>(creature.gestation_chemical_level / 32u) + 1);
        result += decimal(pregnancy_stage);
    }
    result.push_back('|');

    const int life_force =
        static_cast<int>(creature.life_force_chemical_level) * 100 / 255;
    if (creature.dead || life_force == 0) {
        result.append(text.text(StatusTextKey::dead));
    } else {
        result += decimal(life_force);
        result.push_back('%');
    }
    result.push_back('|');

    bool sick = false;
    for (const std::uint8_t level : creature.sickness_chemical_levels) {
        sick = sick || level > 0x32;
    }
    if (creature.dead) {
        result.append(text.text(StatusTextKey::dead));
    } else if (sick) {
        result.append(text.text(StatusTextKey::sick));
    } else {
        result.append(text.text(StatusTextKey::healthy));
    }
    result.push_back('|');

    int room_index = -1;
    int best_vertical_distance = std::abs(creature.down_foot_y - 9999);
    for (std::size_t index = 0; index < world.room_count(); ++index) {
        const CreatureStatusRoom current_room = world.room(index);
        const world::WorldRect& bounds = current_room.bounds;
        if (bounds.min_x <= creature.down_foot_x &&
            creature.down_foot_x <= bounds.max_x) {
            const int vertical_distance =
                std::abs(creature.down_foot_y - bounds.max_y);
            if (vertical_distance < best_vertical_distance) {
                best_vertical_distance = vertical_distance;
                room_index = static_cast<int>(index);
            }
        }
    }
    result += decimal(room_index);
    result.push_back('|');
    result += decimal(creature.down_foot_x);
    result.push_back('|');
    result += decimal(creature.down_foot_y);
    return result;
}

void Creature::regenerate_learned_word_response(
    std::size_t record_index,
    const MultibyteTextApi& text_api) {
    LearnedWordRecord& record = learned_word_records_[record_index];
    const char* const prefix_end =
        find_prefix_end(record.recognized_word, text_api);
    copy_prefix_preserving_existing_first_byte(record, prefix_end, text_api);

    // The length test in the recovered control flow collapses to the same
    // observable condition in both paths: repeat only responses shorter than
    // three bytes.  The original capacities are retained at the CRT edge.
    if (std::strlen(record.response_word) < 3) {
        char response_suffix[12]{};
        text_api.copy_string(response_suffix, 10, record.response_word);
        text_api.append_string(record.response_word,
                               kLearnedWordTextCapacity,
                               response_suffix);
    }

    if (record.response_word[0] == '\0') {
        normalize_learned_word_response(
            record,
            kLearnedWordPhonemeSubstitutions,
            kLearnedWordPhonemeTableSize,
            text_api);
    }
}

void Creature::update_learned_word_record(
    char* heard_word,
    std::size_t record_index,
    objects::Object* source_object,
    const StimulusSourceHost& source_host,
    const MultibyteTextApi& text_api,
    CreatureSpeechHost& speech_host) {
    if (skeleton_.sleep_indicator_active) {
        return;
    }

    LearnedWordRecord& record = learned_word_records_[record_index];
    std::uint32_t reinforcement = record.reinforcement & 0xffu;
    if (text_api.compare_strings(heard_word, record.recognized_word) == 0) {
        reinforcement = std::min(reinforcement + 0x3cu, 0xffu);
        record.reinforcement = reinforcement;

        if (reinforcement > 0x4fu) {
            // The native call at 0040a138 is the admitted C1 normalizer;
            // its Microsoft CRT searches and copies remain behind text_api.
            normalize_learned_word_response(
                record,
                kLearnedWordPhonemeSubstitutions,
                kLearnedWordPhonemeTableSize,
                text_api);
        }
        if (reinforcement > 0x9fu) {
            text_api.copy_string(record.response_word,
                                 kLearnedWordTextCapacity,
                                 record.recognized_word);
        }
    } else {
        reinforcement = reinforcement > 0x2cu ? reinforcement - 0x2du : 0;
        record.reinforcement = reinforcement;
    }

    if (reinforcement < 1) {
        text_api.copy_string(record.recognized_word,
                             kLearnedWordTextCapacity,
                             heard_word);
        regenerate_learned_word_response(record_index, text_api);
        record.reinforcement = 0x3c;
        reinforcement = 0x3c;
    }

    // The executable suppresses three quarters of spontaneous responses to
    // creature sources. This is the same low-two-bit normalization used by
    // the recovered vocabulary initializer; rand remains the CRT boundary.
    if (source_object != nullptr &&
        source_host.classify(*source_object).family ==
            AttentionObjectFamily::creature &&
        random_vocabulary_bank_index() != 0) {
        return;
    }

    speak(record.response_word, speech_host);
}

void Creature::speak_action_phrase(
    const MultibyteTextApi& text_api,
    CreatureSpeechPhraseHost& speech_host,
    CreatureSpeechHost& speech_output) {
    if (skeleton_.sleep_indicator_active) {
        return;
    }

    char phrase_buffer[0x50] = {};
    if (std::rand() % 3 == 0) {
        text_api.append_string(
            phrase_buffer, sizeof(phrase_buffer),
            learned_word_records_[0x10].response_word);
        text_api.append_string(phrase_buffer, sizeof(phrase_buffer), " ");

        objects::Object& creature_object =
            speech_host.object_for_creature(*this);
        const std::int32_t attention_index =
            speech_host.attention_record_index(creature_object);
        speech_host.queue_speech_event(
            creature_object, static_cast<std::uint32_t>(attention_index) + 0x10,
            0);
    }

    const std::int8_t action_index = static_cast<std::int8_t>(
        static_cast<std::uint8_t>(selected_action_id_ & 0xffu));
    if (action_index < 0 ||
        static_cast<std::size_t>(action_index) >=
            kActionPhraseWordIndices.size()) {
        return;
    }

    const std::uint8_t verb_word_slot =
        kActionPhraseWordIndices[static_cast<std::size_t>(action_index)];
    if (verb_word_slot == 0xff) {
        return;
    }

    text_api.append_string(phrase_buffer, sizeof(phrase_buffer),
                           learned_word_records_[verb_word_slot].response_word);

    std::int32_t voice_delay_ticks = 0;
    const bool add_motion_target =
        skeleton_.motion_link != nullptr &&
        kActionPhraseNeedsTarget[static_cast<std::size_t>(action_index)] != 0 &&
        std::rand() % 3 == 0;
    if (!add_motion_target) {
        phrase_buffer[0x18] = '\0';
        speak_text_with_voice(std::string_view(phrase_buffer),
                              voice_delay_ticks, speech_host);
    } else {
        text_api.append_string(phrase_buffer, sizeof(phrase_buffer), " ");
        const std::int32_t target_attention_index =
            speech_host.attention_record_index(*skeleton_.motion_link);
        text_api.append_string(
            phrase_buffer, sizeof(phrase_buffer),
            learned_word_records_[static_cast<std::size_t>(
                target_attention_index + 0x10)]
                .response_word);
        phrase_buffer[0x18] = '\0';
        speak_text_with_voice(std::string_view(phrase_buffer),
                              voice_delay_ticks, speech_host);

        const std::int32_t target_attention_index_after_voice =
            speech_host.attention_record_index(*skeleton_.motion_link);
        objects::Object& creature_object =
            speech_host.object_for_creature(*this);
        speech_host.queue_speech_event(
            creature_object,
            static_cast<std::uint32_t>(target_attention_index_after_voice) +
                0x10,
            voice_delay_ticks);
    }

    objects::Object& creature_object = speech_host.object_for_creature(*this);
    speech_host.queue_speech_event(
        creature_object, static_cast<std::uint32_t>(verb_word_slot),
        voice_delay_ticks);
    speak(std::string_view(phrase_buffer), speech_output);
}

void Creature::speak_dominant_drive_phrase(
    const MultibyteTextApi& text_api,
    CreatureSpeechPhraseHost& phrase_host,
    CreatureSpeechHost& speech_output) {
    if (skeleton_.sleep_indicator_active || death_state_ != 0) {
        return;
    }

    const GoalDriveLevels& drives =
        instinct_runtime_state_.control_state.goal_direction_drive_levels;
    std::size_t dominant_drive_index = 0;
    std::uint8_t dominant_drive_level = drives[0];
    for (std::size_t index = 1; index < GoalDriveLevels::size(); ++index) {
        // The native CMOVLE sequence retains the earlier slot on a tie.
        if (dominant_drive_level < drives[index]) {
            dominant_drive_level = drives[index];
            dominant_drive_index = index;
        }
    }

    char phrase_buffer[0x50] = {};
    auto append_response = [&](std::size_t record_index) {
        text_api.append_string(
            phrase_buffer, sizeof(phrase_buffer),
            learned_word_records_[record_index].response_word);
    };
    auto append_space = [&]() {
        text_api.append_string(phrase_buffer, sizeof(phrase_buffer), " ");
    };

    if (dominant_drive_level < 0x40) {
        append_response(0x10);
        append_space();
        append_response(0x48);
    } else {
        switch (dominant_drive_index) {
        case 0:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x38);
            break;
        case 1:
            append_response(6);
            append_space();
            append_response(0x48);
            append_space();
            append_response(0x10);
            break;
        case 2:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x16);
            break;
        case 3:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x3b);
            break;
        case 4:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x3c);
            break;
        case 5:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x3d);
            break;
        case 6:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x3e);
            break;
        case 7:
        case 8: {
            append_response(dominant_drive_index == 7 ? 4 : 5);
            const objects::Object& creature_object =
                phrase_host.object_for_creature(*this);
            const std::int32_t attention_index =
                phrase_host.attention_record_index(creature_object);
            append_response(static_cast<std::size_t>(attention_index + 0x10));
            break;
        }
        case 9:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(5);
            break;
        case 10:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x42);
            break;
        case 11:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x43);
            break;
        case 12:
            append_default_response_prefix(phrase_buffer, text_api);
            append_response(0x44);
            break;
        default:
            append_response(0x10);
            append_space();
            append_response(dominant_drive_index + 0x38);
            break;
        }
    }

    speak(std::string_view(phrase_buffer), speech_output);
}

void Creature::speak(std::string_view phrase,
                     CreatureSpeechHost& speech_host) {
    speech_host.speak(*this, phrase);
}

void Creature::speak_text_with_voice(
    std::string_view phrase, std::int32_t& out_delay_world_ticks,
    CreatureSpeechPhraseHost& speech_host) {
    speech_host.speak_text_with_voice(
        *this, phrase, out_delay_world_ticks);
}

void Creature::process_heard_words(
    objects::Object* source_object,
    char* mutable_words,
    const StimulusSourceHost& source_host,
    const MultibyteTextApi& text_api,
    CreatureHeardWordsHost& heard_words_host,
    CreatureSpeechHost& speech_output,
    BuiltInStimulusDebugHost* debug_host,
    common::DebugLogHost* log_host) {
    if (skeleton_.sleep_indicator_active || source_object == nullptr ||
        mutable_words == nullptr || *mutable_words == '\0') {
        return;
    }

    if (debug_host != nullptr && debug_host->debug_console_visible() &&
        debug_host->is_selected_creature(*this)) {
        debug_host->log_heard_words(*this, mutable_words);
    }

    std::int32_t name_slot = -1;
    std::int32_t noun_slot = -1;
    std::int32_t verb_slot = -1;
    std::uint32_t attention_magnitude = 0xb4;
    std::int32_t verb_action_neuron = -1;

    // The executable edits the caller's buffer in place: spaces become NUL
    // terminators, and _mbsinc advances over the resulting token boundary.
    char* token = mutable_words;
    while (true) {
        char* word_end = token;
        if (*token == '\0') {
            word_end = nullptr;
        } else {
            while (*word_end != '\0' && *word_end != ' ') {
                word_end = const_cast<char*>(text_api.next_character(word_end));
            }
            if (*word_end == '\0') {
                word_end = nullptr;
            } else {
                *word_end = '\0';
            }
        }

        std::int32_t matching_slot = -1;
        for (std::size_t index = 0;
             index < learned_word_records_.size(); ++index) {
            const LearnedWordRecord& record = learned_word_records_[index];
            if (text_api.compare_strings(token, record.recognized_word) == 0 ||
                text_api.compare_strings(token, record.response_word) == 0) {
                matching_slot = static_cast<std::int32_t>(index);
                break;
            }
        }

        if (matching_slot == 0x10) {
            name_slot = 0x10;
            matching_slot = verb_slot;
            verb_slot = matching_slot;
        } else if (matching_slot >= 0) {
            if (matching_slot >= 0x11 && matching_slot <= 0x36) {
                noun_slot = matching_slot;
            } else {
                verb_slot = matching_slot;
            }
        } else {
            const AttentionClassifier motion_link_classifier =
                skeleton_.motion_link == nullptr
                    ? AttentionClassifier{}
                    : source_host.classify(*skeleton_.motion_link);
            const std::int32_t motion_link_word_slot =
                skeleton_.motion_link == nullptr
                    ? 0x10
                    : static_cast<std::int32_t>(
                          classifier_to_attention_index(
                              motion_link_classifier)) + 0x10;
            update_learned_word_record(
                token, static_cast<std::size_t>(motion_link_word_slot),
                source_object, source_host, text_api, speech_output);
            matching_slot = verb_slot;
            verb_slot = matching_slot;
        }

        noun_slot = matching_slot >= 0 && matching_slot >= 0x11 &&
                            matching_slot <= 0x36
                        ? matching_slot
                        : noun_slot;

        if (word_end == nullptr) {
            break;
        }
        token = const_cast<char*>(text_api.next_character(word_end));
    }

    const AttentionClassifier source_classifier =
        source_host.classify(*source_object);
    if (name_slot < 0 && noun_slot < 0 && verb_slot < 0) {
        trigger_built_in_stimulus(9, source_object, 0, source_host,
                                  debug_host, log_host);
    } else if (heard_words_host.is_pointer_tool(*source_object)) {
        trigger_built_in_stimulus(10, source_object, 0, source_host,
                                  debug_host, log_host);
    } else if (source_classifier.family == AttentionObjectFamily::creature) {
        trigger_built_in_stimulus(11, source_object, 0, source_host,
                                  debug_host, log_host);
    }

    if (skeleton_.motion_link != nullptr) {
        const AttentionClassifier motion_link_classifier =
            source_host.classify(*skeleton_.motion_link);
        const std::int32_t motion_link_base = static_cast<std::int32_t>(
            classifier_to_attention_index(motion_link_classifier));
        if (noun_slot == motion_link_base + 0x10) {
            LearnedWordRecord& record =
                learned_word_records_[static_cast<std::size_t>(noun_slot)];
            update_learned_word_record(
                record.recognized_word, static_cast<std::size_t>(noun_slot),
                source_object, source_host, text_api, speech_output);
        }
    }

    std::int32_t attention_record_index = -1;
    if (name_slot >= 0 && noun_slot < 0 && verb_slot < 0) {
        attention_record_index = static_cast<std::int32_t>(
            classifier_to_attention_index(source_classifier));
    } else {
        attention_record_index = noun_slot < 0 ? -1 : noun_slot - 0x10;
        switch (verb_slot) {
        case 8:
            if ((std::rand() & 1) == 0) {
                attention_record_index = static_cast<std::int32_t>(
                    classifier_to_attention_index(source_classifier));
            }
            verb_action_neuron = verb_slot;
            break;
        case 9:
            attention_record_index = 0;
            verb_action_neuron = verb_slot;
            break;
        case static_cast<std::int32_t>(HeardWordActionSlot::yes):
            trigger_built_in_stimulus(
                heard_words_host.is_pointer_tool(*source_object) ? 1u : 2u,
                source_object, 0, source_host, debug_host, log_host);
            break;
        case static_cast<std::int32_t>(HeardWordActionSlot::no):
            trigger_built_in_stimulus(
                heard_words_host.is_pointer_tool(*source_object) ? 3u : 4u,
                source_object, 0, source_host, debug_host, log_host);
            break;
        case static_cast<std::int32_t>(HeardWordActionSlot::look):
            attention_record_index = static_cast<std::int32_t>(
                resolve_pointer_attention_target(
                    heard_words_host.pointer_attention_api()));
            verb_action_neuron = 0xff;
            attention_magnitude = 0xff;
            break;
        case static_cast<std::int32_t>(HeardWordActionSlot::what):
            speak_action_phrase(text_api, heard_words_host.speech_phrase_host(),
                                speech_output);
            break;
        default:
            if (verb_slot >= 0 && verb_slot < 0x10) {
                verb_action_neuron = verb_slot;
            }
            break;
        }
    }

    std::uint32_t attention_activation = 0xb4;
    std::uint32_t action_commitment = 0xb4;
    if (skeleton_.motion_link == source_object || name_slot >= 0) {
        attention_activation = 0xff;
        action_commitment = 0xff;
        attention_magnitude = 0xff;
    }
    if (noun_slot >= 0) {
        const std::uint32_t reinforcement =
            learned_word_records_[static_cast<std::size_t>(noun_slot)]
                .reinforcement &
            0xffu;
        attention_activation = (attention_activation * reinforcement) >> 8;
        action_commitment = attention_magnitude;
    }
    if (verb_slot >= 0) {
        const std::uint32_t reinforcement =
            learned_word_records_[static_cast<std::size_t>(verb_slot)]
                .reinforcement &
            0xffu;
        action_commitment = (reinforcement * attention_magnitude) >> 8;
    }

    apply_goal_direction(
        attention_record_index, verb_action_neuron,
        static_cast<std::uint8_t>(attention_activation),
        static_cast<std::uint8_t>(action_commitment),
        heard_words_host.goal_direction_host());
}

void Creature::handle_heard_words_event(
    const objects::QueuedObjectEvent& event,
    const StimulusSourceHost& source_host,
    const MultibyteTextApi& text_api,
    CreatureHeardWordsHost& heard_words_host,
    CreatureSpeechHost& speech_output,
    BuiltInStimulusDebugHost* debug_host,
    common::DebugLogHost* log_host) {
    if (death_state_ != 0 || event.source == nullptr) {
        return;
    }

    process_heard_words(
        event.source,
        heard_words_host.mutable_words_for_event(event),
        source_host,
        text_api,
        heard_words_host,
        speech_output,
        debug_host,
        log_host);
}

void Creature::handle_word_learning_event(
    const objects::QueuedObjectEvent& event,
    const StimulusSourceHost& source_host,
    const MultibyteTextApi& text_api,
    const CreatureWordLearningHost& learning_host,
    CreatureSpeechHost& speech_output) {
    if (death_state_ != 0 || event.source == nullptr) {
        return;
    }

    char* heard_word = nullptr;
    std::size_t record_index = 0;
    if (!learning_host.resolve_word_learning_event(
            event, heard_word, record_index) || heard_word == nullptr ||
        record_index >= learned_word_records_.size()) {
        return;
    }

    update_learned_word_record(
        heard_word, record_index, event.source, source_host, text_api,
        speech_output);
}

void Creature::update_attention(CreatureAttentionHost& host) {
    if (dead_) {
        return;
    }

    if (goal_direction_state_.delivery_countdown != 0) {
        --goal_direction_state_.delivery_countdown;
    }

    // The native routine assumes a fully initialized brain. The clean model
    // can be observed during construction, so keep that malformed object from
    // turning this typed port back into a raw null dereference.
    if (brain_ == nullptr) {
        return;
    }

    brain::Lobe& attention_lobe = brain_->lobe(
        static_cast<std::uint32_t>(brain::StandardLobeIndex::attention));
    std::size_t winner_index = 0;
    std::uint8_t winner_firing_strength = 0;
    if (attention_lobe.neuron_count_value() != 0) {
        winner_firing_strength = attention_lobe.neuron(0).firing_strength;
        for (std::size_t index = 1; index < kAttentionRecordCount; ++index) {
            if (index >= attention_lobe.neuron_count_value() ||
                attention_records_[index].target == nullptr) {
                continue;
            }
            const std::uint8_t firing_strength = attention_lobe.neuron(
                static_cast<std::uint32_t>(index)).firing_strength;
            if (winner_firing_strength < firing_strength) {
                winner_index = index;
                winner_firing_strength = firing_strength;
            }
        }
    }

    objects::Object* const new_attention_target =
        attention_records_[winner_index].target;
    if (new_attention_target != skeleton_.motion_link &&
        (new_attention_target != nullptr ||
         !skeleton_.sleep_indicator_active)) {
        skeleton_.motion_link = new_attention_target;
        skeleton_.motion_target_part_index = 0;
        if (new_attention_target != nullptr) {
            new_attention_target->get_part_center(
                &skeleton_.motion_target_x, &skeleton_.motion_target_y, 0);
        }

        clear_lobe_activity(
            brain_->lobe(static_cast<std::uint32_t>(
                brain::StandardLobeIndex::decision)),
            16);
        clear_lobe_activity(
            brain_->lobe(static_cast<std::uint32_t>(
                brain::StandardLobeIndex::general_sensory)),
            32);
        clear_lobe_activity(
            brain_->lobe(static_cast<std::uint32_t>(
                brain::StandardLobeIndex::stimulus_source)),
            40);

        const AttentionRecord& winning_record =
            attention_records_[winner_index];
        if (winning_record.target_neuron_index < 0x20) {
            brain::Lobe& sensory_lobe = brain_->lobe(static_cast<std::uint32_t>(
                brain::StandardLobeIndex::general_sensory));
            const std::size_t target_neuron_index =
                winning_record.target_neuron_index;
            if (target_neuron_index < sensory_lobe.neuron_count_value()) {
                // The executable treats this byte as signed, retaining only
                // values in 0..127 and mapping 0x80..0xff to zero.
                const std::int8_t activation = static_cast<std::int8_t>(
                    winning_record.lobe_activation);
                sensory_lobe.neuron(static_cast<std::uint32_t>(
                    target_neuron_index)).activation =
                    activation >= 0 ? static_cast<std::uint8_t>(activation) : 0;
            }
        }

        brain::Lobe& stimulus_source_lobe = brain_->lobe(
            static_cast<std::uint32_t>(
                brain::StandardLobeIndex::stimulus_source));
        if (winner_index < stimulus_source_lobe.neuron_count_value()) {
            brain::LobeNeuron& neuron = stimulus_source_lobe.neuron(
                static_cast<std::uint32_t>(winner_index));
            neuron.activation = saturating_add_byte(neuron.activation, 0x1e);
        }

        if (skeleton_.sleep_indicator_active) {
            objects::Object* const indicator = sleep_indicator_object_;
            skeleton_.sleep_indicator_active = false;
            if (indicator != nullptr) {
                host.dispatch_sleep_indicator_event(
                    *indicator, objects::ObjectEventId::event_0, indicator, 0);
                host.initialize_sleep_indicator(*indicator);
                sleep_indicator_object_ = nullptr;
            }
        }

        set_action(0, host);
        if (host.debug_console_visible() &&
            host.is_selected_creature(*this)) {
            host.log_attention_shift(*this, skeleton_.motion_link);
        }

        brain::Lobe& decision_lobe = brain_->lobe(static_cast<std::uint32_t>(
            brain::StandardLobeIndex::decision));
        for (std::size_t action_index = 0; action_index < 16; ++action_index) {
            const ActionTargetRequirement requirement =
                host.action_target_requirement(action_index);
            bool eligible = requirement ==
                            ActionTargetRequirement::always_eligible;
            if (!eligible) {
                eligible = skeleton_.motion_link == nullptr
                               ? requirement ==
                                     ActionTargetRequirement::requires_no_target
                               : requirement ==
                                     ActionTargetRequirement::requires_target;
            }
            if (action_index < decision_lobe.neuron_count_value()) {
                decision_lobe.neuron(static_cast<std::uint32_t>(action_index))
                    .winner_take_all_excluded = eligible ? 0 : 1;
            }
        }

        const std::int32_t pending_attention_index =
            goal_direction_state_.attention_record_index;
        if (pending_attention_index != -1 &&
            goal_direction_state_.delivery_countdown != 0 &&
            skeleton_.motion_link != nullptr) {
            const std::uint32_t target_attention_index =
                get_attention_record_index(host.classify_object(
                    *skeleton_.motion_link));
            if (static_cast<std::int32_t>(target_attention_index) ==
                pending_attention_index) {
                brain::Lobe& verb_lobe = brain_->lobe(static_cast<std::uint32_t>(
                    brain::StandardLobeIndex::verb));
                const std::int32_t action_neuron_index =
                    goal_direction_state_.action_lobe_neuron_index;
                if (action_neuron_index >= 0 &&
                    static_cast<std::size_t>(action_neuron_index) <
                        verb_lobe.neuron_count_value()) {
                    brain::LobeNeuron& neuron = verb_lobe.neuron(
                        static_cast<std::uint32_t>(action_neuron_index));
                    neuron.activation = saturating_add_byte(
                        neuron.activation, goal_direction_state_.commitment);
                }
                goal_direction_state_.attention_record_index = -1;
            }
        }
    }

    for (AttentionRecord& record : attention_records_) {
        record.target_neuron_index = 0xff;
    }
}

void Creature::set_action(std::uint32_t action_id,
                          CreatureAttentionHost& host) {
    constexpr std::uint32_t kTargetActionClassifierOffset = 0x10;
    constexpr std::uint32_t kUnlinkedActionClassifierOffset = 0x20;
    constexpr std::uint32_t kQuiescentAction = 0;

    objects::Object& creature_object = host.object_for_creature(*this);
    for (;;) {
        objects::Object* const bounds_reference =
            skeleton_.bounds_reference_object();
        objects::Object* const motion_link = skeleton_.motion_link;
        if (bounds_reference != nullptr &&
            skeleton_.bounds_mode() != objects::Object::BoundsMode::unbounded_1 &&
            motion_link != bounds_reference &&
            host.action_event_target_is_live(*bounds_reference) &&
            motion_link != nullptr && action_id > 0) {
            host.queue_immediate_event(
                creature_object, *bounds_reference,
                objects::ObjectEventId::event_2, 0);
        }

        const std::uint32_t event_offset =
            motion_link == nullptr ? kUnlinkedActionClassifierOffset
                                    : kTargetActionClassifierOffset;
        selected_action_id_ = static_cast<std::uint8_t>(action_id);
        active_involuntary_action_index_ = 0xff;

        if (motion_link != nullptr) {
            std::uint32_t target_classifier = 0;
            const std::uint32_t packed_motion_classifier =
                host.classifier_base(*motion_link);
            if ((packed_motion_classifier & 0xff000000u) != 0x04000000u) {
                target_classifier = packed_motion_classifier;
            }
            const std::uint32_t target_action_classifier =
                target_classifier + action_id + event_offset;
            if (host.execute_script_for_classifier(
                    *this, &creature_object, target_action_classifier, false) !=
                0) {
                if (host.debug_console_visible() &&
                    host.is_selected_creature(*this)) {
                    host.log_override_action_script_selected(
                        *this, target_action_classifier);
                }
                return;
            }
        }

        const std::uint32_t own_action_classifier =
            skeleton_.classifier_base() + action_id + event_offset;
        if (host.execute_script_for_classifier(
                *this, &creature_object, own_action_classifier, false) != 0) {
            return;
        }

        if (host.debug_console_visible() &&
            host.is_selected_creature(*this)) {
            host.log_no_action_script(*this, own_action_classifier);
        }

        const int previous_action = static_cast<int>(
            static_cast<std::int8_t>(selected_action_id_));
        if (brain_ != nullptr && previous_action >= 0 &&
            static_cast<std::size_t>(previous_action) <
                brain_->lobe(static_cast<std::uint32_t>(
                                 brain::StandardLobeIndex::decision))
                    .neuron_count_value()) {
            brain::Lobe& decision_lobe = brain_->lobe(static_cast<std::uint32_t>(
                brain::StandardLobeIndex::decision));
            decision_lobe.neuron(static_cast<std::uint32_t>(previous_action))
                .firing_strength = 0;
            decision_lobe.neuron(static_cast<std::uint32_t>(previous_action))
                .activation = 0;
        }
        action_id = kQuiescentAction;
    }
}

void Creature::update_action_selection(
    CreatureAttentionHost& host, const MultibyteTextApi& text_api,
    CreatureSpeechPhraseHost& phrase_host, CreatureSpeechHost& speech_host) {
    constexpr std::uint32_t kQuiescentAction = 0;
    constexpr std::uint32_t kInvoluntaryActionClassifierOffset = 0x40;
    constexpr std::uint8_t kNoActiveInvoluntaryAction = 0xff;

    if (brain_ == nullptr || instinct_runtime_state_.dream_countdown != 0 ||
        active_involuntary_action_index_ <= 0x7f) {
        return;
    }

    std::uint32_t best_trigger_probability = 0;
    int best_involuntary_action_index = -1;
    int candidate_involuntary_action_index = -1;
    for (std::size_t index = 0;
         index < instinct_runtime_state_.control_state.involuntary_actions.size();
         ++index) {
        InvoluntaryActionState& involuntary =
            instinct_runtime_state_.control_state.involuntary_actions[index];
        if (involuntary.cooldown_ticks == 0) {
            const std::uint32_t trigger_probability =
                involuntary.trigger_probability;
            if (trigger_probability != 0) {
                candidate_involuntary_action_index =
                    best_involuntary_action_index;
                const std::uint32_t trigger_roll =
                    normalize_action_trigger_roll(
                        static_cast<std::uint32_t>(std::rand()));
                if (static_cast<std::int32_t>(trigger_roll) <
                        static_cast<std::int32_t>(trigger_probability) &&
                    best_trigger_probability < trigger_probability) {
                    candidate_involuntary_action_index =
                        static_cast<int>(index);
                    best_trigger_probability = trigger_probability;
                    best_involuntary_action_index = static_cast<int>(index);
                }
            }
        } else {
            --involuntary.cooldown_ticks;
        }
    }

    if (best_trigger_probability != 0 &&
        !skeleton_.sleep_indicator_active &&
        candidate_involuntary_action_index != static_cast<int>(
            static_cast<std::int8_t>(active_involuntary_action_index_))) {
        active_involuntary_action_index_ = static_cast<std::uint8_t>(
            candidate_involuntary_action_index);
        selected_action_id_ = kQuiescentAction;
        action_activation_boost_ = 0;
        const std::uint32_t classifier =
            skeleton_.classifier_base() + kInvoluntaryActionClassifierOffset +
            static_cast<std::uint32_t>(candidate_involuntary_action_index);
        if (host.execute_script_for_classifier(
                *this, &host.object_for_creature(*this), classifier, false) !=
            0) {
            if (host.debug_console_visible() &&
                host.is_selected_creature(*this)) {
                host.log_involuntary_action(
                    *this, candidate_involuntary_action_index);
            }
            return;
        }
    }

    brain::Lobe& decision_lobe = brain_->lobe(static_cast<std::uint32_t>(
        brain::StandardLobeIndex::decision));
    std::uint32_t selected_action = kQuiescentAction;
    std::uint8_t best_firing_strength = 0;
    for (std::uint32_t action = 0; action < 16; ++action) {
        const ActionTargetRequirement requirement =
            host.action_target_requirement(action);
        bool eligible = requirement == ActionTargetRequirement::always_eligible;
        if (!eligible) {
            eligible = skeleton_.motion_link == nullptr
                           ? requirement ==
                                 ActionTargetRequirement::requires_no_target
                           : requirement ==
                                 ActionTargetRequirement::requires_target;
        }
        if (!eligible || action >= decision_lobe.neuron_count_value()) {
            continue;
        }
        const std::uint8_t firing_strength =
            decision_lobe.neuron(action).firing_strength;
        if (best_firing_strength < firing_strength) {
            selected_action = action;
            best_firing_strength = firing_strength;
        }
    }

    const int previous_action = static_cast<int>(
        static_cast<std::int8_t>(selected_action_id_ & 0xffu));
    if (selected_action != static_cast<std::uint32_t>(previous_action)) {
        if (host.debug_console_visible() &&
            host.is_selected_creature(*this)) {
            host.log_action_selection(*this, skeleton_.motion_link,
                                      selected_action);
        }
        if (previous_action >= 0 &&
            static_cast<std::size_t>(previous_action) <
                decision_lobe.neuron_count_value()) {
            decision_lobe.neuron(static_cast<std::uint32_t>(previous_action))
                .firing_strength = 0;
            decision_lobe.neuron(static_cast<std::uint32_t>(previous_action))
                .activation = 0;
        }
        if (skeleton_.sleep_indicator_active) {
            objects::Object* const indicator = sleep_indicator_object_;
            skeleton_.sleep_indicator_active = false;
            if (indicator != nullptr) {
                host.dispatch_sleep_indicator_event(
                    *indicator, objects::ObjectEventId::event_0, indicator, 0);
                host.initialize_sleep_indicator(*indicator);
                sleep_indicator_object_ = nullptr;
            }
        }
        active_involuntary_action_index_ = kNoActiveInvoluntaryAction;
        set_action(selected_action, host);
        if (std::rand() % 6 == 0) {
            if ((std::rand() & 1) == 0) {
                speak_dominant_drive_phrase(text_api, phrase_host, speech_host);
            } else {
                speak_action_phrase(text_api, phrase_host, speech_host);
            }
        }
        if (selected_action < decision_lobe.neuron_count_value()) {
            decision_lobe.neuron(selected_action).activation = saturating_add_byte(
                decision_lobe.neuron(selected_action).activation, 10);
        }
    }
}

void Creature::set_sleep_indicator(bool enabled, CreatureAttentionHost& host) {
    if (enabled == skeleton_.sleep_indicator_active) {
        return;
    }

    skeleton_.sleep_indicator_active = enabled;
    if (enabled) {
        sleep_indicator_object_ = host.create_sleep_indicator(
            *this, skeleton_.normal_render_plane + 10);
        if (sleep_indicator_object_ == nullptr) {
            return;
        }

        const objects::ObjectEventId activation_event =
            (skeleton_.classifier_base() & 0xffff0000U) == 0x04010000U
                ? objects::ObjectEventId::event_1
                : objects::ObjectEventId::event_2;
        host.dispatch_sleep_indicator_event(
            *sleep_indicator_object_, activation_event,
            sleep_indicator_object_, 0);
        return;
    }

    objects::Object* const old_indicator = sleep_indicator_object_;
    if (old_indicator != nullptr) {
        host.dispatch_sleep_indicator_event(
            *old_indicator, objects::ObjectEventId::event_0,
            old_indicator, 0);
        host.initialize_sleep_indicator(*old_indicator);
        sleep_indicator_object_ = nullptr;
    }
}

void Creature::clear_selected_decision_neuron() {
    const int selected_action = static_cast<int>(
        static_cast<std::int8_t>(selected_action_id_ & 0xffu));
    if (brain_ != nullptr && selected_action >= 0 &&
        static_cast<std::uint32_t>(selected_action) <
            brain_->lobe(static_cast<std::uint32_t>(
                             brain::StandardLobeIndex::decision))
                .neuron_count_value()) {
        brain::Lobe& decision_lobe = brain_->lobe(static_cast<std::uint32_t>(
            brain::StandardLobeIndex::decision));
        brain::LobeNeuron& neuron = decision_lobe.neuron(
            static_cast<std::uint32_t>(selected_action));
        neuron.firing_strength = 0;
        neuron.activation = 0;
    }
}

void Creature::stop_current_involuntary_action() {
    constexpr std::uint32_t kQuiescentAction = 0;
    constexpr std::uint8_t kNoActiveInvoluntaryAction = 0xff;

    // Native DONE stores the signed -1 sentinel in the active involuntary
    // action byte before touching the selected decision-lobe neuron.
    active_involuntary_action_index_ = kNoActiveInvoluntaryAction;
    clear_selected_decision_neuron();

    // C1 stores the quiescent action-activation value in the byte adjacent to
    // the selected action. Keep the named state explicit in the clean model.
    action_activation_boost_ = static_cast<std::uint8_t>(kQuiescentAction);
}

void Creature::die(CreatureDeathHost& host) {
    if (death_state_ == 0) {
        host.release_sleep_indicator(*this);
        host.notify_dependents_on_death(*this);
        instinct_runtime_state_.dream_countdown = 0;
        host.purge_destroy_when_finished_macros(*this);
        host.log_death_message(*this);
        host.dispatch_death_event(*this);

        death_state_ = 1;
        skeleton_.eyes_open = false;
        host.rebuild_creature_selection_menu();

        if (host.is_selected_creature(*this)) {
            host.persist_and_close_selected_eye_view();
            host.broadcast_embedded_control_state();
        }
    }

    host.log_goal_drive_table(*this);
}

void Creature::initialize_default_vocabulary() {
    // The native loop calls rand() once for every populated record. The
    // returned value is discarded here exactly as it is by the executable;
    // advancing the CRT PRNG is part of the observable initialization order.
    for (std::size_t index = 0; index < std::size(kDefaultVocabularyGroup3);
         ++index) {
        copy_default_vocabulary_word(learned_word_records_[index],
                                      kDefaultVocabularyGroup3[index]);
        std::rand();
    }

    // The first pointer in group 1 is skipped by the original (+1) cursor;
    // records 17..55 consume the remaining 39 pointers, including empty
    // entries that intentionally do not consume a random value.
    for (std::size_t index = 1; index < std::size(kDefaultVocabularyGroup1);
         ++index) {
        const char* word = kDefaultVocabularyGroup1[index];
        if (*word != '\0') {
            copy_default_vocabulary_word(
                learned_word_records_[0x11 + index - 1], word);
            std::rand();
        }
    }

    for (std::size_t index = 0; index < std::size(kDefaultVocabularyGroup2);
         ++index) {
        copy_default_vocabulary_word(learned_word_records_[0x38 + index],
                                     kDefaultVocabularyGroup2[index]);
        std::rand();
    }

    constexpr const char* kFixedWords[] = {"yes", "no", "look", "what"};
    for (std::size_t index = 0; index < std::size(kFixedWords); ++index) {
        copy_default_vocabulary_word(learned_word_records_[0x48 + index],
                                     kFixedWords[index]);
        std::rand();
    }
}

int Creature::initialize_learned_word_records(
    std::uint32_t classifier_base,
    const MultibyteTextApi& text_api) {
    const std::size_t partition_offset =
        ((classifier_base & 0xffff0000u) != 0x04010000u) ? 4u : 0u;

    auto initialize_class = [&](std::size_t first_record,
                                std::size_t count,
                                std::size_t word_index) {
        for (std::size_t index = 0; index < count; ++index) {
            const VocabularyWordBank& bank =
                kRandomizedVocabularyBanks[partition_offset +
                                           random_vocabulary_bank_index()];
            const char* word = nullptr;
            switch (word_index) {
            case 0:
                word = bank.word_0;
                break;
            case 1:
                word = bank.word_1;
                break;
            case 2:
                word = bank.word_2;
                break;
            case 3:
                word = bank.word_3;
                break;
            default:
                word = bank.word_4;
                break;
            }
            LearnedWordRecord& record =
                learned_word_records_[first_record + index];
            copy_randomized_word(record, word, text_api);
            record.reinforcement =
                static_cast<std::uint32_t>(std::rand() % 0x15 + 0x1e);
        }
    };

    initialize_class(0, 0x10, 0);
    initialize_class(0x10, 0x28, 1);
    initialize_class(0x38, 0x10, 2);
    initialize_class(0x48, 8, 3);

    const VocabularyWordBank& final_bank =
        kRandomizedVocabularyBanks[partition_offset +
                                   random_vocabulary_bank_index()];
    LearnedWordRecord& name_record = learned_word_records_[0x10];
    copy_randomized_word(name_record, final_bank.word_4, text_api);
    const int final_random_value = std::rand();
    name_record.reinforcement =
        static_cast<std::uint32_t>(final_random_value % 0x15 + 0x1e);
    return final_random_value / 0x15;
}

void Creature::apply_goal_direction(
    std::int32_t attention_record_index,
    std::int32_t action_lobe_neuron_index,
    std::uint8_t attention_activation,
    std::uint8_t action_commitment,
    const CreatureGoalDirectionHost& world) {
    if (dead_) {
        return;
    }

    if (attention_record_index >= 0 &&
        static_cast<std::size_t>(attention_record_index) <
            attention_records_.size() &&
        attention_records_[static_cast<std::size_t>(attention_record_index)]
                .target != nullptr &&
        brain_ != nullptr) {
        brain_->add_lobe_neuron_activation(
            static_cast<std::uint32_t>(
                brain::StandardLobeIndex::stimulus_source),
            static_cast<std::uint32_t>(attention_record_index),
            attention_activation);
    }

    if (action_lobe_neuron_index < 0) {
        return;
    }

    bool motion_link_matches_attention = true;
    if (attention_record_index >= 0) {
        if (skeleton_.motion_link == nullptr) {
            motion_link_matches_attention = attention_record_index == 0;
        } else {
            motion_link_matches_attention =
                world.attention_record_index(*skeleton_.motion_link) ==
                attention_record_index;
        }

        if (!motion_link_matches_attention) {
            goal_direction_state_.attention_record_index =
                attention_record_index;
            goal_direction_state_.action_lobe_neuron_index =
                action_lobe_neuron_index;
            goal_direction_state_.commitment = action_commitment;
            goal_direction_state_.delivery_countdown = 5;
            return;
        }
    }

    if (brain_ != nullptr) {
        brain::Lobe& verb_lobe = brain_->lobe(static_cast<std::uint32_t>(
            brain::StandardLobeIndex::verb));
        const std::uint32_t neuron_index =
            static_cast<std::uint32_t>(action_lobe_neuron_index);
        if (neuron_index < verb_lobe.neuron_count_value()) {
            brain::LobeNeuron& neuron = verb_lobe.neuron(neuron_index);
            const std::uint32_t activation =
                static_cast<std::uint32_t>(neuron.activation) +
                action_commitment;
            neuron.activation = static_cast<std::uint8_t>(
                activation > 0xffu ? 0xffu : activation);
        }
    }

    goal_direction_state_.attention_record_index = -1;
}

void Creature::update_goal_direction(const CreatureGoalDirectionHost& world) {
    if (dead_ || skeleton_.bounds_reference_object() != nullptr ||
        skeleton_.sleep_indicator_active ||
        instinct_runtime_state_.dream_countdown != 0) {
        return;
    }

    const auto& drives = instinct_runtime_state_.control_state
                             .goal_direction_drive_levels;
    const std::array<std::uint8_t, 16> drive_values = {
        drives.pain,
        drives.nfp,
        drives.hunger,
        drives.coldness,
        drives.hotness,
        drives.tiredness,
        drives.sleepiness,
        drives.loneliness,
        drives.crowdedness,
        drives.fear,
        drives.boredom,
        drives.anger,
        drives.sex,
        drives.not_allocated_drive_2,
        drives.not_allocated_drive_3,
        drives.not_allocated_drive_4,
    };

    std::array<std::int32_t, 16> drive_scores{};
    for (std::size_t index = 0; index < drive_scores.size(); ++index) {
        const std::int32_t product =
            static_cast<std::int32_t>(drive_values[index]) *
            static_cast<std::int32_t>(g_goal_direction_drive_scale_factors[index]);
        // This is the native signed fixed-point product reduction.  The
        // inputs are non-negative, but retaining the signed operation keeps
        // the translation faithful if a future typed drive becomes signed.
        drive_scores[index] =
            (product + ((product >> 31) & 0xff)) >> 8;
    }

    // The native loop carries the maximum of 10 and each drive score / 4.
    // It is not a four-way sum or an average despite the decompiler's
    // temporary-variable shape.
    std::int32_t drive_baseline = 10;
    for (const std::int32_t score : drive_scores) {
        drive_baseline = std::max(drive_baseline, score / 4);
    }

    std::array<std::int32_t, kAttentionRecordCount> goal_scores{};
    for (std::size_t attention_index = 1;
         attention_index < kAttentionRecordCount; ++attention_index) {
        const AttentionRecord& attention = attention_records_[attention_index];
        if (attention.world_x == -1) {
            continue;
        }
        std::int32_t score = 0;
        for (std::size_t drive_index = 0; drive_index < drive_scores.size();
             ++drive_index) {
            score += (drive_scores[drive_index] *
                      goal_direction_weight_matrix_[attention_index][drive_index]) >> 8;
        }
        goal_scores[attention_index] = score;
    }

    std::int32_t best_score = -0x7fff;
    std::size_t best_attention_index = 0;
    for (std::size_t attention_index = 1;
         attention_index < kAttentionRecordCount; ++attention_index) {
        if (best_score < goal_scores[attention_index] &&
            attention_records_[attention_index].world_x >= 0) {
            best_score = goal_scores[attention_index];
            best_attention_index = attention_index;
        }
    }
    if (best_score < 1) {
        return;
    }

    AttentionRecord& selected = attention_records_[best_attention_index];
    const int creature_x = skeleton_.down_foot_x;
    const int creature_y = skeleton_.down_foot_y;
    if (selected.target == nullptr) {
        const std::int32_t creature_room =
            world.room_index_at(creature_x, creature_y);
        const std::int32_t goal_room =
            world.room_index_at(selected.world_x, selected.world_y);

        if (creature_room == goal_room) {
            const bool goal_is_ahead =
                (skeleton_.facing_direction == FacingDirection::west &&
                 creature_x <= selected.world_x) ||
                (skeleton_.facing_direction == FacingDirection::east &&
                 selected.world_x <= creature_x);
            const bool facing_direction_is_relevant =
                skeleton_.facing_direction == FacingDirection::west ||
                skeleton_.facing_direction == FacingDirection::east;
            if (facing_direction_is_relevant && !goal_is_ahead &&
                std::abs(selected.world_x - creature_x) < 500) {
                selected.world_x = -1;
                selected.world_y = -1;
                return;
            }
        }
    }

    if (selected.target != nullptr) {
        if (selected.target == skeleton_.motion_link) {
            return;
        }
        if (brain_ != nullptr) {
            brain::Lobe& attention_lobe = brain_->lobe(static_cast<std::uint32_t>(
                brain::StandardLobeIndex::stimulus_source));
            if (best_attention_index < attention_lobe.neuron_count_value()) {
                brain::LobeNeuron& neuron = attention_lobe.neuron(
                    static_cast<std::uint32_t>(best_attention_index));
                const std::uint32_t activation =
                    static_cast<std::uint32_t>(neuron.activation) +
                    static_cast<std::uint32_t>(drive_baseline);
                neuron.activation = static_cast<std::uint8_t>(
                    activation > 0xffu ? 0xffu : activation);
            }
        }
        return;
    }

    const objects::Object* preferred_goal_target = attention_records_[0x1b].target;
    const objects::Object* interaction_target = attention_records_[2].target;
    const std::size_t interaction_context =
        interaction_target == nullptr
            ? 0
            : (world.current_interaction_event_id(*interaction_target) != 0
                   ? 2
                   : 1);
    const std::size_t horizontal_offset =
        creature_x < selected.world_x ? 6 : 0;
    const std::size_t selected_goal_offset =
        preferred_goal_target == nullptr ? 0 : 6;
    const std::int32_t room_class =
        std::clamp(world.room_class_at(skeleton_.sound_source_x(),
                                       skeleton_.sound_source_y()),
                   0, 1);
    const world::WorldRect movement_bounds = skeleton_.movement_bounds();
    const bool goal_above = selected.world_y < movement_bounds.min_y;
    const bool goal_below = movement_bounds.max_y < selected.world_y;
    const auto& table = g_goal_direction_action_candidate_scores;
    const auto* horizontal_scores =
        &table.goal_horizontal_relation_score_by_candidate[0][0];
    const auto* selected_goal_scores =
        &table.selected_goal_presence_score_by_candidate[0][0];

    std::array<std::int32_t, 6> candidate_scores{};
    for (std::size_t candidate = 0; candidate < candidate_scores.size();
         ++candidate) {
        candidate_scores[candidate] =
            static_cast<std::int32_t>(horizontal_scores[horizontal_offset +
                                                        candidate]) +
            static_cast<std::int32_t>(
                table.interaction_context_score_by_candidate[interaction_context]
                    [candidate]) +
            static_cast<std::int32_t>(selected_goal_scores[selected_goal_offset +
                                                           candidate]) +
            static_cast<std::int32_t>(
                table.creature_room_class_score_by_candidate[room_class]
                    [candidate]);
    }
    candidate_scores[0] += goal_below ? 9 : 0;
    candidate_scores[1] += goal_above ? 9 : 0;
    candidate_scores[2] += (goal_above ? 5 : 0) + (goal_below ? 5 : 0);
    candidate_scores[4] += (goal_above ? 1 : 0) + (goal_below ? 1 : 0);
    candidate_scores[5] += (goal_above ? 1 : 0) + (goal_below ? 1 : 0);

    const auto best_candidate = static_cast<std::size_t>(
        std::distance(candidate_scores.begin(),
                      std::max_element(candidate_scores.begin(),
                                       candidate_scores.end())));
    if (candidate_scores[best_candidate] < 1) {
        return;
    }

    apply_goal_direction(
        table.attention_record_index_by_candidate[best_candidate],
        table.action_lobe_neuron_index_by_candidate[best_candidate],
        static_cast<std::uint8_t>(drive_baseline),
        static_cast<std::uint8_t>(drive_baseline), world);
}

void Creature::advance_pose_animation(
    CreaturePoseAnimationHost& world,
    objects::ObjectSoundPlaybackHost& sound_host) {
    // The native routine starts with fifteen X characters and uses a
    // bounded copy of the target pose.  Keep that exact fixed-width staging
    // buffer visible instead of treating the target as a C string.
    std::array<char, kPoseStringLength + 1> pose_buffer{};
    std::memcpy(pose_buffer.data(), kC1PoseUnknownFill,
                sizeof(kC1PoseUnknownFill));
    std::memcpy(pose_buffer.data(), skeleton_.target_pose.characters.data(),
                kPoseStringLength);
    pose_buffer[kPoseStringLength] = '\0';

    skeleton_.previous_sprite_bounds = skeleton_.sprite_bounds;

    char pose_head = pose_buffer[0];
    const bool has_motion_link = skeleton_.motion_link != nullptr;
    bool in_locomotion_mode = false;

    // '?' and '!' are the two native relative-head selectors.  They first
    // resolve to the side of the motion link, while no link resolves to X.
    if (pose_head == '?' || pose_head == '!') {
        if (!has_motion_link) {
            pose_head = 'X';
        } else if (pose_head == '?') {
            in_locomotion_mode = true;
            pose_head = skeleton_.motion_target_x < skeleton_.down_foot_x
                            ? '3'
                            : '2';
        } else {
            // Native 0043bbd7: '!' resolves the side but, unlike '?', leaves
            // EDI (the locomotion flag) clear.
            pose_head = skeleton_.down_foot_x <= skeleton_.motion_target_x
                            ? '3'
                            : '2';
        }
    }

    char transformed_tail = pose_buffer[1];
    // Native 0043bcbe: a locomotion '4' tail demoted to '5' jumps straight to
    // the stepping loop, skipping the '?' tail resolution.
    bool skip_tail_resolution = false;
    if (pose_head != 'X' && pose_head != skeleton_.current_pose.characters[0]) {
        int orientation_offset = 4;
        if (skeleton_.facing_direction == FacingDirection::east) {
            orientation_offset = 0;
        } else if (skeleton_.facing_direction == FacingDirection::west) {
            orientation_offset = 2;
        }
        if (skeleton_.current_pose.characters[1] > '3') {
            ++orientation_offset;
        }
        if (skeleton_.current_pose.characters[1] == '4' &&
            skeleton_.facing_direction == FacingDirection::north) {
            pose_head = '2';
        }

        // Ghidra's indexed address is 48 records before the defined table
        // item at 0x0045abc8.  Subtracting ASCII '0' expresses that same
        // source-table coordinate without leaking the binary address.
        const int pair_index =
            (static_cast<unsigned char>(pose_head) - '0') +
            orientation_offset * 4;
        const C1InteractionPosePair& pair =
            g_interaction_pose_pairs_ascii_0_to_5[pair_index];
        pose_head = pair.primary_pose_digit;
        transformed_tail = pair.secondary_pose_digit;

        if (in_locomotion_mode && pose_head == '1' && has_motion_link &&
            world.render_plane(*skeleton_.motion_link) <
                world.render_plane(skeleton_)) {
            pose_head = '0';
        }
        if (in_locomotion_mode && transformed_tail == '4' && has_motion_link &&
            world.render_plane(*skeleton_.motion_link) <
                world.render_plane(skeleton_)) {
            transformed_tail = '5';
            skip_tail_resolution = true;
        }
    }

    // Native writes the table pair back into the pose buffer, so the '?'
    // test at 0043bbb7 sees the transformed tail, not the requested one.
    if (!skip_tail_resolution && transformed_tail == '?') {
        if (!has_motion_link) {
            transformed_tail = '1';
        } else {
            const int horizontal_delta =
                skeleton_.motion_target_x - skeleton_.down_foot_x;
            const int absolute_horizontal_delta = std::abs(horizontal_delta);
            if (absolute_horizontal_delta < 16) {
                transformed_tail =
                    world.render_plane(*skeleton_.motion_link) <
                            world.render_plane(skeleton_)
                        ? '5'
                        : '4';
            } else if (absolute_horizontal_delta < 128) {
                const int vertical_delta =
                    skeleton_.motion_target_y -
                        (skeleton_.body != nullptr ? skeleton_.body->world_y()
                                                    : skeleton_.down_foot_y);
                int vertical_bin =
                    ((vertical_delta + ((vertical_delta >> 31) & 7)) >> 3) + 5;
                vertical_bin = std::clamp(vertical_bin, 0, 9);
                const int distance_bin =
                    (absolute_horizontal_delta +
                     ((absolute_horizontal_delta >> 31) & 0x1f)) >> 5;
                transformed_tail =
                    g_motion_pose_digit_by_vertical_bin_and_distance_bin
                        [vertical_bin][std::clamp(distance_bin, 0, 3)];
            } else {
                transformed_tail = '1';
            }
        }
    }

    const char current_pose_gait = skeleton_.current_pose.characters[1];
    pose_buffer[0] = pose_head;
    pose_buffer[1] = transformed_tail;

    // Native 0043bd4f: the applied string starts as the "XXXXXXXXXXXXXXX"
    // template (0x0045a9fc).  Slot 0 takes the resolved head; each of the
    // fourteen components is written only when it steps one digit toward
    // its target, so matched and X components stay X (keep current).
    std::array<char, kPoseStringLength + 1> applied{};
    applied.fill('X');
    applied[kPoseStringLength] = '\0';
    applied[0] = pose_head;
    for (std::size_t component = 0; component < kPoseStringLength - 1;
         ++component) {
        const char current =
            skeleton_.current_pose.characters[component + 1];
        const char target = pose_buffer[component + 1];
        if (target == 'X' || current == target) {
            continue;
        }
        ++skeleton_.pose_transition_component_count;
        applied[component + 1] = current < target ? current + 1 : current - 1;
    }

    // Native 0043bd84: slot 1 tests the requested digit.  A target of '4' or
    // above is taken immediately; below that, leaving a '4'+ state snaps to
    // '1', otherwise the stepped (or X) value stands.
    if (transformed_tail >= '4') {
        applied[1] = transformed_tail;
    } else if (current_pose_gait >= '4') {
        applied[1] = '1';
    }
    skeleton_.apply_pose_string(
        std::string_view(applied.data(), kPoseStringLength), sound_host);

    world::WorldRect dirty_bounds{};
    world::union_wrapped_world_rects(dirty_bounds, skeleton_.sprite_bounds,
                                    skeleton_.previous_sprite_bounds);
    world.queue_dirty_world_rect(dirty_bounds);
}

void Creature::update(CreatureUpdateHost& world,
                      objects::ObjectSoundPlaybackHost& sound_host) {
    // Object sound state is updated before any position or pose work, exactly
    // as in the native Creature tick entry point.
    skeleton_.update_sound(sound_host);

    if (skeleton_.uses_unbounded_world_position()) {
        update_unbounded_world_position(world, world);
    }

    advance_pose_animation(world, sound_host);

    // A completed two-digit gait entry advances the cursor by two bytes. R
    // loops the sequence back to its first entry; the following two bytes are
    // the decimal pose-table coordinate used by the executable.
    if (skeleton_.animation_cursor <
            skeleton_.animation_sequence.size() &&
        skeleton_.animation_sequence[skeleton_.animation_cursor] != '\0' &&
        skeleton_.is_pose_at_target()) {
        skeleton_.animation_cursor += 2;
        if (skeleton_.animation_cursor <
                skeleton_.animation_sequence.size() &&
            skeleton_.animation_sequence[skeleton_.animation_cursor] == 'R') {
            skeleton_.animation_cursor = 0;
        }

        if (skeleton_.animation_cursor + 1 <
                skeleton_.animation_sequence.size() &&
            skeleton_.animation_sequence[skeleton_.animation_cursor] != '\0') {
            const char pose_row =
                skeleton_.animation_sequence[skeleton_.animation_cursor];
            const char pose_column = skeleton_.animation_sequence[
                skeleton_.animation_cursor + 1];
            // Native Update @00408fe0 indexes
            // `16 * (10 * (row - 0x33) + column)` from the object base, with
            // the column keeping its ASCII bias.  That single constant folds
            // both digits' '0' bias and the pose table's own 0x120 (eighteen
            // entry) base, so a source-level array wants plain decimal.
            const int pose_table_index =
                (static_cast<int>(pose_row) - '0') * 10 +
                (static_cast<int>(pose_column) - '0');
            if (pose_table_index >= 0 &&
                static_cast<std::size_t>(pose_table_index) <
                    kPoseTableEntryCount) {
                skeleton_.set_target_pose_string(std::string_view(
                    skeleton_.pose_string_table[
                        static_cast<std::size_t>(pose_table_index)]
                        .characters.data(),
                    kPoseStringLength));
            }
        }
    }

    if (dead_) {
        if (sleep_indicator_object_ != nullptr) {
            objects::Object* indicator = sleep_indicator_object_;
            world.dispatch_sleep_indicator_event(
                *indicator, objects::ObjectEventId::event_0, indicator, 0);
            world.initialize_sleep_indicator(*indicator);
            sleep_indicator_object_ = nullptr;
        }
    } else {
        if (!skeleton_.sleep_indicator_active) {
            skeleton_.eyes_open = (std::rand() % 0x15) != 0;
        } else {
            skeleton_.eyes_open = false;
        }

        if (skeleton_.motion_link != nullptr) {
            // Native Update @00409097 pushes [this+0x7fc] -- the part chosen
            // by AIM: -- as GetPartCentre's part argument, not 0.
            skeleton_.motion_link->get_part_center(
                &skeleton_.motion_target_x, &skeleton_.motion_target_y,
                skeleton_.motion_target_part_index);
        }
        register_state_.advance_age_tick();
    }

    if (sleep_indicator_object_ != nullptr) {
        world.move_to_and_redraw(
            *sleep_indicator_object_, skeleton_.sound_source_x(),
            skeleton_.sound_source_y() - 0x14);
    }
}

void Creature::queue_event_8_after_bounds_update(
    CreatureBoundsEventHost& world) {
    world.update_movement_bounds(*this);
    world.update_anchor_and_bounds(*this);
    world.queue_event_8(*this);
}

void Creature::handle_pickup_event(
    const objects::QueuedObjectEvent& event, CreaturePickupHost& host) {
    // The native event dispatcher only calls this handler with a populated
    // source pointer. Keeping that invariant explicit avoids manufacturing a
    // decompiler-shaped null path in the recovered game policy.
    objects::Object& source = *event.source;
    objects::Object* pointer_tool = host.pointer_tool();

    if (source.has_bounds_flag(objects::Object::kIsVehicle)) {
        if (skeleton_.uses_unbounded_world_position() &&
            pointer_tool != nullptr) {
            host.queue_immediate_event(
                *pointer_tool, *pointer_tool,
                objects::ObjectEventId::event_4, 0);
        }

        skeleton_.previous_sprite_bounds = skeleton_.sprite_bounds;
        skeleton_.body->set_render_plane(
            host.vehicle_attachment_render_plane(source));
        skeleton_.set_bounds_mode(static_cast<std::uint32_t>(
                                      objects::Object::BoundsMode::vehicle_local),
                                  host.renderables());
        skeleton_.set_bounds_reference_object(&source);
        skeleton_.update_movement_bounds(host.movement_bounds_host());

        skeleton_.down_foot_y = skeleton_.movement_bounds().max_y;
        skeleton_.down_foot_x = std::clamp(
            skeleton_.down_foot_x, skeleton_.movement_bounds().min_x,
            skeleton_.movement_bounds().max_x);
        skeleton_.update_anchor_and_bounds(host.sound_host());
        skeleton_.update_limb_frames_for_pose();

        world::WorldRect dirty_bounds{};
        world::union_wrapped_world_rects(
            dirty_bounds, skeleton_.sprite_bounds,
            skeleton_.previous_sprite_bounds);
        host.queue_dirty_world_rect(dirty_bounds);
        skeleton_.boundary_correction_pending = false;
    }

    if (pointer_tool != nullptr && &source == pointer_tool &&
        host.pointer_pickup_is_privileged(source)) {
        skeleton_.set_bounds_mode(static_cast<std::uint32_t>(
                                      objects::Object::BoundsMode::unbounded_1),
                                  host.renderables());
        skeleton_.set_bounds_reference_object(nullptr);
        skeleton_.body->set_render_plane(kPointerHeldRenderPlane);
        skeleton_.update_movement_bounds(host.movement_bounds_host());
        skeleton_.update_limb_frames_for_pose();
        update_unbounded_world_position(host, host);

        host.queue_immediate_event(
            host.object_for_creature(*this), *pointer_tool,
            objects::ObjectEventId::event_5, 0);
        host.dispatch_script_event(*this, &source,
                                   static_cast<std::uint32_t>(
                                       objects::ObjectEventId::event_4),
                                   0);
        host.select_creature(*this);
    }
}

void Creature::handle_drop_event(
    const objects::QueuedObjectEvent& event, CreatureDropHost& host) {
    if (event.source == nullptr) {
        return;
    }

    objects::Object& source = *event.source;
    if (source.has_bounds_flag(objects::Object::kIsVehicle)) {
        skeleton_.previous_sprite_bounds = skeleton_.sprite_bounds;
        if (skeleton_.body != nullptr) {
            skeleton_.body->set_render_plane(skeleton_.normal_render_plane);
        }
        skeleton_.set_bounds_mode(
            static_cast<std::uint32_t>(objects::Object::BoundsMode::default_world),
            host.renderables());
        skeleton_.set_bounds_reference_object(nullptr);
        skeleton_.update_movement_bounds(host.movement_bounds_host());
        skeleton_.down_foot_y = skeleton_.movement_bounds().max_y;
        skeleton_.update_anchor_and_bounds(host.sound_host());
        skeleton_.update_limb_frames_for_pose();

        world::WorldRect dirty_bounds{};
        world::union_wrapped_world_rects(
            dirty_bounds, skeleton_.sprite_bounds,
            skeleton_.previous_sprite_bounds);
        host.queue_dirty_world_rect(dirty_bounds);
        return;
    }

    if (!skeleton_.uses_unbounded_world_position()) {
        return;
    }

    skeleton_.set_bounds_reference_object(nullptr);
    objects::Object* vehicle = host.find_topmost_vehicle_overlap(*this);
    if (vehicle == nullptr) {
        skeleton_.set_bounds_mode(
            static_cast<std::uint32_t>(objects::Object::BoundsMode::default_world),
            host.renderables());
        if (skeleton_.body != nullptr) {
            skeleton_.body->set_render_plane(skeleton_.normal_render_plane);
        }
        skeleton_.update_movement_bounds(host.movement_bounds_host());
        skeleton_.previous_sprite_bounds = skeleton_.sprite_bounds;
        skeleton_.down_foot_y = skeleton_.movement_bounds().max_y;
        skeleton_.update_anchor_and_bounds(host.sound_host());
        skeleton_.update_limb_frames_for_pose();

        world::WorldRect dirty_bounds{};
        world::union_wrapped_world_rects(
            dirty_bounds, skeleton_.sprite_bounds,
            skeleton_.previous_sprite_bounds);
        host.queue_dirty_world_rect(dirty_bounds);
    } else {
        objects::Object& creature_object = host.object_for_creature(*this);
        host.queue_immediate_event(
            creature_object, *vehicle, objects::ObjectEventId::event_4, 0);
    }

    host.dispatch_script_event(
        *this, event.source,
        static_cast<std::uint32_t>(objects::ObjectEventId::event_5), 0);

    objects::Object* pointer_tool = host.pointer_tool();
    if (pointer_tool != nullptr) {
        host.queue_immediate_event(
            *pointer_tool, *pointer_tool,
            objects::ObjectEventId::event_4, 0);
    }
}

int Creature::dispatch_script_event(
    std::uint32_t event_id, objects::Object* source, bool force_restart,
    CreatureAttentionHost& attention, CreatureScriptDispatchHost& scripts) {
    constexpr std::uint32_t kSleepIndicatorEvent = 0x29;

    if (skeleton_.sleep_indicator_active &&
        event_id != kSleepIndicatorEvent) {
        objects::Object* const indicator = sleep_indicator_object_;
        skeleton_.sleep_indicator_active = false;
        if (indicator != nullptr) {
            attention.dispatch_sleep_indicator_event(
                *indicator, objects::ObjectEventId::event_0, indicator, 0);
            attention.initialize_sleep_indicator(*indicator);
            sleep_indicator_object_ = nullptr;
        }
    }

    return scripts.execute_script_for_classifier(
        *this, source, scripts.classifier_base(*this) | event_id,
        force_restart);
}

void Creature::initialize_runtime_state(const InitializationHost& host) {
    // The first writes mirror the native reset order, but use the recovered
    // owners instead of the MFC Object/Register subobjects.
    // Native InitializeRuntimeState @00408520 opens with
    // `OR byte ptr [EDI+0x9], 0x2`: the Object bounds-flag byte the Creature
    // shares with its Skeleton base, granting pointer-tool pickup.  Bit 0x01
    // is the unrelated creature explicit-rect permission.
    skeleton_.merge_bounds_flags(
        objects::Object::kAllowPointerToolUnboundedPlacement);
    skeleton_.disable_ticking();
    sleep_indicator_object_ = nullptr;
    skeleton_.set_bounds_reference_object(nullptr);
    caos_object_pointer_ = nullptr;
    classifier_ = {AttentionObjectFamily::creature, 0};
    selected_action_id_ = 0;
    action_activation_boost_ = 0;
    active_involuntary_action_index_ = 0xff;
    goal_direction_state_ = {};
    goal_direction_state_.attention_record_index = -1;

    for (std::size_t index = 0; index < built_in_stimulus_contexts_.size();
         ++index) {
        const StimulusContext* defaults = host.default_stimulus_context(index);
        built_in_stimulus_contexts_[index] =
            defaults == nullptr ? StimulusContext{} : *defaults;
    }

    instinct_runtime_state_.control_state.reset_to_initial_state();
    instinct_runtime_state_.instinct_count = 0;
    instinct_runtime_state_.dream_countdown = 0;
    genome_sex_ = GenomeSex::male;
    genome_life_stage_ = 0;
    child_genome_source_filename_ = 0;
    gamete_genome_source_filename_ = 0;
    biochemistry_tick_ = 0;
    dead_ = false;
    death_state_ = 0;
    register_state_.set_age_ticks(0);
    goal_direction_weight_matrix_ = {};

    register_state_.history().display_name = host.localized_birthplace();
    register_state_.history().genome_moniker =
        host.format_moniker(skeleton_.genome_source_filename);
    register_state_.history().father_moniker =
        host.format_moniker(skeleton_.father_moniker);
    register_state_.history().mother_moniker =
        host.format_moniker(skeleton_.mother_moniker);
    register_state_.history().birthplace = "The birthplace";
    selection_menu_command_id_ = 0;
}

void Creature::initialize_from_genome(GenomeInitializationHost& host) {
    Genome genome(skeleton_.genome_source_filename,
                  genome_sex_,
                  genome_life_stage(),
                  &host.genome_files());

    if (genome_life_stage() == GenomeLifeStage::stage_zero &&
        brain_ != nullptr) {
        brain_->load_genome(genome);
    }

    if (!skeleton_.load_genome(genome,
                               host.skeleton_services(),
                               host.render_plane_host(),
                               host.sound_host())) {
        return;
    }

    if (biochemistry_ != nullptr) {
        biochemistry_->load_genome(
            genome, host.biochemistry_locus_host(*biochemistry_));
    }
    load_genome(genome);

    std::string_view voice_filename = "grendel.vce";
    if ((skeleton_.classifier_base() & 0xffff0000U) == 0x04010000U) {
        voice_filename = genome_sex_ == GenomeSex::male
                             ? "male.vce"
                             : "female.vce";
    }
    voice_.load_voice_file(voice_filename, host.voice_files());
}

void Creature::load_genome(Genome& genome) {
    // Native Creature::LoadGenome passes MATCH_GENOME_LOAD_STAGE to every one
    // of its FindNextMatchingGene calls -- stimulus (0), pose (3), gait (4)
    // and instinct (5).  Ignoring the stage loaded every life stage's genes
    // over each other, so a newborn ended up with the last stage's poses and
    // gaits.  (Skeleton and Body genes really are IGNORE_STAGE in the native;
    // those calls are left alone.)
    constexpr GenomeStageFilter kLoadStage =
        GenomeStageFilter::match_genome_load_stage;
    constexpr std::uint8_t kCreatureFamily =
        static_cast<std::uint8_t>(GenomeGeneFamily::creature);

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(kCreatureFamily, 0, 7, kLoadStage)) {
        const std::size_t context_index =
            normalize_modulo(genome.read_payload_byte(),
                             kBuiltInStimulusContextCount);
        StimulusContext& context = built_in_stimulus_contexts_[context_index];
        context.descriptor.attention_activation = genome.read_payload_byte();
        context.descriptor.target_neuron_index = genome.read_payload_byte();
        context.descriptor.target_lobe_activation = genome.read_payload_byte();
        context.descriptor.flags = genome.read_payload_byte();
        context.chemical_ids.first = genome.read_payload_byte();
        context.chemical_amounts.first = genome.read_payload_byte();
        context.chemical_ids.second = genome.read_payload_byte();
        context.chemical_amounts.second = genome.read_payload_byte();
        context.chemical_ids.third = genome.read_payload_byte();
        context.chemical_amounts.third = genome.read_payload_byte();
        context.chemical_ids.fourth = genome.read_payload_byte();
        context.chemical_amounts.fourth = genome.read_payload_byte();
    }

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(kCreatureFamily, 3, 7, kLoadStage)) {
        const std::size_t pose_index =
            normalize_modulo(genome.read_payload_byte(), kPoseTableEntryCount);
        PoseString& pose = skeleton_.pose_string_table[pose_index];
        for (char& character : pose.characters) {
            std::uint8_t value = genome.read_payload_byte();
            if (value < 0x20 || value > 0x5a) {
                value = static_cast<std::uint8_t>(value % 0x3b + 0x20);
            }
            const char candidate = static_cast<char>(value);
            character = std::strchr("?!X0123456789", candidate) == nullptr
                            ? 'X'
                            : candidate;
        }
        pose.nul_terminator = '\0';
    }

    genome.set_cursor(0);
    bool gait_gene =
        genome.find_next_matching_gene(kCreatureFamily, 4, 7, kLoadStage);
    while (gait_gene) {
        const std::size_t gait_index =
            normalize_modulo(genome.read_payload_byte(), kGaitTableEntryCount);
        auto& sequence = skeleton_.gait_animation_table[gait_index];
        sequence.fill('\0');
        std::size_t byte_index = 0;
        while (byte_index < 8) {
            const std::uint8_t value = genome.read_payload_byte();
            if (value == 0) {
                break;
            }
            append_gait_byte(sequence, byte_index, normalize_modulo(value, 100));
            ++byte_index;
        }
        sequence[byte_index * 2] = 'R';
        sequence[byte_index * 2 + 1] = '\0';
        gait_gene =
            genome.find_next_matching_gene(kCreatureFamily, 4, 7, kLoadStage);
    }

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(kCreatureFamily, 5, 7, kLoadStage)) {
        if (instinct_runtime_state_.instinct_count >= kInstinctCapacity) {
            continue;
        }
        brain::Instinct& instinct =
            instinct_runtime_state_.instincts[instinct_runtime_state_.instinct_count];
        for (std::size_t index = 0; index < instinct.replay_lobe_indices.size();
             ++index) {
            const std::uint8_t lobe = genome.read_payload_byte();
            instinct.replay_lobe_indices[index] = lobe < 8 ? lobe : lobe & 7;
            const std::uint32_t neuron_count =
                brain_ == nullptr
                    ? 0
                    : brain_->lobe(instinct.replay_lobe_indices[index])
                          .neuron_count_value();
            instinct.replay_neuron_indices[index] =
                normalize_modulo(genome.read_payload_byte(), neuron_count);
        }
        instinct.decision_lobe_neuron_index =
            genome.read_payload_byte() & 0x0f;
        instinct.dream_chemical_index = genome.read_payload_byte();
        instinct.dream_chemical_concentration = genome.read_payload_byte();
        instinct.dream_step_index = 0;
        ++instinct_runtime_state_.instinct_count;
    }
}

void Creature::select_walk_gait() {
    std::uint8_t best_level = 0;
    std::size_t best_gait = 0;
    const auto& gait_levels = instinct_runtime_state_.control_state.gait_locus_levels;

    for (std::size_t gait = 1; gait < kGaitTableEntryCount - 1; ++gait) {
        const auto& sequence = skeleton_.gait_animation_table[gait];
        if (sequence[0] == '\0' || gait_levels[gait - 1] <= best_level) {
            continue;
        }
        best_gait = gait;
        best_level = gait_levels[gait - 1];
    }
    if (skeleton_.motion_link != nullptr &&
        gait_levels[kGaitTableEntryCount - 1] > best_level) {
        best_gait = kGaitTableEntryCount - 1;
    }

    const auto& selected = skeleton_.gait_animation_table[best_gait];
    std::copy(selected.begin(), selected.end(), skeleton_.animation_sequence.begin());
    skeleton_.animation_cursor = 0;

    if (selected[0] >= '0' && selected[0] <= '9' &&
        selected[1] >= '0' && selected[1] <= '9') {
        const std::size_t pose_index =
            static_cast<std::size_t>(selected[0] - '0') * 10 +
            static_cast<std::size_t>(selected[1] - '0');
        if (pose_index < kPoseTableEntryCount) {
            skeleton_.set_target_pose_string(std::string_view(
                skeleton_.pose_string_table[pose_index].characters.data(),
                kPoseStringLength));
        }
    }
}

namespace {

bool image_bounds(const BodyPart& part, world::WorldRect& out_bounds) {
    if (part.gallery() == nullptr ||
        part.current_image_index() >= part.gallery()->image_count) {
        out_bounds = {};
        return false;
    }
    const display::Image& image =
        part.gallery()->images[part.current_image_index()];
    out_bounds = {part.world_x(), part.world_y(),
                  part.world_x() + image.width(),
                  part.world_y() + image.height()};
    return true;
}

bool contains(const world::WorldRect& bounds, int x, int y) {
    return bounds.min_x <= x && x < bounds.max_x &&
           bounds.min_y <= y && y < bounds.max_y;
}

} // namespace

objects::ObjectEventId Creature::click_event_id_at_world_position(
    int world_x, int world_y) const {
    if (skeleton_.limb_chain_heads[0] != nullptr) {
        world::WorldRect head_bounds;
        if (image_bounds(*skeleton_.limb_chain_heads[0], head_bounds) &&
            contains(head_bounds, world_x, world_y)) {
            return objects::ObjectEventId::event_0;
        }
    }

    if (skeleton_.body != nullptr) {
        world::WorldRect body_bounds;
        if (image_bounds(*skeleton_.body, body_bounds) &&
            contains(body_bounds, world_x, world_y)) {
            return objects::ObjectEventId::event_2;
        }
    }
    return objects::ObjectEventId::no_event;
}

void Creature::get_part_center(int* out_x, int* out_y, int part_index) const {
    if (out_x == nullptr || out_y == nullptr) {
        return;
    }
    if (part_index == 1 && skeleton_.body != nullptr) {
        world::WorldRect bounds;
        if (image_bounds(*skeleton_.body, bounds)) {
            *out_x = bounds.min_x + (bounds.max_x - bounds.min_x) / 2;
            *out_y = bounds.min_y + (bounds.max_y - bounds.min_y) / 2;
            return;
        }
    }
    *out_x = skeleton_.limb_chain_end_x[0];
    *out_y = skeleton_.limb_chain_end_y[0];
}

bool Creature::can_perceive(const objects::Object& target,
                            const CreaturePerceptionHost& world) const {
    if (world.is_this_creature(target, *this) ||
        world.has_bounds_flag(target, 0x10)) {
        return false;
    }

    world::WorldRect target_bounds;
    if (!world.read_bounds(target, target_bounds) ||
        target_bounds.min_x >= target_bounds.max_x ||
        target_bounds.min_y >= target_bounds.max_y) {
        return false;
    }

    const int foot_x = skeleton_.down_foot_x;
    if (skeleton_.facing_direction == FacingDirection::west &&
        foot_x < target_bounds.min_x) {
        return false;
    }
    if (skeleton_.facing_direction == FacingDirection::east &&
        target_bounds.max_x < foot_x) {
        return false;
    }

    world::WorldRect perception = skeleton_.movement_bounds();
    if (skeleton_.bounds_reference_object() != nullptr) {
        if (world.is_map_room_reference(*skeleton_.bounds_reference_object())) {
            return false;
        }
        world.nearest_map_room_bounds(skeleton_.down_foot_x,
                                      skeleton_.down_foot_y, perception);
        if (!contains(perception, skeleton_.down_foot_x,
                      skeleton_.down_foot_y)) {
            perception = {0, 0, world::kWorldWidth, world::kWorldHeight};
        }
    }

    const bool overlaps = perception.min_x < target_bounds.max_x &&
                          target_bounds.min_x < perception.max_x &&
                          perception.min_y < target_bounds.max_y &&
                          target_bounds.min_y < perception.max_y;
    if (!overlaps) {
        return false;
    }

    const int target_center_x =
        target_bounds.min_x + (target_bounds.max_x - target_bounds.min_x) / 2;
    return std::abs(target_center_x - foot_x) < 500;
}

void Creature::update_perception(
    const CreaturePerceptionHost& world,
    const StimulusSourceHost& source_host,
    BuiltInStimulusDebugHost* debug_host) {
    if (dead_) {
        return;
    }

    auto deliver_visibility_stimulus = [&](objects::Object* target,
                                           std::uint32_t magnitude) {
        if (target == nullptr) {
            return;
        }
        trigger_built_in_stimulus(8, target, magnitude, source_host,
                                  debug_host);
    };

    if (!skeleton_.sleep_indicator_active) {
    // Sound movement is checked before registry scanning.  The native loop
    // deliberately retains the previous source coordinate in each attention
    // record, rather than using the target's current visual bounds.
    for (std::size_t index = 1; index < attention_records_.size(); ++index) {
        AttentionRecord& record = attention_records_[index];
        if (record.target == nullptr) {
            continue;
        }

        const int current_x = source_host.sound_source_x(*record.target);
        const int previous_x = record.world_x;
        const int delta = previous_x - current_x;
        if (previous_x != 0 && std::abs(delta) > 0x20 &&
            can_perceive(*record.target, world)) {
            const int creature_x = skeleton_.sound_source_x();
            const int previous_distance = std::abs(previous_x - creature_x);
            const int current_distance = std::abs(current_x - creature_x);
            // Stimulus 5 means the sound moved toward the creature; stimulus
            // 6 means it moved away.  The choice is based on distance to the
            // creature, not on the sign of the world coordinate delta.
            const std::uint32_t movement_stimulus =
                current_distance < previous_distance ? 5u : 6u;
            trigger_built_in_stimulus(
                movement_stimulus, record.target,
                static_cast<std::uint32_t>(std::abs(delta)), source_host,
                debug_host);
        }
        record.world_x = current_x;
        record.world_y = source_host.sound_source_y(*record.target);
    }

    // Every non-scenery object is offered to the fixed attention-slot table.
    // Registry ownership and mutation rules remain in the world adapter.
    std::size_t object_index = 0;
    std::size_t object_count = world.non_scenery_object_count();
    while (object_index < object_count) {
        objects::Object* candidate = world.non_scenery_object_at(object_index);
        if (candidate != nullptr) {
            const std::uint32_t attention_index = get_attention_record_index(
                source_host.classify(*candidate));
            AttentionRecord& record = attention_records_[attention_index];
            if (record.target == nullptr) {
                if (can_perceive(*candidate, world)) {
                    const std::uint32_t magnitude = record.stimulus_seen
                                                       ? 0x50u
                                                       : 0xb4u;
                    record.stimulus_seen = true;
                    deliver_visibility_stimulus(candidate, magnitude);
                    record.visible = true;
                }
            } else if (record.target != candidate && !record.visible &&
                       record.target != skeleton_.motion_link &&
                       can_perceive(*candidate, world)) {
                const std::uint32_t magnitude = record.stimulus_seen
                                                   ? 0x50u
                                                   : 0xb4u;
                record.stimulus_seen = true;
                deliver_visibility_stimulus(candidate, magnitude);
                record.visible = true;
                if (brain_ != nullptr) {
                    brain::Lobe& attention_lobe = brain_->lobe(
                        static_cast<std::uint32_t>(
                            brain::StandardLobeIndex::attention));
                    if (attention_index < attention_lobe.neuron_count_value()) {
                        brain::LobeNeuron& neuron = attention_lobe.neuron(
                            attention_index);
                        neuron.firing_strength = 0;
                        neuron.activation = 0;
                    }
                }
            }
        }
        ++object_index;
        object_count = world.non_scenery_object_count();
    }

    // A target that leaves/re-enters perception gets the smaller return-to-
    // view stimulus. This is distinct from first-seen novelty above.
    for (std::size_t index = 1; index < attention_records_.size(); ++index) {
        AttentionRecord& record = attention_records_[index];
        if (record.target == nullptr) {
            continue;
        }
        if (!record.visible) {
            if (can_perceive(*record.target, world)) {
                const std::uint32_t magnitude = record.stimulus_seen
                                                   ? 0x1au
                                                   : 0x3cu;
                record.stimulus_seen = true;
                deliver_visibility_stimulus(record.target, magnitude);
                record.visible = true;
            }
        } else if (!can_perceive(*record.target, world)) {
            record.visible = false;
        }
    }
    }

    if (brain_ != nullptr && skeleton_.motion_link != nullptr) {
        const CreatureMotionLinkFacts facts = world.motion_link_facts(
            *skeleton_.motion_link, *this);
        if (facts.is_creature) {
            brain::Lobe& sensory_lobe = brain_->lobe(
                static_cast<std::uint32_t>(brain::StandardLobeIndex::general_sensory));
            const auto set_activation = [&](std::size_t neuron_index,
                                            std::uint8_t activation) {
                if (neuron_index < sensory_lobe.neuron_count_value()) {
                    sensory_lobe.neuron(static_cast<std::uint32_t>(neuron_index))
                        .activation = activation;
                }
            };
            set_activation(0x0f, 0xff);
            set_activation(0x11, facts.link_is_my_parent ? 0xff : 0);
            set_activation(0x12, facts.link_is_my_child ? 0xff : 0);
            set_activation(0x10, facts.shares_a_parent ? 0xff : 0);
            set_activation(0x13, facts.is_opposite_sex ? 0xff : 0);
        }
    }

    if (brain_ != nullptr) {
        brain::Lobe& sensory_lobe = brain_->lobe(
            static_cast<std::uint32_t>(brain::StandardLobeIndex::general_sensory));
        const world::WorldRect bounds = skeleton_.movement_bounds();
        const int limb_x = skeleton_.limb_chain_end_x[0];
        int edge_distance = -1;
        if (skeleton_.facing_direction == FacingDirection::west &&
            limb_x - bounds.min_x <= 0x4f) {
            edge_distance = bounds.min_x - limb_x;
        } else if (skeleton_.facing_direction == FacingDirection::east &&
                   bounds.max_x - limb_x <= 0x4f) {
            edge_distance = limb_x - bounds.max_x;
        }
        if (edge_distance >= 0 && sensory_lobe.neuron_count_value() > 3) {
            const int activation = std::clamp(edge_distance * 2 + 200, 0, 0xff);
            sensory_lobe.neuron(3).activation =
                static_cast<std::uint8_t>(activation);
        }
        if (skeleton_.bounds_reference_object() != nullptr &&
            sensory_lobe.neuron_count_value() > 4) {
            sensory_lobe.neuron(4).activation = 0x7f;
        }
    }
}

void Creature::update_drive_threshold_state(
    const std::array<std::int32_t, 16>& lower_thresholds,
    const std::array<std::int32_t, 16>& upper_thresholds) {
    if (dead_) {
        return;
    }

    skeleton_.drive_threshold_state =
        static_cast<std::uint8_t>(DriveThresholdState::all_at_or_below_lower);
    bool found_above_lower = false;
    bool found_above_upper = false;
    const auto& drives = instinct_runtime_state_.control_state.goal_direction_drive_levels;
    const std::array<std::uint8_t, 16> values = {
        drives.pain, drives.nfp, drives.hunger, drives.coldness,
        drives.hotness, drives.tiredness, drives.sleepiness, drives.loneliness,
        drives.crowdedness, drives.fear, drives.boredom, drives.anger,
        drives.sex, drives.not_allocated_drive_2,
        drives.not_allocated_drive_3, drives.not_allocated_drive_4};
    for (std::size_t index = 0; index < values.size(); ++index) {
        found_above_lower = found_above_lower ||
                            lower_thresholds[index] < values[index];
        found_above_upper = found_above_upper ||
                            upper_thresholds[index] < values[index];
    }
    if (found_above_upper) {
        skeleton_.drive_threshold_state =
            static_cast<std::uint8_t>(DriveThresholdState::some_above_upper);
    } else if (found_above_lower) {
        skeleton_.drive_threshold_state =
            static_cast<std::uint8_t>(DriveThresholdState::some_above_lower);
    }
    instinct_runtime_state_.control_state.always_on_signal = 0xff;
    skeleton_.pose_transition_component_count = 0;
    instinct_runtime_state_.control_state.asleep_signal =
        skeleton_.sleep_indicator_active ? 0xff : 0;
}

void Creature::apply_stimulus(
    objects::Object* source_object,
    Creature* source_creature,
    StimulusDescriptor descriptor,
    StimulusChemicalIds chemical_ids,
    StimulusChemicalAmounts chemical_amounts,
    std::uint32_t magnitude,
    const StimulusSourceHost& source_host,
    common::DebugLogHost* log_host) {
    if (dead_ || source_object == nullptr) {
        return;
    }

    const std::uint8_t original_flags = descriptor.flags;
    if ((original_flags & 0x01) != 0) {
        descriptor.attention_activation =
            scaled_byte(descriptor.attention_activation, magnitude);
        descriptor.target_lobe_activation =
            scaled_byte(descriptor.target_lobe_activation, magnitude);
        chemical_amounts.first = scaled_byte(chemical_amounts.first, magnitude);
        chemical_amounts.second = scaled_byte(chemical_amounts.second, magnitude);
        chemical_amounts.third = scaled_byte(chemical_amounts.third, magnitude);
        chemical_amounts.fourth = scaled_byte(chemical_amounts.fourth, magnitude);
    }
    if ((original_flags & 0x02) != 0) {
        descriptor.target_neuron_index = static_cast<std::uint8_t>(
            descriptor.target_neuron_index + static_cast<std::int8_t>(magnitude));
    }
    if (control_state().asleep_signal != 0 && (original_flags & 0x04) == 0) {
        return;
    }
    if (control_state().asleep_signal != 0) {
        descriptor.target_lobe_activation /= 2;
        descriptor.attention_activation /= 2;
    }

    const AttentionClassifier classifier = source_host.classify(*source_object);
    const std::uint32_t attention_index = get_attention_record_index(classifier);
    AttentionRecord& record = attention_records_[attention_index];
    record.target = source_host.is_this_creature(*source_object, *this)
                        ? nullptr
                        : source_object;
    record.target_neuron_index = descriptor.target_neuron_index;
    record.lobe_activation = descriptor.target_lobe_activation;
    record.world_x = source_host.sound_source_x(*source_object);
    record.world_y = source_host.sound_source_y(*source_object);
    record.visible = true;
    record.stimulus_seen = true;

    if (brain_ != nullptr) {
        // The Stimulus-source lobe is indexed by the *attention record*, not by
        // the descriptor's target neuron: `Creature::ApplyStimulus @ 0x0040b8a0`
        // writes `lobes[2].neurons[attention_record_index]`.  Indexing it by
        // `target_neuron_index` sent every write out of range -- that field is
        // 0xff on the built-in stimuli -- so the lobe was never fed, and the
        // Attention lobe that reads it almost never fired.
        brain_->add_lobe_neuron_activation(
            static_cast<std::uint32_t>(brain::StandardLobeIndex::stimulus_source),
            attention_index, descriptor.attention_activation);
        // Native gates the General-sensory write on the attention record's
        // target -- which is null for a self-stimulus -- against the skeleton's
        // motion link, and contributes the target lobe activation.
        if (record.target == skeleton_.motion_link &&
            descriptor.target_neuron_index < 0x20) {
            brain_->add_lobe_neuron_activation(
                static_cast<std::uint32_t>(brain::StandardLobeIndex::general_sensory),
                descriptor.target_neuron_index,
                descriptor.target_lobe_activation);
        }
    }

    if (biochemistry_ != nullptr) {
        biochemistry_->add_chemical_moles(chemical_ids.first,
                                          chemical_amounts.first, log_host);
        biochemistry_->add_chemical_moles(chemical_ids.second,
                                          chemical_amounts.second, log_host);
        biochemistry_->add_chemical_moles(chemical_ids.third,
                                          chemical_amounts.third, log_host);
        biochemistry_->add_chemical_moles(chemical_ids.fourth,
                                          chemical_amounts.fourth, log_host);
    }

    const bool is_self = source_creature == this ||
                         source_host.is_this_creature(*source_object, *this);
    if (!is_self && attention_index < goal_direction_weight_matrix_.size()) {
        const std::array<std::uint8_t, 4> ids = {
            chemical_ids.first, chemical_ids.second, chemical_ids.third,
            chemical_ids.fourth};
        const std::array<std::uint8_t, 4> amounts = {
            chemical_amounts.first, chemical_amounts.second,
            chemical_amounts.third, chemical_amounts.fourth};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            if (ids[index] < 0x21 || ids[index] > 0x30) {
                continue;
            }
            const std::size_t column = ids[index] - 0x21;
            const std::int32_t old_weight =
                goal_direction_weight_matrix_[attention_index][column];
            goal_direction_weight_matrix_[attention_index][column] =
                (static_cast<std::int32_t>(amounts[index]) + old_weight * 4) / 5;
        }
    }
}

void Creature::trigger_built_in_stimulus(
    std::uint32_t stimulus_id,
    objects::Object* target,
    std::uint32_t magnitude,
    const StimulusSourceHost& source_host,
    BuiltInStimulusDebugHost* debug_host,
    common::DebugLogHost* log_host) {
    // The native caller supplies a valid index into the fixed 36-entry table;
    // retaining unchecked indexing preserves that established contract.
    StimulusContext& context = built_in_stimulus_contexts_[stimulus_id];
    context.target = target;
    context.source_creature = this;

    if (debug_host != nullptr && debug_host->debug_console_visible() &&
        debug_host->is_selected_creature(*this)) {
        debug_host->log_built_in_stimulus(
            *this, target, stimulus_id, magnitude, context);
    }

    apply_stimulus(context.target, context.source_creature,
                   context.descriptor, context.chemical_ids,
                   context.chemical_amounts, magnitude, source_host,
                   log_host);
}

void Creature::notify_dependents_on_removal(
    objects::ObjectRegistryHost& non_scenery_objects,
    objects::ObjectImmediateEventQueueHost& immediate_events,
    const CreatureObjectIdentityHost& object_identity,
    const StimulusSourceHost& source_host,
    BuiltInStimulusDebugHost* debug_host,
    common::DebugLogHost* log_host) {
    objects::Object& creature_object = object_identity.object_for_creature(*this);
    std::size_t object_count = non_scenery_objects.object_count();
    std::size_t dependent_count = 0;

    for (std::size_t index = 0; index < object_count; ++index) {
        objects::Object* candidate = non_scenery_objects.object_at(index);
        if (candidate == nullptr ||
            candidate->bounds_reference_object() != &creature_object) {
            continue;
        }

        ++dependent_count;
        immediate_events.queue_immediate_event(
            creature_object, *candidate, objects::ObjectEventId::event_5, 0);

        // The native registry count is reread after queue submission because
        // object callbacks can mutate the registry while this pass runs.
        object_count = non_scenery_objects.object_count();
    }

    if (dependent_count != 0) {
        return;
    }

    trigger_built_in_stimulus(0, &creature_object, 0, source_host,
                              debug_host, log_host);
}

void Creature::update_unbounded_world_position(
    CreatureUnboundedWorldPositionHost& world,
    objects::ObjectImmediateEventQueueHost& immediate_events) {
    UnboundedWorldPositionInput input;
    const bool had_pending_input =
        world.read_view_input_and_clear_pending_flag(input);
    objects::Object& creature_object = world.object_for_creature(*this);

    if (had_pending_input) {
        immediate_events.queue_immediate_event(
            creature_object, creature_object,
            objects::ObjectEventId::event_5, 0);
    }

    int world_x = input.viewport_left + input.mouse_client_x;
    if (world_x >= world::kWorldWidth) {
        world_x -= world::kWorldWidth;
    }
    const int world_y = input.viewport_top + input.mouse_client_y +
                        creature_object.current_visual_height();
    world.move_to_and_redraw(creature_object, world_x, world_y);
    world.update_pointer_tool_unbounded_position_and_redraw();
}

void Creature::process_insemination(CreatureInseminationHost& host) {
    Creature* recipient = host.recipient_for_insemination(*this);
    if (recipient == nullptr) {
        return;
    }

    const GenomeFilenameId paternal_source_filename =
        gamete_genome_source_filename_;
    if (host.debug_console_visible() && host.is_selected_creature(*this)) {
        host.log_insemination(InseminationLogEvent::gamete_available,
                              *this, recipient, paternal_source_filename);
    }

    if (host.debug_console_visible() && host.is_selected_creature(*recipient)) {
        host.log_insemination(InseminationLogEvent::sperm_accepted,
                              *this, recipient, paternal_source_filename);
    }

    const GenomeFilenameId maternal_source_filename =
        recipient->gamete_genome_source_filename_;
    if (maternal_source_filename != 0 && paternal_source_filename != 0 &&
        recipient->child_genome_source_filename_ == 0) {
        const std::uint32_t random_value =
            static_cast<std::uint32_t>(std::rand());
        std::uint32_t normalized_probability = random_value & 0x800000ffu;
        if (static_cast<std::int32_t>(normalized_probability) < 0) {
            normalized_probability =
                ((normalized_probability - 1u) | 0xffffff00u) + 1u;
        }
        if (static_cast<std::int32_t>(normalized_probability) <
            static_cast<std::int32_t>(
                recipient->instinct_runtime_state_.control_state
                    .conception_probability)) {
            recipient->child_genome_source_filename_ =
                host.generate_offspring_genome_file(
                    maternal_source_filename, paternal_source_filename);
        }
        if (host.debug_console_visible() &&
            host.is_selected_creature(*recipient)) {
            host.log_insemination(InseminationLogEvent::fertilization_attempted,
                                  *this, recipient, paternal_source_filename);
        }
    }

    gamete_genome_source_filename_ = 0;
}

std::string Creature::format_status_for_external_query(
    const world::MapRoomTable& rooms, const StatusStrings& strings) const {
    // Ten fields, '|' between them and none after the last.  Field order and
    // every derivation below is Creature::FormatStatusForExternalQuery @
    // 0x0040e520.
    std::string out;
    const auto field = [&out](std::string_view text) {
        if (!out.empty()) {
            out.push_back('|');
        }
        out.append(text);
    };

    field(register_state_.history().display_name);
    field(register_state_.history().genome_moniker);
    field(genome_sex_ == GenomeSex::male ? "1" : "2");

    // Age is printed as hours:minutes with the minutes zero-padded below ten.
    const std::uint32_t age = register_state_.age_ticks();
    const std::uint32_t hours = age / 36000u;
    const std::uint32_t minutes = age / 600u - hours * 60u;
    std::string age_text = std::to_string(hours) + ":";
    if (minutes < 10u) {
        age_text.push_back('0');
    }
    age_text += std::to_string(minutes);
    field(age_text);

    // Pregnancy: males report the "N/A" string, non-pregnant females "No",
    // and a gestating female the 1..8 stage her gestation chemical falls in.
    if (genome_sex_ == GenomeSex::male) {
        field(strings.male);
    } else if (child_genome_source_filename_ == 0) {
        field(strings.not_pregnant);
    } else {
        // The native walks thresholds 0, 32, ... 224 and keeps the last stage
        // whose threshold is strictly below the concentration.  A zero
        // concentration matches nothing and leaves the shared scratch buffer
        // holding the previous field -- a native bug this port does not
        // reproduce; it reports stage 0 instead.
        std::uint32_t stage = 0;
        const std::uint8_t gestation =
            biochemistry_ == nullptr
                ? 0
                : biochemistry_->chemical_states()[0x42].concentration;
        for (std::uint32_t index = 0, threshold = 0; threshold < 0x100u;
             ++index, threshold += 0x20u) {
            if (threshold < gestation) {
                stage = index + 1;
            }
        }
        field(std::to_string(stage));
    }

    // Health is the life-force chemical as a percentage; zero or dead reports
    // the "Dead" string instead of a number.
    const std::uint32_t health =
        biochemistry_ == nullptr
            ? 0
            : (static_cast<std::uint32_t>(
                   biochemistry_->chemical_states()[0x3b].concentration) *
               100u) /
                  0xffu;
    if (health == 0 || dead_) {
        field(strings.dead);
    } else {
        field(std::to_string(health) + "%");
    }

    // Sickness scans two antigen/antibody chemical ranges -- 248..255 and
    // 232..234 -- for any concentration above 0x32.  Death overrides both.
    std::string sickness = strings.healthy;
    if (biochemistry_ != nullptr) {
        const auto& chemicals = biochemistry_->chemical_states();
        const auto any_above = [&chemicals](std::size_t first,
                                            std::size_t last) {
            for (std::size_t index = first; index <= last; ++index) {
                if (chemicals[index].concentration > 0x32) {
                    return true;
                }
            }
            return false;
        };
        if (any_above(248, 255) || any_above(232, 234)) {
            sickness = strings.sick;
        }
    }
    if (dead_) {
        sickness = strings.dead;
    }
    field(sickness);

    // The room is the FIRST whose horizontal span contains the down foot; the
    // native's second test compares the distance to that room's bottom edge
    // against the distance to 9999, which admits any sane foot position.
    const int foot_x = skeleton_.down_foot_x;
    const int foot_y = skeleton_.down_foot_y;
    int room_index = -1;
    for (std::size_t index = 0; index < rooms.room_count; ++index) {
        const world::MapRoom& room = rooms.rooms[index];
        if (room.bounds.left <= foot_x && foot_x <= room.bounds.right &&
            std::abs(foot_y - room.bounds.bottom) < std::abs(foot_y - 9999)) {
            room_index = static_cast<int>(index);
            break;
        }
    }
    field(std::to_string(room_index));
    field(std::to_string(foot_x));
    field(std::to_string(foot_y));
    return out;
}

void Creature::append_default_response_prefix(
    char* phrase_buffer, const MultibyteTextApi& text_api) {
    text_api.append_string(phrase_buffer, 0x50,
                           learned_word_records_[0x10].response_word);
    text_api.append_string(phrase_buffer, 0x50, " ");
}

void Creature::handle_queued_event_slot5(
    const objects::QueuedObjectEvent& event,
    CreatureScriptEventHost& event_host) {
    if (!dead_) {
        event_host.dispatch_script_event(*this, event.source, 1, 1);
    }
}

void Creature::handle_queued_event_slot6(
    const objects::QueuedObjectEvent& event,
    CreatureScriptEventHost& event_host) {
    if (!dead_) {
        event_host.dispatch_script_event(*this, event.source, 2, 1);
    }
}

void Creature::handle_queued_event_slot7(
    const objects::QueuedObjectEvent& event,
    CreatureScriptEventHost& event_host) {
    if (!dead_) {
        event_host.dispatch_script_event(*this, event.source, 0, 1);
    }
}

std::uint32_t Creature::first_seen_stimulus_magnitude(
    const objects::Object& object,
    const StimulusSourceHost& source_host) {
    const std::uint32_t attention_index = get_attention_record_index(
        source_host.classify(object));
    AttentionRecord& record = attention_records_[attention_index];
    if (!record.stimulus_seen) {
        record.stimulus_seen = true;
        return 0xb4;
    }
    return 0x50;
}

bool Creature::references_object(const objects::Object* candidate) const {
    if (sleep_indicator_object_ == candidate ||
        skeleton_.motion_link == candidate ||
        skeleton_.bounds_reference_object() == candidate ||
        caos_object_pointer_ == candidate) {
        return true;
    }

    for (const AttentionRecord& record : attention_records_) {
        if (record.target == candidate) {
            return true;
        }
    }
    return false;
}

void Creature::clear_references_to_object(
    const objects::Object* candidate) {
    if (skeleton_.motion_link == candidate) {
        skeleton_.motion_link = nullptr;
    }
    if (skeleton_.bounds_reference_object() == candidate) {
        skeleton_.set_bounds_reference_object(nullptr);
    }
    if (caos_object_pointer_ == candidate) {
        caos_object_pointer_ = nullptr;
    }
    for (AttentionRecord& record : attention_records_) {
        if (record.target == candidate) {
            record.target = nullptr;
        }
    }
}

namespace {

void load_creature_text(std::string value, char* destination,
                        std::size_t capacity) {
    std::fill(destination, destination + capacity, '\0');
    const std::size_t copy_count =
        std::min(value.size(), capacity == 0 ? 0 : capacity - 1);
    std::copy_n(value.data(), copy_count, destination);
}

std::string creature_text(const char* source, std::size_t capacity) {
    std::size_t length = 0;
    while (length < capacity && source[length] != '\0') {
        ++length;
    }
    return {source, length};
}

class CreatureVoiceArchive final : public VoiceArchive {
public:
    explicit CreatureVoiceArchive(CreatureArchive& archive)
        : archive_(archive) {}

    bool is_loading() const override { return archive_.is_loading(); }
    std::uint32_t read_uint32() override { return archive_.read_uint32(); }
    void write_uint32(std::uint32_t value) override {
        archive_.write_uint32(value);
    }

private:
    CreatureArchive& archive_;
};

class CreatureStringArchive final : public archive::StringArchive {
public:
    explicit CreatureStringArchive(CreatureArchive& archive)
        : archive_(archive) {}

    bool is_loading() const override { return archive_.is_loading(); }
    std::string read_string() override { return archive_.read_string(); }
    void write_string(std::string_view value) override {
        archive_.write_string(value);
    }

private:
    CreatureArchive& archive_;
};

} // namespace

void Creature::update_environment_and_life_stage(
    CreatureEnvironmentHost& environment,
    common::DebugLogHost* log_host) {
    constexpr std::uint8_t kTerminalLifeStage = 7;
    constexpr int kTemperatureScale = 0x55;
    constexpr std::uint8_t kOvulationStart = 0xa0;
    constexpr std::uint8_t kOvulationStop = 0x50;
    constexpr int kCrowdednessDistanceLimit = 200;
    constexpr std::uint8_t kCrowdednessMaximum = 0x32;

    if (death_state_ != 0) {
        return;
    }

    const CreatureEnvironmentSample local_environment = environment.sample_at(
        skeleton_.sound_source_x(), skeleton_.sound_source_y());
    control_state().air_coldness_signal = 0;
    control_state().air_hotness_signal = 0;
    if (local_environment.uses_ambient_temperature) {
        const int scaled_temperature =
            local_environment.temperature_delta_raw * kTemperatureScale;
        if (scaled_temperature < 0) {
            control_state().air_coldness_signal =
                static_cast<std::uint8_t>(-scaled_temperature);
        } else if (scaled_temperature > 0) {
            control_state().air_hotness_signal =
                static_cast<std::uint8_t>(scaled_temperature);
        }
    }

    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        common::debug_log(*log_host, 0x10,
                          "loc_coldness=%d loc_hotness=%d\\n",
                          control_state().air_coldness_signal,
                          control_state().air_hotness_signal);
    }

    control_state().ambient_light_signal =
        local_environment.ambient_light_level;
    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        common::debug_log(*log_host, 0x10, "loc_lightlevel=%d\\n",
                          control_state().ambient_light_signal);
    }

    const std::size_t attention_index =
        classifier_to_attention_index(classifier_);
    const objects::Object* attention_target =
        attention_index < attention_records_.size()
            ? attention_records_[attention_index].target
            : nullptr;
    if (attention_target == nullptr) {
        control_state().crowdedness_signal = 0;
    } else {
        const int distance = std::abs(
            environment.sound_source_x(*attention_target) -
            skeleton_.sound_source_x());
        // The original stores the negative short-distance result in the
        // one-byte signal and saturates only distances above 200.
        control_state().crowdedness_signal =
            distance > kCrowdednessDistanceLimit
                ? kCrowdednessMaximum
                : static_cast<std::uint8_t>(-distance - 6);
    }
    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        common::debug_log(*log_host, 0x10, "loc_crowdedness=%d\\n",
                          control_state().crowdedness_signal);
    }

    const std::uint8_t ovulation_signal = control_state().ovulation_signal;
    if (gamete_genome_source_filename_ == 0) {
        if (ovulation_signal > kOvulationStart) {
            gamete_genome_source_filename_ = skeleton_.genome_source_filename;
        }
    } else if (ovulation_signal < kOvulationStop) {
        gamete_genome_source_filename_ = 0;
    }
    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        common::debug_log(*log_host, 0x10, "loc_ovulate=%d\\n",
                          ovulation_signal);
    }

    control_state().fertility_signal =
        gamete_genome_source_filename_ != 0 ? 0xff : 0;
    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        common::debug_log(*log_host, 0x10, "loc_fertile=%d\\n",
                          control_state().fertility_signal);
    }

    control_state().pregnancy_signal =
        child_genome_source_filename_ != 0 ? 0xff : 0;
    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        common::debug_log(*log_host, 0x10, "loc_pregnant=%d\\n",
                          control_state().pregnancy_signal);
    }

    const std::uint8_t life_stage = genome_life_stage_;
    if (life_stage < kTerminalLifeStage &&
        life_stage < control_state().life_stage_advance_signals.size() &&
        control_state().life_stage_advance_signals[life_stage] != 0) {
        genome_life_stage_ = static_cast<std::uint8_t>(life_stage + 1);
        if (log_host != nullptr && environment.is_selected_creature(*this) &&
            log_host->debug_category_enabled(0x400)) {
            common::debug_log(*log_host, 0x400, "Age changed to %d\\n",
                              genome_life_stage_);
        }
        if (genome_life_stage_ < kTerminalLifeStage) {
            environment.initialize_from_genome(*this);
        } else {
            environment.die(*this);
        }
    }

    if (control_state().death_signal != 0) {
        environment.die(*this);
    }
}

bool Creature::force_age_one_stage(CreatureEnvironmentHost& environment,
                                   common::DebugLogHost* log_host) {
    constexpr auto kLastAgeableStage =
        GenomeLifeStage::stage_six;
    constexpr auto kTerminalStage =
        GenomeLifeStage::terminal_stage_seven;

    if (genome_life_stage() > kLastAgeableStage) {
        return false;
    }

    genome_life_stage_ = static_cast<std::uint8_t>(genome_life_stage_) + 1;
    if (log_host != nullptr && environment.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x400)) {
        common::debug_log(*log_host, 0x400, "Age changed to %d\\n",
                          genome_life_stage_);
    }

    if (genome_life_stage() < kTerminalStage) {
        environment.initialize_from_genome(*this);
    } else {
        environment.die(*this);
    }
    return true;
}

void Creature::apply_instant_verb_vocabulary(
    CreatureEnvironmentHost& environment,
    const MultibyteTextApi& text_api,
    CreatureSpeechHost& speech_host) {
    auto install_word = [&](std::size_t record_index, const char* word) {
        LearnedWordRecord& record = learned_word_records_[record_index];
        text_api.copy_string(record.recognized_word,
                             kLearnedWordTextCapacity, word);
        text_api.copy_string(record.response_word,
                             kLearnedWordTextCapacity, word);
        std::rand();
        record.reinforcement = 0xff;
    };

    for (std::size_t index = 0; index < std::size(kDefaultVocabularyGroup3);
         ++index) {
        install_word(index, kDefaultVocabularyGroup3[index]);
    }
    install_word(0x10, "Ron");
    install_word(0x4a, "look");
    install_word(0x4b, "what");

    if (genome_life_stage() == GenomeLifeStage::stage_zero) {
        genome_life_stage_ = static_cast<std::uint8_t>(
            GenomeLifeStage::stage_one);
        environment.initialize_from_genome(*this);
    }

    instinct_runtime_state_.dream_countdown = 8;
    speak("WAIT FOR 40 SECS!", speech_host);
}

void Creature::remove_from_world(CreatureRemovalHost& host) {
    host.end_interactions_with_creature(*this);

    const CreatureSelectionRemovalResult selection =
        host.remove_from_creature_selection(*this);
    if (selection.was_selected) {
        host.refresh_after_selected_creature_removal(
            *this, selection.remaining_selection_count);
    }

    host.clear_bounds_reference_and_set_default(*this);
    host.release_sleep_indicator(*this);
    host.notify_dependents_on_removal(*this);

    instinct_runtime_state_.dream_countdown = 0;
    host.purge_destroy_when_finished_macros(*this);
    host.decrement_living_norns();
    host.notify_creature_removed_to_embedded_kits();
}

void Creature::update_bacterium_and_environment(
    CreatureBacteriumEnvironmentHost& host,
    common::DebugLogHost* log_host) {
    Bacterium& bacterium = bacterium_;

    if (log_host != nullptr && host.is_selected_creature(*this) &&
        log_host->debug_category_enabled(0x10)) {
        const auto& drives = control_state().goal_direction_drive_levels;
        common::debug_log(
            *log_host, 0x10,
            "PAI=%3.3d NFP=%3.3d HUN=%3.3d COL=%3.3d HOT=%3.3d "
            "TIR=%3.3d SLE=%3.3d LON=%3.3d CRO=%3.3d FEA=%3.3d "
            "BOR=%3.3d ANG=%3.3d SEX=%3.3d xxx=%3.3d xxx=%3.3d "
            "xxx=%3.3d\n",
            drives.pain, drives.nfp, drives.hunger, drives.coldness,
            drives.hotness, drives.tiredness, drives.sleepiness,
            drives.loneliness, drives.crowdedness, drives.fear,
            drives.boredom, drives.anger, drives.sex,
            drives.not_allocated_drive_2, drives.not_allocated_drive_3,
            drives.not_allocated_drive_4);
    }

    if (bacterium.activity_state() != BacteriumActivityState::inactive) {
        biochemistry::Biochemistry& biochemistry = *biochemistry_;
        const std::uint8_t input_chemical = static_cast<std::uint8_t>(
            static_cast<std::int32_t>(bacterium.input_chemical_id()) - 8);
        const std::uint8_t input_concentration =
            biochemistry.chemical_states()[input_chemical].concentration;

        if (bacterium.activity_state() == BacteriumActivityState::dormant &&
            input_concentration < bacterium.activation_threshold()) {
            bacterium.set_activity_state(BacteriumActivityState::active);
            if (log_host != nullptr &&
                log_host->debug_category_enabled(0x400)) {
                common::debug_log(*log_host, 0x400, "Bacterium active!\n");
            }
        }

        if (bacterium.kill_threshold() < input_concentration) {
            bacterium.set_activity_state(BacteriumActivityState::inactive);
            if (log_host != nullptr &&
                log_host->debug_category_enabled(0x400)) {
                common::debug_log(*log_host, 0x400, "Bacterium killed!\n");
            }
        } else {
            biochemistry.add_chemical_moles(
                static_cast<int>(bacterium.input_chemical_id()), 0x50,
                log_host);
            for (const std::int8_t output_chemical_id :
                 bacterium.output_chemical_ids()) {
                if (output_chemical_id != 0) {
                    auto& output = biochemistry.chemical_states()[
                        static_cast<std::uint8_t>(output_chemical_id)];
                    const std::uint32_t concentration =
                        static_cast<std::uint32_t>(output.concentration) + 0x1e;
                    output.concentration = static_cast<std::uint8_t>(
                        std::min<std::uint32_t>(concentration, 0xffu));
                    if (log_host != nullptr &&
                        log_host->debug_category_enabled(0x10)) {
                        common::debug_log(
                            *log_host, 0x10,
                            "Add %d moles of Chemical %d (conc->%d)\n",
                            0x1e, static_cast<int>(output_chemical_id),
                            static_cast<unsigned int>(output.concentration));
                    }
                }
            }
        }
    }

    if (death_state_ == 0) {
        update_environment_and_life_stage(host.environment_host(), log_host);
        update_goal_direction(host.goal_direction_host());
    }
}

void Creature::process_dreaming(bool selected_creature,
                                common::DebugLogHost* log_host) {
    if (instinct_runtime_state_.dream_countdown == 0) {
        return;
    }

    if (instinct_runtime_state_.instinct_count == 0) {
        instinct_runtime_state_.dream_countdown = 0;
        if (selected_creature && log_host != nullptr &&
            log_host->debug_category_enabled(0x400)) {
            common::debug_log(
                *log_host, 0x400,
                "Started dreaming but there were no instincts to process\n");
        }
        return;
    }

    brain::Instinct& current_instinct = instinct_runtime_state_.instincts[0];
    if (current_instinct.dream_step_index == 0) {
        brain_->reset_runtime_activity();
        ++current_instinct.dream_step_index;
        ++biochemistry_tick_;
        return;
    }

    if (current_instinct.dream_step_index < 0x23) {
        current_instinct.run_dream_step(*brain_, *biochemistry_,
                                        biochemistry_tick_);
        ++current_instinct.dream_step_index;
        ++biochemistry_tick_;
        return;
    }

    if (current_instinct.dream_step_index < 0x28) {
        biochemistry_->chemical_states()[current_instinct.dream_chemical_index]
            .concentration = static_cast<std::uint8_t>(
                current_instinct.dream_chemical_concentration);
        current_instinct.run_dream_step(*brain_, *biochemistry_,
                                        biochemistry_tick_);
        ++current_instinct.dream_step_index;
        ++biochemistry_tick_;
        return;
    }

    brain_->normalize_dream_connection_weights();
    brain_->reset_runtime_activity();

    for (std::size_t index = 1;
         index < instinct_runtime_state_.instinct_count; ++index) {
        instinct_runtime_state_.instincts[index - 1] =
            instinct_runtime_state_.instincts[index];
    }
    instinct_runtime_state_.instincts[
        instinct_runtime_state_.instinct_count - 1] = {};
    --instinct_runtime_state_.instinct_count;

    if (instinct_runtime_state_.instinct_count == 0) {
        instinct_runtime_state_.dream_countdown = 0;
    } else if (genome_life_stage_ != 0) {
        --instinct_runtime_state_.dream_countdown;
    }

    if (selected_creature && log_host != nullptr &&
        log_host->debug_category_enabled(0x400)) {
        common::debug_log(*log_host, 0x400,
                          "An instinct has been successfully processed\n");
        if (instinct_runtime_state_.dream_countdown == 0) {
            common::debug_log(*log_host, 0x400,
                              "Dream time is over.%d instincts remain\n",
                              instinct_runtime_state_.instinct_count);
        }
    }
    ++biochemistry_tick_;
}

void Creature::serialize(
    CreatureArchive& archive,
    objects::ObjectSoundPlaybackHost* sound_host,
    common::DebugLogHost* log_host) {
    skeleton_.serialize(archive, sound_host);

    if (archive.is_loading()) {
        for (LearnedWordRecord& record : learned_word_records_) {
            record.response_word[0] = '\0';
            load_creature_text(archive.read_string(), record.response_word,
                               kLearnedWordTextCapacity);
            load_creature_text(archive.read_string(), record.recognized_word,
                               kLearnedWordTextCapacity);
            record.reinforcement = archive.read_uint32();
        }

        for (AttentionRecord& record : attention_records_) {
            record = {};
            record.world_x = archive.read_int32();
            record.world_y = archive.read_int32();
        }

        for (StimulusContext& context : built_in_stimulus_contexts_) {
            context.target = nullptr;
            context.source_creature = nullptr;
            context.descriptor.attention_activation = archive.read_byte();
            context.descriptor.target_neuron_index = archive.read_byte();
            context.descriptor.target_lobe_activation = archive.read_byte();
            context.descriptor.flags = archive.read_byte();
            context.chemical_ids.first = archive.read_byte();
            context.chemical_amounts.first = archive.read_byte();
            context.chemical_ids.second = archive.read_byte();
            context.chemical_amounts.second = archive.read_byte();
            context.chemical_ids.third = archive.read_byte();
            context.chemical_amounts.third = archive.read_byte();
            context.chemical_ids.fourth = archive.read_byte();
            context.chemical_amounts.fourth = archive.read_byte();
        }

        brain_.reset(static_cast<brain::Brain*>(
            archive.read_object_reference("CBrain")));
        biochemistry_.reset(static_cast<biochemistry::Biochemistry*>(
            archive.read_object_reference("CBiochemistry")));

        // C1 stores gender as MALE=1 / FEMALE=2 -- FormatStatusForExternalQuery
        // @ 0040e520 tests `byte [this+0x2cc1] == 1` for male and prints the
        // raw value as the third status field.  GenomeSex is this port's
        // zero-based application enum (see body.cpp), so the encodings differ
        // by one and the boundary has to convert.  Casting the archived byte
        // straight in made every male read back as female.
        const std::uint8_t archived_gender = archive.read_byte();
        genome_sex_ = archived_gender <= 1u ? GenomeSex::male
                                            : GenomeSex::female;
        genome_life_stage_ = archive.read_byte();
        biochemistry_tick_ = archive.read_uint32();
        gamete_genome_source_filename_ = archive.read_uint32();
        child_genome_source_filename_ = archive.read_uint32();
        death_state_ = archive.read_byte();
        dead_ = death_state_ != 0;
        register_state_.set_age_ticks(archive.read_uint32());
        const std::uint32_t archived_instinct_count = archive.read_uint32();
        // The dream countdown sits between the instinct count and the instinct
        // records themselves; reading the records without it consumed the
        // countdown as the first record's tag and desynchronised the rest of
        // the creature.
        instinct_runtime_state_.dream_countdown = archive.read_uint32();
        instinct_runtime_state_.instinct_count = std::min<std::uint32_t>(
            archived_instinct_count,
            static_cast<std::uint32_t>(kInstinctCapacity));
        brain::Instinct discarded_instinct;
        for (std::uint32_t index = 0; index < archived_instinct_count;
             ++index) {
            brain::Instinct& destination =
                index < kInstinctCapacity
                    ? instinct_runtime_state_.instincts[index]
                    : discarded_instinct;
            archive.read_instinct_reference(destination);
        }

        for (auto& row : goal_direction_weight_matrix_) {
            for (std::int32_t& value : row) {
                value = archive.read_int32();
            }
        }

        // Creature::Serialize @ 0x00407440 reads the sleep indicator as a
        // SimpleObject reference between the goal-direction matrix and the
        // voice.  Skipping the tag and reading two bytes before the matrix
        // instead happens to balance out while every creature's indicator is
        // null -- and desynchronises the rest of the creature, and everything
        // after it, the moment one is not.
        sleep_indicator_object_ = static_cast<objects::Object*>(
            archive.read_object_reference("SimpleObject"));
        CreatureVoiceArchive voice_archive(archive);
        voice_.serialize(voice_archive);

        CreatureStringArchive register_archive(archive);
        register_state_.serialize(register_archive);
        skeleton_.enable_ticking();
        return;
    }

    for (const LearnedWordRecord& record : learned_word_records_) {
        archive.write_string(
            creature_text(record.response_word, kLearnedWordTextCapacity));
        archive.write_string(
            creature_text(record.recognized_word, kLearnedWordTextCapacity));
        archive.write_uint32(record.reinforcement);
    }

    for (const AttentionRecord& record : attention_records_) {
        archive.write_int32(record.world_x);
        archive.write_int32(record.world_y);
    }

    for (const StimulusContext& context : built_in_stimulus_contexts_) {
        archive.write_byte(context.descriptor.attention_activation);
        archive.write_byte(context.descriptor.target_neuron_index);
        archive.write_byte(context.descriptor.target_lobe_activation);
        archive.write_byte(context.descriptor.flags);
        archive.write_byte(context.chemical_ids.first);
        archive.write_byte(context.chemical_amounts.first);
        archive.write_byte(context.chemical_ids.second);
        archive.write_byte(context.chemical_amounts.second);
        archive.write_byte(context.chemical_ids.third);
        archive.write_byte(context.chemical_amounts.third);
        archive.write_byte(context.chemical_ids.fourth);
        archive.write_byte(context.chemical_amounts.fourth);
    }

    archive.write_object_reference(brain_.get(), "CBrain");
    archive.write_object_reference(biochemistry_.get(), "CBiochemistry");
    // Back to C1's MALE=1 / FEMALE=2; writing the zero-based value would have
    // handed the real game a gender it reads as one lower.
    archive.write_byte(
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(genome_sex_) + 1u));
    archive.write_byte(genome_life_stage_);
    archive.write_uint32(biochemistry_tick_);
    archive.write_uint32(gamete_genome_source_filename_);
    archive.write_uint32(child_genome_source_filename_);
    archive.write_byte(death_state_);
    archive.write_uint32(register_state_.age_ticks());

    const std::uint32_t instinct_count = std::min<std::uint32_t>(
        instinct_runtime_state_.instinct_count,
        static_cast<std::uint32_t>(kInstinctCapacity));
    archive.write_uint32(instinct_count);
    archive.write_uint32(instinct_runtime_state_.dream_countdown);
    for (std::uint32_t index = 0; index < instinct_count; ++index) {
        archive.write_instinct_reference(
            instinct_runtime_state_.instincts[index]);
    }

    for (const auto& row : goal_direction_weight_matrix_) {
        for (std::int32_t value : row) {
            archive.write_int32(value);
        }
    }

    archive.write_object_reference(sleep_indicator_object_, "SimpleObject");

    CreatureVoiceArchive voice_archive(archive);
    voice_.serialize(voice_archive);
    CreatureStringArchive register_archive(archive);
    register_state_.serialize(register_archive);

    (void)log_host;
}

void Creature::deserialize(CreatureArchive& archive,
                           CreatureDeserializationHost& host,
                           common::DebugLogHost* log_host) {
    // The native archive contains a dynamic CGenome object, not an inline
    // byte array.  The adapter owns the runtime class tag and lifetime while
    // this method retains the recovered collision/materialisation order.
    auto genome = host.read_genome_reference(archive);
    if (genome == nullptr) {
        return;
    }

    host.ensure_unique_primary_genome_filename(*genome, *this);
    host.save_generated_genome(*genome);

    // LoadGenome is deliberately invoked after the collision resolution and
    // generated-file write, matching the native CGenome/Skeleton sequence.
    if (!host.load_materialized_genome(*this, *genome)) {
        return;
    }

    genome.reset();

    if (child_genome_source_filename_ != 0) {
        if (log_host != nullptr) {
            common::debug_log(*log_host, 0x1000,
                              "Serialising in child genome.");
        }

        auto child_genome = host.read_genome_reference(archive);
        if (child_genome != nullptr) {
            host.ensure_unique_child_genome_filename(*child_genome, *this);
            host.save_generated_genome(*child_genome);
            child_genome.reset();
        }
    }

    host.set_unbounded_bounds_and_update(*this);
    host.move_to_and_redraw(*this, 0xa1f, 0x39f);
    host.set_default_bounds_and_update(*this);

    host.select_loaded_creature(*this);
    host.rebuild_creature_selection_menu();
    host.increment_living_norns();
    host.notify_creature_loaded_to_embedded_kits();

    if (biochemistry_ != nullptr) {
        biochemistry_->update(biochemistry_tick_, log_host);
    }
}

} // namespace creatures1::creatures
