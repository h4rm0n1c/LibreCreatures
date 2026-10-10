#pragma once

// Lab Kit resource ids (../resources/lc_resources.rcinc).

namespace labkit {

constexpr unsigned kIconKit = 128;
constexpr unsigned kDialogPage = 300;
constexpr unsigned kStringOleInitFailed = 100;
constexpr unsigned kStringKitName = 101;       // "Lab Kit" (title)
constexpr unsigned kStringPausedSuffix = 108;  // " - Paused"
constexpr unsigned kStringToolName = 200;      // "Lab Kit"
constexpr unsigned kStringToolHelp = 201;
constexpr unsigned kStringMessagesTab = 202;

constexpr unsigned kTimerStartup = 1;
constexpr unsigned kStartupDelayMs = 30;
constexpr unsigned kCommandBufferBytes = 0x8000;

// The pipe thread hands each request to the window thread with this message
// (wparam: the PipeRequest), and the window runs deferred actions (quit,
// die) with the next one, after the reply has gone.
constexpr unsigned kMessagePipeRequest = WM_APP + 1;
constexpr unsigned kMessageDeferred = WM_APP + 2;

constexpr int kDefaultPageWidthDlu = 320;
constexpr int kDefaultPageHeightDlu = 160;

enum : unsigned {
    kControlMessages = 3000,
};

} // namespace labkit
