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

constexpr unsigned kSysCommandOnTop = 0x0020;

// The classic look's menu (1996: Help > About Hatchery, ID_APP_ABOUT).
constexpr unsigned kCommandMute = 0x8101;
constexpr unsigned kCommandRefill = 0x8102;
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
