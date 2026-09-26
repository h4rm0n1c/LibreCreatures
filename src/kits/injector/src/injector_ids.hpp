#pragma once

// Object Injector resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace injector {

constexpr unsigned kDialogPage = 300;  // blank page; controls made in code
constexpr unsigned kIconKit = 128;

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringTitle = 500;         // "Injector Kit - "
constexpr unsigned kStringNoSubject = 502;     // "No Subject"
constexpr unsigned kStringPausedMarker = 503;  // "Paused - "
constexpr unsigned kStringToolName = 504;      // "Injector Kit"
constexpr unsigned kStringToolHelp = 505;      // "Inject and remove agents"
constexpr unsigned kStringCobsTab = 507;       // "COBs"
constexpr unsigned kStringAnalysisTab = 508;   // "Analysis"
constexpr unsigned kStringInfinite = 509;
constexpr unsigned kStringNoneLeft = 510;      // "There are no more of this agent left."
constexpr unsigned kStringExpired = 511;       // "EXPIRED"

constexpr unsigned kTimerStartup = 1;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kCommandBufferBytes = 0x1000;

constexpr int kDefaultPageWidthDlu = 360;
constexpr int kDefaultPageHeightDlu = 280;

constexpr unsigned kSysCommandOnTop = 0x0020;

enum : unsigned {
    kControlFind = 3000,
    kControlList,
    kControlPicture,
    kControlDescription,
    kControlInject,
    kControlRemove,
    kControlRefresh,
    kControlBrowse,
    kControlQuantity,
    kControlFolder,
    kControlIgnoreAmount,
    kControlAllowWithout,
    kControlTree,
    kControlFindLabel,
};

} // namespace injector
