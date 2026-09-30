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
constexpr int kDefaultPageWidthDlu = 520;
constexpr int kDefaultPageHeightDlu = 320;

// The classic look: the cover, the ambience (the 1996 kit's "kits" loop at
// -10 dB), and each medicine's liquid in the syringe (CInjectPage::
// RefreshDosageDisplay @ 0x0040aca0's table, by the medicine's place in
// injections.str; anything past it the blue).
constexpr unsigned kDialogCover = 140;
constexpr char kCoverPicture[] = "Science.bmp";
constexpr char kAmbience[] = "kits";
constexpr int kAmbienceVolume = -1000;
constexpr const char* kMedicineLiquids[] = {
    "yDosage.spr", "ODosage.spr", "BDosage.spr", "cDosage.spr", "pDosage.spr", "sDosage.spr",
    "Dosage.spr",  "BDosage.spr", "cDosage.spr", "pDosage.spr", "ODosage.spr", "Dosage.spr",
    "pDosage.spr", "cDosage.spr", "BDosage.spr", "ODosage.spr", "Dosage.spr"};
constexpr char kOtherMedicineLiquid[] = "BDosage.spr";
constexpr unsigned kTimerSyringe = 21;  // on the Injections page


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
    kControlWiring,
    kControlWiringLabel,
    kControlGridPositions,
    kControlDecisionBars,
    kControlDecisionValue,
    kControlMedicineList,
    kControlDose,
    kControlDoseLabel,
    kControlInject,
    kControlMedicineLevel,
    kControlHint,
    kControlThemesGroup,  // the classic look's
    kControlSyringe,
};

// At most this many chemicals on the graph at once (the 1996 kit had four).
constexpr int kMaxTrackedChemicals = 16;

} // namespace science
