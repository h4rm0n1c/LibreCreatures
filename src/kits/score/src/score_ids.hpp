#pragma once

// Score ("Performance") Kit resource and control ids, from the original's PE
// resources and their call sites in /C1 Kits/Score Kit.exe.  See
// ../ORIGINAL.md.

namespace score {

// Dialog, icon and strings
constexpr unsigned kDialogScore = 138;
constexpr unsigned kIconKit = 128;
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringKitName = 101;        // "Performance Kit" (title)
constexpr unsigned kStringPausedSuffix = 108;   // " - Paused"
constexpr unsigned kStringToolName = 111;       // "Performance Kit"
constexpr unsigned kStringToolHelp = 112;       // "Scoring data"

// Controls
constexpr unsigned kControlPanel = 1140;        // icon picture
constexpr unsigned kControlPanelScroll = 1114;
constexpr unsigned kControlHatcheryEggs = 1126;
constexpr unsigned kControlNaturalEggs = 1127;
constexpr unsigned kControlPreviousNorns = 1128;
constexpr unsigned kControlCurrentNorns = 1130;
constexpr unsigned kControlBreedersScore = 1123;
constexpr unsigned kControlElapsedTime = 1124;
constexpr unsigned kControlClose = 1129;
constexpr unsigned kLabelBreedersScore = 1113;
constexpr unsigned kLabelElapsedTime = 1121;

// Art, read from the game's Main Directory.
constexpr char kDigitsFile[] = "AllNumbers.spr";   // 0-9, blank
constexpr char kIconsFile[] = "Score.spr";         // hatchery egg, natural
                                                   // egg, dead norn, norn
constexpr char kColonFile[] = "Time.spr";          // blank, colon
constexpr char kCounterBackdrop[] = "Scorebgd.bmp";
constexpr char kScoreBackdrop[] = "Brdscore.bmp";
constexpr char kPaletteFile[] = "palette.dta";
// The panel background, palette index 0xd6 (InitializeBitmapLayout
// @ 0x004081c0); the icons are drawn on the same colour.
constexpr unsigned char kPanelColourIndex = 0xd6;

// Layout of the art, in pixels (RefreshDisplay @ 0x004084e0,
// RenderWorldTime @ 0x00408980).
constexpr int kDigitStep = 10;       // digits overlap: frames are 13 wide
constexpr int kDigitWidth = 13;
constexpr int kDigitHeight = 20;
constexpr int kIconStepX = 30;
constexpr int kIconStepY = 50;
constexpr int kIconMargin = 10;

// Timers
constexpr unsigned kTimerStartup = 1;
constexpr unsigned kTimerTick = 2;   // one second: the colon blinks
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kTickMs = 1000;
// The original refreshed every 50 ticks (plus whenever the game reported a
// change); the clock then lagged by up to 50 seconds.  Five is still one
// small query every five seconds.
constexpr int kRefreshTicks = 5;

// The kit's command buffer (InitializeDdeConnection @ 0x00401fa0).
constexpr unsigned kCommandBufferBytes = 0x1000;

// Default page area; the original's 214x209 dialog units are the minimum.
constexpr int kDefaultPageWidthDlu = 300;
constexpr int kDefaultPageHeightDlu = 209;

// System menu item: Always on top (the original's menu 143, id 32771, which
// nothing in the kit loaded).
constexpr unsigned kSysCommandOnTop = 0x0020;

} // namespace score
