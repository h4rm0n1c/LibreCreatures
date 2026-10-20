#include "fix_catalog.hpp"

namespace creatures1::common {

const FixCatalogEntry kFixCatalog[] = {
    {1, "creature cohorts: four, stepped by four", "Brain, genetics, biochemistry", true},
    {2, "rule registers reset per neuron", "Brain, genetics, biochemistry", true},
    {4, "migrating neuron does not match itself", "Brain, genetics, biochemistry", true},
    {5, "migration duplicate test by target sets", "Brain, genetics, biochemistry", true},
    {6, "loose dendrite duplicate test", "Brain, genetics, biochemistry", true},
    {7, "mutation keeps the gext tag", "Brain, genetics, biochemistry", true},
    {8, "sense values of 128+ reach the brain", "Creatures", true},
    {9, "parked (killed) objects are forgotten", "Creatures", true},
    {11, "involuntary action with no script", "Creatures", true},
    {12, "gait selection tests the chosen gait", "Creatures", true},
    {13, "empty-handed drop does not disappoint", "Creatures", true},
    {14, "\"come Norn\", \"run Norn\"", "Creatures", true},
    {15, "status report pregnancy stage", "Creatures", true},
    {16, "missing body sprite rebuilt", "Creatures", true},
    {17, "stm# tact reaches only the toucher", "Creatures", true},
    {18, "the egg in the sky", "Scripts", true},
    {22, "the Black Hole", "World, objects and vehicles", true},
    {23, "the Flying Lemon", "World, objects and vehicles", true},
    {24, "lift called to its own floor", "World, objects and vehicles", true},
    {26, "Ocean Dome Sound unseen by creatures", "World data (repaired on load)", false},
    {27, "drop in a room's upper half", "World, objects and vehicles", true},
    {28, "click target by plane", "The hand and the interface", false},
    {29, "hand moves before the click", "The hand and the interface", false},
    {30, "deleting a creature keeps the selection", "The hand and the interface", false},
    {31, "hand freed when its object goes", "The hand and the interface", false},
    {32, "no empty spoken word", "The hand and the interface", false},
    {33, "save/speed change keep a pause", "The hand and the interface", false},
    {35, "SFC.OLE brain report runs its script", "Kit protocol and sound", false},
    {36, "sound cache size", "Kit protocol and sound", false},
    {37, "every listener hears every word", "Creatures", true},
    {38, "unknown parents are not siblings", "Creatures", true},
    {39, "no empty heard words", "Creatures", true},
    {40, "a command to category 0 is delivered", "Creatures", true},
    {41, "lift arrival at fractional speeds", "World, objects and vehicles", true},
    {42, "mutated bacterium output keeps its offset", "Creatures", true},
    {43, "status report names the norn's own room", "Creatures", true},
    {44, "bacterial threshold mutation clamps", "Creatures", true},
    {45, "all eight lift call slots answered", "World, objects and vehicles", true},
    {49, "renamed import keeps its gamete's name", "Creatures", true},
    {50, "reaction product yield not cut to a byte", "Brain, genetics, biochemistry", true},
    {51, "same reactant twice counted once", "Brain, genetics, biochemistry", true},
    {52, "circling vehicle wraps its position", "World, objects and vehicles", true},
    {53, "enum/next survives kill", "Scripts", true},
    {54, "empty enum skips to its own next", "Scripts", true},
    {55, "block scans skip bracketed text", "Scripts", true},
    {56, "gsub label scan skips bracketed text", "Scripts", true},
    {57, "mvto/mvby/mcrt move a vehicle's position", "World, objects and vehicles", true},
    {59, "moving vehicle frame codes", "World, objects and vehicles", true},
    {61, "ended script does not cost the next a turn", "Scripts", true},
    {62, "looping sounds survive mute", "Kit protocol and sound", false},
    {64, "dead kit's slot is freed", "Kit protocol and sound", false},
    {67, "kit toolbar tooltips", "The hand and the interface", false},
    {68, "event bar smooth scroll", "The hand and the interface", false},
    {69, "Retreat script", "World data (repaired on load)", false},
    {70, "Say Need scripts", "World data (repaired on load)", false},
    {71, "the drum", "World data (repaired on load)", false},
    {72, "the island boat", "World data (repaired on load)", false},
    {73, "perception outside every room", "Creatures", true},
    {74, "empty macro reply (CE 10.4)", "Kit protocol and sound", false},
    {75, "kit window raised (CE 10.4)", "Kit protocol and sound", false},
    {76, "kit pipe waits for a starting kit", "Kit protocol and sound", false},
    {83, "body sprite file without two stray bytes", "Creatures", true},};

const std::size_t kFixCatalogSize = sizeof(kFixCatalog) / sizeof(kFixCatalog[0]);

const FixCatalogEntry* find_fix_catalog_entry(int row) {
    for (std::size_t i = 0; i < kFixCatalogSize; ++i) {
        if (kFixCatalog[i].row == row) {
            return &kFixCatalog[i];
        }
    }
    return nullptr;
}

}  // namespace creatures1::common
