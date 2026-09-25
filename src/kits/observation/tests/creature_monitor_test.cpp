// Portable tests for the Observation Kit's alert and icon decisions.
// Builds with any C++17 compiler:
//   g++ -std=c++17 -I../src -I../../c1kitlib/include creature_monitor_test.cpp

#include "creature_monitor.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace observation;

c1kit::OverviewRecord creature(const std::string& name,
                               const std::string& moniker,
                               const std::string& pregnancy,
                               const std::string& life_force) {
    c1kit::OverviewRecord record;
    record.fields[c1kit::kOverviewName] = name;
    record.fields[c1kit::kOverviewMoniker] = moniker;
    record.fields[c1kit::kOverviewSex] = "2";
    record.fields[c1kit::kOverviewAge] = "0:10";
    record.fields[c1kit::kOverviewPregnancy] = pregnancy;
    record.fields[c1kit::kOverviewLifeForce] = life_force;
    record.fields[c1kit::kOverviewMedical] = "Healthy";
    record.fields[c1kit::kOverviewRoom] = "7";
    return record;
}

std::vector<AlertType> types(const std::vector<Alert>& alerts) {
    std::vector<AlertType> out;
    for (const Alert& alert : alerts) {
        out.push_back(alert.type);
    }
    return out;
}

void test_icons() {
    const AlertSettings settings;  // warn level 25
    assert(row_icon(creature("a", "1", "No", "50%"), settings) == kIconNone);
    assert(row_icon(creature("a", "1", "3", "50%"), settings) == kIconPregnant);
    assert(row_icon(creature("a", "1", "7", "50%"), settings) == kIconBirth);
    assert(row_icon(creature("a", "1", "7", "10%"), settings) == kIconNearDeath);
    assert(row_icon(creature("a", "1", "N/A", "Dead"), settings) ==
           kIconNearDeath);
    // Icons do not depend on the alert switches (bug 11).
    AlertSettings off;
    off.alert_near_death = off.alert_on_pregnancy = off.alert_on_birth = 0;
    assert(row_icon(creature("a", "1", "3", "50%"), off) == kIconPregnant);
}

void test_pregnancy_at_any_stage() {
    CreatureMonitor monitor;
    const AlertSettings settings;
    // Already pregnant when first seen: not announced.
    assert(monitor.update({creature("a", "1", "4", "50%")}, settings, 0).empty());
    // A creature seen not pregnant, then at stage 3 (a poll never landed on
    // stage 1): announced once (bug 7).
    monitor.update({creature("b", "2", "No", "50%")}, settings, 0);
    assert(types(monitor.update({creature("b", "2", "3", "50%")}, settings, 0)) ==
           std::vector<AlertType>{kAlertPregnancy});
    assert(monitor.update({creature("b", "2", "4", "50%")}, settings, 0).empty());
}

void test_birth_once_per_pregnancy() {
    CreatureMonitor monitor;
    const AlertSettings settings;
    // First seen already due: announced.
    assert(types(monitor.update({creature("a", "1", "7", "50%")}, settings, 0)) ==
           std::vector<AlertType>{kAlertBirth});
    assert(monitor.update({creature("a", "1", "8", "50%")}, settings, 0).empty());
    // Delivered, then a new pregnancy reaching term again.
    monitor.update({creature("a", "1", "No", "50%")}, settings, 0);
    const auto again =
        types(monitor.update({creature("a", "1", "7", "50%")}, settings, 0));
    assert((again == std::vector<AlertType>{kAlertPregnancy, kAlertBirth}));
}

void test_state_follows_the_creature() {
    // Alert state is keyed by moniker, so a creature leaving the list does
    // not hand its state to the next one (bug 9).
    CreatureMonitor monitor;
    const AlertSettings settings;
    monitor.update({creature("a", "1", "7", "50%"),
                    creature("b", "2", "No", "50%")}, settings, 0);
    // "a" leaves; "b" becomes due and must still be announced.
    assert(types(monitor.update({creature("b", "2", "7", "50%")}, settings, 0)) ==
           (std::vector<AlertType>{kAlertPregnancy, kAlertBirth}));
}

void test_near_death_repeats_and_skips_the_dead() {
    CreatureMonitor monitor;
    const AlertSettings settings;
    const std::uint32_t repeat = CreatureMonitor::kNearDeathRepeatMs;
    assert(types(monitor.update({creature("a", "1", "N/A", "10%")}, settings,
                                1000)) ==
           std::vector<AlertType>{kAlertNearDeath});
    assert(monitor.update({creature("a", "1", "N/A", "10%")}, settings,
                          1000 + repeat - 1).empty());
    assert(monitor.update({creature("a", "1", "N/A", "10%")}, settings,
                          1000 + repeat).size() == 1);
    // Recovering re-arms immediately.
    monitor.update({creature("a", "1", "N/A", "60%")}, settings, 50000);
    assert(monitor.update({creature("a", "1", "N/A", "10%")}, settings,
                          50001).size() == 1);
    // Dead creatures never raise it (bug 8).
    assert(monitor.update({creature("z", "9", "N/A", "Dead")}, settings, 0)
               .empty());
    // The clock may wrap.
    CreatureMonitor wrapping;
    wrapping.update({creature("a", "1", "N/A", "10%")}, settings, 0xfffff000u);
    assert(wrapping.update({creature("a", "1", "N/A", "10%")}, settings,
                           0xfffff000u + repeat).size() == 1);
}

void test_switches() {
    CreatureMonitor monitor;
    AlertSettings off;
    off.alert_near_death = off.alert_on_pregnancy = off.alert_on_birth = 0;
    monitor.update({creature("a", "1", "No", "50%")}, off, 0);
    assert(monitor.update({creature("a", "1", "7", "10%")}, off, 0).empty());
}

} // namespace

int main() {
    test_icons();
    test_pregnancy_at_any_stage();
    test_birth_once_per_pregnancy();
    test_state_follows_the_creature();
    test_near_death_repeats_and_skips_the_dead();
    test_switches();
    std::puts("creature monitor tests passed");
    return 0;
}
