#pragma once

#include <cstdint>

namespace creatures1::ui {

class ScoreArchive {
public:
    virtual ~ScoreArchive() = default;
    virtual bool is_loading() const = 0;
    virtual std::int32_t read_int32() = 0;
    virtual void write_int32(std::int32_t value) = 0;
};

class Score {
public:
    int hatchery_eggs_used = 0;
    int natural_eggs_laid = 0;
    int dead_norns = 0;
    int living_norns = 0;
    int population_time_accumulator = 0;

    void serialize(ScoreArchive& archive);
};

extern Score* g_score;

// Constructs the process-wide score owner and publishes it through g_score.
Score* allocate_and_construct_score();

} // namespace creatures1::ui
