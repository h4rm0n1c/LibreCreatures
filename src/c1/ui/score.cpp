#include "score.hpp"

#include <new>

namespace creatures1::ui {

Score* g_score = nullptr;

Score* allocate_and_construct_score() {
    Score* score = new (std::nothrow) Score{};
    if (score != nullptr) {
        g_score = score;
    }
    return score;
}

void Score::serialize(ScoreArchive& archive) {
    if (archive.is_loading()) {
        hatchery_eggs_used = archive.read_int32();
        natural_eggs_laid = archive.read_int32();
        dead_norns = archive.read_int32();
        living_norns = archive.read_int32();
        population_time_accumulator = archive.read_int32();
        return;
    }

    archive.write_int32(hatchery_eggs_used);
    archive.write_int32(natural_eggs_laid);
    archive.write_int32(dead_norns);
    archive.write_int32(living_norns);
    archive.write_int32(population_time_accumulator);
}

} // namespace creatures1::ui
