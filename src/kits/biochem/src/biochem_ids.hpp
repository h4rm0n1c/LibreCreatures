#pragma once

// Biochemistry Kit resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace biochem {

constexpr unsigned kDialogPage = 300;  // blank page; controls made in code
constexpr unsigned kIconKit = 128;

constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringTitle = 500;          // "Biochemistry Kit - "
constexpr unsigned kStringPausedMarker = 502;   // "Paused - "
constexpr unsigned kStringToolName = 503;       // "Biochemistry Kit"
constexpr unsigned kStringToolHelp = 504;       // "Biochemistry tools"
constexpr unsigned kStringMonitorTab = 508;     // "Biochemistry"
constexpr unsigned kStringInjectTab = 509;      // "Injections"
constexpr unsigned kStringNamesTab = 510;       // "Chemical Names"

constexpr unsigned kTimerStartup = 1;
constexpr unsigned kTimerMonitor = 2;
constexpr unsigned kTimerRepeat = 3;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kMonitorMs = 1000;  // CMonitorPage's timer
constexpr unsigned kCommandBufferBytes = 0x1000;

constexpr int kDefaultPageWidthDlu = 400;
constexpr int kDefaultPageHeightDlu = 250;

// The graph's channels (CMonitorChannelRecord[32]).
constexpr int kMaxChannels = 32;

constexpr unsigned kSysCommandOnTop = 0x0020;

enum : unsigned {
    kControlFilter = 3000,
    kControlChemical,
    kControlAdd,
    kControlRemove,
    kControlClear,
    kControlSaved,
    kControlLoad,
    kControlSave,
    kControlDelete,
    kControlFollowed,
    kControlGraph,
    kControlDose,
    kControlAmount,
    kControlInject,
    kControlRepeat,
    kControlEvery,
    kControlCount,
    kControlStop,
    kControlStatus,
    kControlSearch,
    kControlNames,
    kControlName,
    kControlRename,
    kControlSaveNames,
    kControlLabel,
};

} // namespace biochem
