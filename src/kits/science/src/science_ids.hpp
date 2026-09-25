#pragma once

// Science Kit resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace science {

// Dialogs (lc_dialogs.rcinc) and icons
constexpr unsigned kDialogPage = 200;       // blank page; controls made in code
constexpr unsigned kDialogThemeName = 201;  // "Save Theme"
constexpr unsigned kControlThemeName = 1114;
constexpr unsigned kIconKit = 128;
constexpr unsigned kBitmapReward = 181;
constexpr unsigned kBitmapPunishment = 182;

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringTitlePaused = 107;   // "Science Kit...  "
constexpr unsigned kStringPausedMarker = 108;  // " Paused - "
constexpr unsigned kStringTitle = 109;         // "Science Kit - "
constexpr unsigned kStringMale = 385;
constexpr unsigned kStringFemale = 386;
constexpr unsigned kStringGeneticsTab = 398;
constexpr unsigned kStringDecisionsTab = 399;
constexpr unsigned kStringInjectionsTab = 400;
constexpr unsigned kStringBiochemistryTab = 401;
constexpr unsigned kStringBrainTab = 402;
constexpr unsigned kStringTime = 403;
constexpr unsigned kStringToolName = 404;  // "Science Kit"
constexpr unsigned kStringToolHelp = 405;  // "Advanced monitoring"

// Timers
constexpr unsigned kTimerStartup = 1;
constexpr unsigned kTimerPoll = 2;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kPollMs = 250;

constexpr unsigned kCommandBufferBytes = 0x1000;

// The page area the kit opens at; the 1996 pages' 252 x 188 dialog units are
// the smallest it can be.
constexpr int kDefaultPageWidthDlu = 430;
constexpr int kDefaultPageHeightDlu = 300;

// System menu item: Always on top (the original's menu 143, never loaded).
constexpr unsigned kSysCommandOnTop = 0x0020;

// Controls made in code.
enum : unsigned {
    kControlGraph = 3000,
    kControlChemicalList,
    kControlThemeCombo,
    kControlSaveTheme,
    kControlDeleteTheme,
    kControlClearGraph,
    kControlDetails,
    kControlBrainGrid,
    kControlReportMode,
    kControlReportRule,
    kControlLobeList,
    kControlNeuronInfo,
    kControlDecisionBars,
    kControlDecisionValue,
    kControlMedicineList,
    kControlDose,
    kControlDoseLabel,
    kControlInject,
    kControlMedicineLevel,
    kControlHint,
};

// At most this many chemicals on the graph at once (the 1996 kit had four).
constexpr int kMaxTrackedChemicals = 16;

} // namespace science
