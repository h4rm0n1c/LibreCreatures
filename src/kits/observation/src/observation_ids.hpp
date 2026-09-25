#pragma once

// Observation Kit resource and control ids, from the original's PE resources
// and their call sites in /C1 Kits/observation.exe.  See ../ORIGINAL.md.

namespace observation {

// Dialogs
constexpr unsigned kDialogAbout = 100;
constexpr unsigned kDialogCover = 129;
constexpr unsigned kDialogOverview = 130;  // caption "Details"
constexpr unsigned kDialogOptions = 143;
constexpr unsigned kDialogAlert = 144;

// Bitmaps and icon
constexpr unsigned kIconKit = 128;
constexpr unsigned kBitmapCover = 136;
constexpr unsigned kBitmapListIcons = 142;       // blank, egg, grave, sex symbol
constexpr unsigned kBitmapAlertPregnancy = 147;  // 0x93
constexpr unsigned kBitmapAlertDeath = 148;      // 0x94
constexpr unsigned kBitmapAlertBirth = 149;      // 0x95

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringPleaseWait = 102;      // 0x66, sheet caption
constexpr unsigned kStringKitName = 103;         // 0x67, window title
constexpr unsigned kStringColumnName = 104;
constexpr unsigned kStringColumnAge = 105;
constexpr unsigned kStringColumnPregnant = 106;
constexpr unsigned kStringColumnLifeForce = 107;
constexpr unsigned kStringColumnMedical = 108;
constexpr unsigned kStringColumnMoniker = 109;
constexpr unsigned kStringColumnSex = 110;
constexpr unsigned kStringAlertPregnant = 111;
constexpr unsigned kStringAlertBirth = 112;
constexpr unsigned kStringAlertDeath = 113;
constexpr unsigned kStringPausedSuffix = 116;    // 0x74, " - Paused"
constexpr unsigned kStringToolName = 117;        // 0x75
constexpr unsigned kStringToolHelp = 118;        // 0x76
constexpr unsigned kStringSexMale = 119;         // 0x77, "M"
constexpr unsigned kStringSexFemale = 120;       // 0x78, "F"

// Controls
constexpr unsigned kControlOverviewList = 1001;
constexpr unsigned kControlAboutText = 1003;
constexpr unsigned kControlOnTop = 1006;
constexpr unsigned kControlAlertNearDeath = 1007;
constexpr unsigned kControlAlertPregnancy = 1008;
constexpr unsigned kControlAlertBirth = 1009;
constexpr unsigned kControlMessageBox = 1010;
constexpr unsigned kControlWarnSpin = 1011;
constexpr unsigned kControlWarnLevel = 1012;
constexpr unsigned kControlAbout = 1013;
constexpr unsigned kControlClose = 1014;
constexpr unsigned kControlAlertText = 1018;

// Timers
constexpr unsigned kTimerStartup = 3;      // sheet, 30 ms, once
constexpr unsigned kTimerPoll = 5;         // overview page, 3000 ms
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kPollIntervalMs = 3000;
// How often a near-death alert repeats while a creature stays below the warn
// level (the original's per-row 30-second re-arm timers).
constexpr unsigned kNearDeathRepeatMs = 30000;

// The original's command buffer (InitializeSheetPages @ 0x00403660).
constexpr unsigned kCommandBufferBytes = 0x400;

// Default page area.  The original's pages are 244x106 dialog units and the
// window could not be resized (bug 3); that size is now the minimum.
constexpr int kDefaultPageWidthDlu = 340;
constexpr int kDefaultPageHeightDlu = 180;

} // namespace observation
