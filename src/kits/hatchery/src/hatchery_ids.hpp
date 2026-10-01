#pragma once

// Hatchery resource ids (the original's, and LibreCreatures' own in
// lc_resources.rcinc).  See ../ORIGINAL.md.

namespace hatchery {

constexpr unsigned kIconKit = 128;
constexpr unsigned kDialogPage = 300;
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringNoMoreEggs = 61205;  // "You have no more eggs!"
constexpr unsigned kStringToolName = 200;      // "The Hatchery"
constexpr unsigned kStringToolHelp = 201;      // "Hatch a new norn"
constexpr unsigned kStringNestTab = 202;

constexpr unsigned kTimerStartup = 1;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kCommandBufferBytes = 0x1000;

constexpr int kDefaultPageWidthDlu = 440;
constexpr int kDefaultPageHeightDlu = 220;

constexpr unsigned kOptionRefill = 0x0200;  // Options > Refill the nest
constexpr unsigned kOptionScramble = 0x0210;  // Options > Scramble Eggs
constexpr unsigned kOptionColourful = 0x0220;  // Options > Make My Creatures Colourful
// The 1996 kit's sounds (the game's Sounds folder), loaded for both looks.
constexpr const char* kHatcherySounds[] = {"hfan", "hdsk", "hlgt", "hegg", "hmle", "hfml", "hslt"};
constexpr int kLibreFanVolume = -1000;  // the fan under the nest, quieter than the machine's
// The classic look's menu (1996: Help > About Hatchery, ID_APP_ABOUT).
constexpr unsigned kCommandAbout = 57664;
constexpr unsigned kTimerMachine = 2;
constexpr unsigned kMachineTickMs = 75;  // the 1996 view's timer
constexpr int kMachineWidth = 320;
constexpr int kMachineHeight = 240;

enum : unsigned {
    kControlNest = 3000,
    kControlHatch,
    kControlRefill,
    kControlStatus,
};

} // namespace hatchery
