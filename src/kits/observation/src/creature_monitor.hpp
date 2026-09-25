#pragma once

// What the Observation Kit decides from each poll: every creature's row icon
// and which alerts to raise.  Plain C++ with no UI, so it can be tested
// without Windows (see ../tests).
//
// The original made these decisions inside its list update; ../ORIGINAL.md
// numbers its bugs, and the fixes here are marked "Fix (bug N)".

#include "c1kit/protocol.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace observation {

// Alert settings, kept by the sheet and saved under the kit's registry key
// (defaults from COverviewSheet::Constructor @ 0x004030b0).
struct AlertSettings {
    std::uint32_t alert_near_death = 1;
    std::uint32_t alert_on_pregnancy = 1;
    std::uint32_t alert_on_birth = 1;
    std::uint32_t message_box = 0;
    std::uint32_t warn_level = 25;
};

// Row icons: image indices into bitmap 142.
enum RowIcon : int {
    kIconNone = 0,
    kIconBirth = 1,      // egg
    kIconNearDeath = 2,  // gravestone
    kIconPregnant = 3,   // sex symbol
};

enum AlertType : int {
    kAlertPregnancy = 0,
    kAlertBirth = 1,
    kAlertNearDeath = 2,
};

struct Alert {
    AlertType type;
    std::string name;
};

// Pregnancy is "N/A" for males, "No", or the gestation stage.
inline bool is_pregnant(const std::string& pregnancy) {
    return !pregnancy.empty() && pregnancy != "N/A" && pregnancy != "No";
}

inline bool is_due(const std::string& pregnancy) {
    return pregnancy == "7" || pregnancy == "8";
}

// Life force is "NN%" for a living creature; anything else (the game sends
// "Dead") is not a percentage.
inline bool is_alive(const std::string& life_force) {
    return !life_force.empty() && life_force.back() == '%';
}

inline bool is_low(const std::string& life_force, std::uint32_t warn_level) {
    return !is_alive(life_force) ||
           c1kit::life_force_percent(life_force) < static_cast<int>(warn_level);
}

// Fix (bug 11): the icon shows the creature's state whether or not the
// matching alert is switched on.  As in the original, a later match wins:
// pregnant, then due, then near death (a dead creature shows the grave).
inline RowIcon row_icon(const c1kit::OverviewRecord& record,
                        const AlertSettings& settings) {
    RowIcon icon = kIconNone;
    if (is_pregnant(record[c1kit::kOverviewPregnancy])) {
        icon = kIconPregnant;
    }
    if (is_due(record[c1kit::kOverviewPregnancy])) {
        icon = kIconBirth;
    }
    if (is_low(record[c1kit::kOverviewLifeForce], settings.warn_level)) {
        icon = kIconNearDeath;
    }
    return icon;
}

class CreatureMonitor {
public:
    // How often a near-death alert repeats while a creature stays below the
    // warn level (the original's per-row 30-second re-arm timers).
    static constexpr std::uint32_t kNearDeathRepeatMs = 30000;

    // Takes one poll's records and returns the alerts to raise.  `now_ms` is
    // a millisecond clock that may wrap (GetTickCount).
    std::vector<Alert> update(const std::vector<c1kit::OverviewRecord>& records,
                              const AlertSettings& settings,
                              std::uint32_t now_ms) {
        std::vector<Alert> alerts;
        std::map<std::string, Watch> seen;
        for (const c1kit::OverviewRecord& record : records) {
            const std::string& moniker = record[c1kit::kOverviewMoniker];
            const std::string& name = record[c1kit::kOverviewName];
            const std::string& pregnancy = record[c1kit::kOverviewPregnancy];
            const std::string& life_force = record[c1kit::kOverviewLifeForce];
            const auto known = watch_.find(moniker);
            const bool first_sight = known == watch_.end();
            Watch watch = first_sight ? Watch{} : known->second;
            const bool pregnant = is_pregnant(pregnancy);

            // Fix (bug 7): a pregnancy is noticed at whatever stage a poll
            // first sees it; the original needed a poll to land on stage 1.
            // Creatures already pregnant when the kit opens are not
            // announced.
            if (pregnant && !watch.was_pregnant && !first_sight &&
                settings.alert_on_pregnancy != 0) {
                alerts.push_back({kAlertPregnancy, name});
            }
            // Once per pregnancy, including one first seen already due.
            if (is_due(pregnancy) && !watch.birth_alerted) {
                watch.birth_alerted = true;
                if (settings.alert_on_birth != 0) {
                    alerts.push_back({kAlertBirth, name});
                }
            }
            if (!pregnant) {
                watch.birth_alerted = false;
            }
            watch.was_pregnant = pregnant;

            // Fix (bug 8): the original parsed "Dead" as 0% and raised the
            // near-death alert for dead creatures.
            if (is_alive(life_force) &&
                is_low(life_force, settings.warn_level)) {
                if (settings.alert_near_death != 0 &&
                    (!watch.near_death_pending ||
                     static_cast<std::int32_t>(now_ms - watch.next_near_death) >=
                         0)) {
                    alerts.push_back({kAlertNearDeath, name});
                    watch.near_death_pending = true;
                    watch.next_near_death = now_ms + kNearDeathRepeatMs;
                }
            } else {
                watch.near_death_pending = false;
            }
            seen[moniker] = watch;
        }
        // Creatures no longer reported are forgotten.
        watch_.swap(seen);
        return alerts;
    }

private:
    // What is remembered about one creature between polls, keyed by moniker.
    // Fix (bug 9): the original kept this per list row, so it moved to
    // another creature whenever rows were inserted or deleted.
    struct Watch {
        bool was_pregnant = false;
        bool birth_alerted = false;
        bool near_death_pending = false;
        std::uint32_t next_near_death = 0;
    };

    std::map<std::string, Watch> watch_;
};

} // namespace observation
