#pragma once

// Breeder's Kit resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace breeder {

constexpr unsigned kDialogPage = 200;  // blank page; controls made in code
constexpr unsigned kIconKit = 128;
constexpr unsigned kIconFemale = 187;  // Page8A loads 0xbb for a female
constexpr unsigned kIconMale = 188;    // and 0xbc for a male

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringOestrogen = 101;     // "Estrogen"
constexpr unsigned kStringProgesterone = 102;
constexpr unsigned kStringTestosterone = 103;
constexpr unsigned kStringSexDrive = 105;
constexpr unsigned kStringTitlePaused = 107;   // "Breeder's Kit..."
constexpr unsigned kStringTitle = 108;         // "Breeder's Kit - "
constexpr unsigned kStringPausedMarker = 109;  // " Paused - "
constexpr unsigned kStringShopTab = 111;       // "Aphrodisiac page"
constexpr unsigned kStringFertilityTab = 112;  // "Fertility page"
constexpr unsigned kStringToolName = 116;      // "Breeder's Kit"
constexpr unsigned kStringToolHelp = 117;      // "Pregnancy monitoring"

// Timers
constexpr unsigned kTimerStartup = 1;
constexpr unsigned kTimerPoll = 2;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kPollMs = 1000;

constexpr unsigned kCommandBufferBytes = 0x1000;

// The page area the kit opens at; the 1996 pages' 216 x 212 dialog units are
// the smallest it can be.
constexpr int kDefaultPageWidthDlu = 320;
constexpr int kDefaultPageHeightDlu = 250;

// System menu item: Always on top (the original's menu 143, never loaded).
constexpr unsigned kSysCommandOnTop = 0x0020;

enum : unsigned {
    kControlFertility = 3000,
};

} // namespace breeder
