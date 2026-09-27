#pragma once

// Health Kit resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace health {

constexpr unsigned kDialogPage = 200;  // blank page; controls made in code
constexpr unsigned kIconKit = 128;

// The classic look: the 1996 pages' templates, their picture control, the
// cover picture, and the ambience (the 1996 kit's "kith" loop at -10 dB).
constexpr unsigned kDialogClassicBrain = 134;
constexpr unsigned kDialogClassicFitness = 138;
constexpr unsigned kDialogClassicDrives = 139;
constexpr unsigned kDialogCover = 140;
constexpr unsigned kDialogClassicDoctor = 142;
constexpr unsigned kControlClassicPicture = 1019;
constexpr char kCoverPicture[] = "health.bmp";
constexpr char kDoctorBoard[] = "Black.bmp";
constexpr char kAmbience[] = "kith";
constexpr int kAmbienceVolume = -1000;

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
