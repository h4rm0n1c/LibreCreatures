#pragma once

// Funeral Kit resource and control ids, from the original's PE resources and
// their call sites in /C1 Kits/Funeral Kit.exe.  See ../ORIGINAL.md.

namespace funeral {

// Dialogs and icon
constexpr unsigned kDialogMemorial = 137;   // CFuneralSheet: one dead creature
constexpr unsigned kDialogGraveyard = 182;  // CGravePage: "GraveYard"
constexpr unsigned kDialogUnmarked = 183;   // CUnMarkedPage: "UnMarked Grave"
constexpr unsigned kIconKit = 128;

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringTitle = 101;      // "Creature Graveyard"
constexpr unsigned kStringNoRecord = 104;   // the unmarked grave's tab
constexpr unsigned kStringToolName = 105;   // "Creature Graveyard"
constexpr unsigned kStringToolHelp = 107;   // "Pay your respects"

// Memorial page (dialog 137)
constexpr unsigned kControlPicture = 1071;
constexpr unsigned kControlNextPhoto = 1069;
constexpr unsigned kControlPreviousPhoto = 1072;
constexpr unsigned kControlLifeSpan = 1065;
constexpr unsigned kControlEpitaph = 1067;
constexpr unsigned kControlMakeHeadstone = 1070;
constexpr unsigned kControlClose = 1110;

// GraveYard page (dialog 182); the picture, arrows, life span and Close
// share the memorial page's ids.
constexpr unsigned kControlHeadstoneEpitaph = 1064;
constexpr unsigned kControlHeadstoneName = 1066;

// Art, in the game's Main Directory
constexpr char kGraveBackdrop[] = "GRAVE.bmp";      // 240 x 302, photo frame
constexpr char kUnknownBackdrop[] = "UNKNOWN.bmp";  // the same, silhouetted
constexpr char kHeadstoneBackdrop[] = "funeral.bmp";  // 238 x 300
constexpr char kPaletteFile[] = "palette.dta";

// Where a photograph sits in GRAVE.bmp (RenderSelectedPhotoItem @ 0x0040a2b0
// copies it to (0x3b,0x1c)-(0xb3,0xa8)).
constexpr int kPhotoLeft = 59;
constexpr int kPhotoTop = 28;

// Timers
constexpr unsigned kTimerStartup = 1;
constexpr unsigned kStartupDelayMs = 30;

constexpr unsigned kCommandBufferBytes = 0x1000;

// Default page area; the original's 215x225 dialog units are the minimum.
constexpr int kDefaultPageWidthDlu = 215;
constexpr int kDefaultPageHeightDlu = 225;

// System menu item: Always on top (the original's menu 143, never loaded).
constexpr unsigned kSysCommandOnTop = 0x0020;

} // namespace funeral
