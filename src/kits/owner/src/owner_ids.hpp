#pragma once

// Owner's Kit resource and control ids, from the original's PE resources and
// their call sites in /C1 Kits/Owners_Kit.exe.  See ../ORIGINAL.md.

namespace owner {

// Dialogs and icon
constexpr unsigned kDialogRegister = 136;     // "Register the birth"
constexpr unsigned kDialogAlbum = 137;        // "Photo Album"
constexpr unsigned kDialogCertificate = 141;  // "Certificate"
constexpr unsigned kIconKit = 128;

// Strings
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringUnknown = 104;
constexpr unsigned kStringNameTooLong = 105;  // "...10 characters or less"
constexpr unsigned kStringTitlePaused = 106;  // "Owner's Kit... "
constexpr unsigned kStringPausedMarker = 107; // " Paused  -  "
constexpr unsigned kStringTitle = 108;        // "Owner's Kit - "
constexpr unsigned kStringUnregistered = 109;
constexpr unsigned kStringMale = 110;
constexpr unsigned kStringFemale = 111;
constexpr unsigned kStringRegisterTab = 115;
constexpr unsigned kStringAlbumTab = 116;
constexpr unsigned kStringCertificateTab = 117;
constexpr unsigned kStringToolName = 118;     // "Owner's kit"
constexpr unsigned kStringToolHelp = 119;     // "Norn details"
constexpr unsigned kStringDeletePhoto = 120;  // "Delete photograph"

// Register page (dialog 136)
constexpr unsigned kControlRegisterBirth = 1051;
constexpr unsigned kControlCreatureName = 1049;
constexpr unsigned kControlSex = 1115;
constexpr unsigned kControlAge = 1116;
constexpr unsigned kControlOwnerName = 1055;
constexpr unsigned kControlOwnerAddress = 1054;
constexpr unsigned kControlOwnerPhone = 1056;
constexpr unsigned kControlOwnerEmail = 1059;
constexpr unsigned kControlRegisterClose = 1119;

// Album page (dialog 137)
constexpr unsigned kControlAlbumPicture = 1071;
constexpr unsigned kControlSaveAs = 1066;
constexpr unsigned kControlDeletePhoto = 1067;
constexpr unsigned kControlTakePhoto = 1068;
constexpr unsigned kControlPreviousPhoto = 1072;
constexpr unsigned kControlNextPhoto = 1069;
constexpr unsigned kControlCaption = 1064;
constexpr unsigned kControlTaken = 1065;
constexpr unsigned kControlAlbumClose = 1116;

// Certificate page (dialog 141)
constexpr unsigned kControlCertificatePicture = 1105;
constexpr unsigned kControlCertificateClose = 1116;

// Art, in the game's Main Directory
constexpr char kAlbumBackdrop[] = "photograph.bmp";  // 312 x 299
constexpr char kBlankPhoto[] = "Blank.bmp";           // 125 x 145
constexpr char kCertificateBackdrop[] = "birth.bmp";  // 244 x 304
constexpr char kPaletteFile[] = "palette.dta";

// Where the photograph sits in the album picture, and the certificate text
// (measured from the 1996 kit on screen).
constexpr int kPhotoLeft = 61;
constexpr int kPhotoTop = 31;
constexpr int kCertificateTextLeft = 92;
constexpr int kCertificateNameTop = 104;
constexpr int kCertificateMotherTop = 129;
constexpr int kCertificateFatherTop = 153;
constexpr int kCertificateDateTop = 176;
constexpr int kCertificateTimeTop = 246;

// A creature name may have at most this many characters (string 105).
constexpr int kMaxCreatureName = 10;

// Timers
constexpr unsigned kTimerStartup = 1;
constexpr unsigned kTimerAge = 2;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kAgeRefreshMs = 10000;

constexpr unsigned kCommandBufferBytes = 0x1000;

// Default page area; the original's 225x223 dialog units are the minimum.
constexpr int kDefaultPageWidthDlu = 225;
constexpr int kDefaultPageHeightDlu = 223;

// System menu item: Always on top (the original's menu 143, never loaded).
constexpr unsigned kSysCommandOnTop = 0x0020;

} // namespace owner
