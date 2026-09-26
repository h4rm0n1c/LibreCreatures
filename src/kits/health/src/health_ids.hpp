#pragma once

// Health Kit resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace health {

constexpr unsigned kDialogPage = 200;  // blank page; controls made in code
constexpr unsigned kIconKit = 128;

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringTitlePaused = 101;   // "Health Kit... "
constexpr unsigned kStringTitle = 102;         // "Health Kit -  "
constexpr unsigned kStringPausedMarker = 103;  // " Paused - "
constexpr unsigned kStringFitnessTab = 110;    // "Fitness page"
constexpr unsigned kStringDoctorTab = 111;     // "Doctor's page"
constexpr unsigned kStringBrainTab = 112;      // "Brain activity"
constexpr unsigned kStringDrivesTab = 113;     // "Drives and needs"
constexpr unsigned kStringToolName = 114;      // "Health Kit"
constexpr unsigned kStringToolHelp = 115;      // "Initial monitoring"

// Timers
constexpr unsigned kTimerStartup = 1;
constexpr unsigned kTimerPoll = 2;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kPollMs = 500;

constexpr unsigned kCommandBufferBytes = 0x1000;

// The page area the kit opens at; the 1996 pages' 255 x 191 dialog units are
// the smallest it can be.
constexpr int kDefaultPageWidthDlu = 330;
constexpr int kDefaultPageHeightDlu = 250;

// System menu item: Always on top (the original's menu 143, never loaded).
constexpr unsigned kSysCommandOnTop = 0x0020;

// Controls made in code.
enum : unsigned {
    kControlVitals = 3000,
    kControlDrives,
    kControlLobes,
};

} // namespace health
