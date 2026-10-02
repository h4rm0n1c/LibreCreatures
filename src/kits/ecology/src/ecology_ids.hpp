#pragma once

// Ecology Kit resource ids (../resources/lc_resources.rcinc).

namespace ecology {

constexpr unsigned kIconKit = 128;
constexpr unsigned kDialogPage = 300;
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringKitName = 101;       // "Ecology Kit" (title)
constexpr unsigned kStringPausedSuffix = 108;  // " - Paused"
constexpr unsigned kStringToolName = 200;      // "Ecology Kit"
constexpr unsigned kStringToolHelp = 201;
constexpr unsigned kStringMapTab = 202;

constexpr unsigned kTimerStartup = 1;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kTimerPoll = 2;
constexpr unsigned kPollMs = 2000;
constexpr unsigned kCommandBufferBytes = 0x8000;

constexpr int kDefaultPageWidthDlu = 560;
constexpr int kDefaultPageHeightDlu = 130;

enum : unsigned {
    kControlMap = 3000,
    kControlLayerLabel,
    kControlLayer,
    kControlLegend,
};

} // namespace ecology
