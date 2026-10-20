#include "fix_catalog.hpp"

namespace creatures1::common {

const FixCatalogEntry kFixCatalog[] = {
    {1, "creature cohorts: four, stepped by four", "Brain, genetics, biochemistry", FixTier::locked},
    {2, "rule registers reset per neuron", "Brain, genetics, biochemistry", FixTier::locked},
    {4, "migrating neuron does not match itself", "Brain, genetics, biochemistry", FixTier::locked},
    {5, "migration duplicate test by target sets", "Brain, genetics, biochemistry", FixTier::locked},
    {6, "loose dendrite duplicate test", "Brain, genetics, biochemistry", FixTier::locked},
    {7, "mutation keeps the gext tag", "Brain, genetics, biochemistry", FixTier::locked},
    {8, "sense values of 128+ reach the brain", "Creatures", FixTier::default_off},
    {9, "parked (killed) objects are forgotten", "Creatures", FixTier::default_off},
    {11, "involuntary action with no script", "Creatures", FixTier::locked},
    {12, "gait selection tests the chosen gait", "Creatures", FixTier::locked},
    {13, "empty-handed drop does not disappoint", "Creatures", FixTier::default_off},
    {14, "\"come Norn\", \"run Norn\"", "Creatures", FixTier::locked},
    {15, "status report pregnancy stage", "Creatures", FixTier::locked},
    {16, "missing body sprite rebuilt", "Creatures", FixTier::locked},
    {17, "stm# tact reaches only the toucher", "Creatures", FixTier::default_off},
    {18, "the egg in the sky", "Scripts", FixTier::default_off},
    {22, "the Black Hole", "World, objects and vehicles", FixTier::default_off},
    {23, "the Flying Lemon", "World, objects and vehicles", FixTier::default_off},
    {24, "lift called to its own floor", "World, objects and vehicles", FixTier::default_off},
    {26, "Ocean Dome Sound unseen by creatures", "World data (repaired on load)", FixTier::locked},
    {27, "drop in a room's upper half", "World, objects and vehicles", FixTier::default_off},
    {28, "click target by plane", "The hand and the interface", FixTier::default_off},
    {29, "hand moves before the click", "The hand and the interface", FixTier::locked},
    {30, "deleting a creature keeps the selection", "The hand and the interface", FixTier::default_on},
    {31, "hand freed when its object goes", "The hand and the interface", FixTier::locked},
    {32, "no empty spoken word", "The hand and the interface", FixTier::locked},
    {33, "save/speed change keep a pause", "The hand and the interface", FixTier::locked},
    {35, "SFC.OLE brain report runs its script", "Kit protocol and sound", FixTier::locked},
    {36, "sound cache size", "Kit protocol and sound", FixTier::locked},
    {37, "every listener hears every word", "Creatures", FixTier::locked},
    {38, "unknown parents are not siblings", "Creatures", FixTier::locked},
    {39, "no empty heard words", "Creatures", FixTier::locked},
    {40, "a command to category 0 is delivered", "Creatures", FixTier::locked},
    {41, "lift arrival at fractional speeds", "World, objects and vehicles", FixTier::default_off},
    {42, "mutated bacterium output keeps its offset", "Creatures", FixTier::default_off},
    {43, "status report names the norn's own room", "Creatures", FixTier::locked},
    {44, "bacterial threshold mutation clamps", "Creatures", FixTier::default_off},
    {45, "all eight lift call slots answered", "World, objects and vehicles", FixTier::default_off},
    {49, "renamed import keeps its gamete's name", "Creatures", FixTier::locked},
    {50, "reaction product yield not cut to a byte", "Brain, genetics, biochemistry", FixTier::locked},
    {51, "same reactant twice counted once", "Brain, genetics, biochemistry", FixTier::locked},
    {52, "circling vehicle wraps its position", "World, objects and vehicles", FixTier::default_off},
    {53, "enum/next survives kill", "Scripts", FixTier::locked},
    {54, "empty enum skips to its own next", "Scripts", FixTier::locked},
    {55, "block scans skip bracketed text", "Scripts", FixTier::locked},
    {56, "gsub label scan skips bracketed text", "Scripts", FixTier::locked},
    {57, "mvto/mvby/mcrt move a vehicle's position", "World, objects and vehicles", FixTier::default_on},
    {59, "moving vehicle frame codes", "World, objects and vehicles", FixTier::default_off},
    {61, "ended script does not cost the next a turn", "Scripts", FixTier::locked},
    {62, "looping sounds survive mute", "Kit protocol and sound", FixTier::locked},
    {64, "dead kit's slot is freed", "Kit protocol and sound", FixTier::locked},
    {67, "kit toolbar tooltips", "The hand and the interface", FixTier::locked},
    {68, "event bar smooth scroll", "The hand and the interface", FixTier::default_on},
    {69, "Retreat script", "World data (repaired on load)", FixTier::locked},
    {70, "Say Need scripts", "World data (repaired on load)", FixTier::locked},
    {71, "the drum", "World data (repaired on load)", FixTier::locked},
    {72, "the island boat", "World data (repaired on load)", FixTier::locked},
    {73, "perception outside every room", "Creatures", FixTier::default_off},
    {74, "empty macro reply (CE 10.4)", "Kit protocol and sound", FixTier::locked},
    {75, "kit window raised (CE 10.4)", "Kit protocol and sound", FixTier::default_on},
    {76, "kit pipe waits for a starting kit", "Kit protocol and sound", FixTier::locked},
    {83, "body sprite file without two stray bytes", "Creatures", FixTier::locked},};

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
