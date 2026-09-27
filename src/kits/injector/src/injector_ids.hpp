#pragma once

// Object Injector resource ids, from the original's PE resources.  See
// ../ORIGINAL.md.

namespace injector {

constexpr unsigned kDialogPage = 300;  // blank page; controls made in code
constexpr unsigned kIconKit = 128;
constexpr unsigned kDialogCover = 140;  // classic only

// The classic look: its cover picture and ambience (the 1996 kit's "kits"
// loop at -10 dB), and menu 130's commands, with Mute added.
constexpr char kCoverPicture[] = "jigsaw.bmp";
constexpr char kPictureBackdrop[] = "Alima.bmp";  // behind the COB's picture (CDibView)
constexpr char kAmbience[] = "kits";
constexpr int kAmbienceVolume = -1000;
constexpr unsigned kCommandSetFolder = 32771;
constexpr unsigned kCommandRefresh = 32772;
constexpr unsigned kCommandOnTop = 32773;
constexpr unsigned kCommandHide = 32774;
constexpr unsigned kCommandIgnoreAmount = 32775;
constexpr unsigned kCommandAllowWithout = 32776;
constexpr unsigned kCommandAbout = 57664;
constexpr unsigned kCommandClose = 57665;
constexpr unsigned kCommandMute = 0x8101;

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
    // The classic look: the Analysis page's labels and buttons, and the
    // sheet's bottom row.
    kControlListLabel,
    kControlResultsLabel,
    kControlAnalysisBrowse,
    kControlAnalysisRefresh,
    kControlAnalysisFolder,
    kControlOnTopCheck,
    kControlHide,
    kControlCloseKit,
};

} // namespace injector
