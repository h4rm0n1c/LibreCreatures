// The Observation Kit's sheet: start-up, preferences and pausing.  The
// original's behaviour is in ../ORIGINAL.md; deviations are marked
// "Fix (bug N)".

#include "observation.hpp"
#include "observation_ids.hpp"

#include <limits>

namespace observation {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Overview Kit";
constexpr char kVersion[] = "1.0";
constexpr char kOverviewQuery[] = "inst,dde: getb ovvd,endm";

// Registry value layouts: "Location" is the original's; "Size" is new.
struct WindowLocation {
    std::int32_t left;
    std::int32_t top;
};
struct WindowSize {
    std::int32_t width;
    std::int32_t height;
};

} // namespace

BEGIN_MESSAGE_MAP(ObservationSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// COverviewSheet::Constructor @ 0x004030b0 (defaults in AlertSettings).
ObservationSheet::ObservationSheet(CFont& default_font)
    : KitSheet(kStringKitName, kStringPausedSuffix),
      default_font_(default_font),
      overview_(*this),
      options_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    // The original opened on a cover page (a picture) and added these two
    // once connected; this build has no cover and opens on Details.
    AddPage(&overview_);
    AddPage(&options_);
}

ObservationSheet::~ObservationSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

// Fix (bug 3): a sizing border and maximise box.
bool ObservationSheet::create_window() {
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

// InitializeInstance @ 0x00403910: the startup font and the kit's own
// registry key (HKCU); window creation fails if it cannot be opened.
int ObservationSheet::OnCreate(LPCREATESTRUCT create) {
    if (c1kitshell::KitSheet::OnCreate(create) == -1) {
        return -1;
    }
    SendMessage(WM_SETFONT,
                reinterpret_cast<WPARAM>(default_font_.GetSafeHandle()), 0);
    registry_ = c1kit::open_kit_settings(kCompany, kProduct, kVersion,
                                         c1kit::SettingsOpenPolicy::user_key_only);
    if (registry_ == nullptr || !registry_->is_open()) {
        return -1;
    }
    return 0;
}

BOOL ObservationSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    load_preferences();
    return result;
}

// LoadPreferences @ 0x00403ae0.
void ObservationSheet::load_preferences() {
    WindowSize size = {};
    if (registry_->read_binary(c1kit::SettingsScope::user, "Size", &size,
                               sizeof(size)) &&
        size.width > 0 && size.height > 0) {
        const int screen_x = GetSystemMetrics(SM_CXSCREEN);
        const int screen_y = GetSystemMetrics(SM_CYSCREEN);
        set_window_size(CSize(size.width < screen_x ? size.width : screen_x,
                              size.height < screen_y ? size.height : screen_y));
    }

    CRect window;
    GetWindowRect(&window);
    const int max_left = GetSystemMetrics(SM_CXSCREEN) - window.Width();
    const int max_top = GetSystemMetrics(SM_CYSCREEN) - window.Height();
    WindowLocation location = {0x100, 0x80};
    if (!registry_->read_binary(c1kit::SettingsScope::user, "Location",
                                &location, sizeof(location))) {
        location = {0x100, 0x80};
    }
    registry_->read_dword(c1kit::SettingsScope::user, "On Top", always_on_top_);
    registry_->read_dword(c1kit::SettingsScope::user, "Alert near Death",
                          settings_.alert_near_death);
    registry_->read_dword(c1kit::SettingsScope::user, "Alert on Pregnancy",
                          settings_.alert_on_pregnancy);
    registry_->read_dword(c1kit::SettingsScope::user, "Alert on Birth",
                          settings_.alert_on_birth);
    registry_->read_dword(c1kit::SettingsScope::user, "Message Box",
                          settings_.message_box);
    registry_->read_dword(c1kit::SettingsScope::user, "Warn Level",
                          settings_.warn_level);
    // "Page" counts the original's cover as page 0 (the key is shared with
    // the 1996 kit), so Details is 1 and Options 2.
    std::uint32_t page = 1;
    registry_->read_dword(c1kit::SettingsScope::user, "Page", page);
    // Fix (bug 12): applied once connected (initialize_pages); the original
    // applied it while only the cover had been added, so it never worked.
    saved_page_ = page > 0 ? static_cast<int>(page) - 1 : 0;

    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top_ != 0 ? &wndTopMost : &wndNoTopMost,
                 left < 0 ? 0 : left, top < 0 ? 0 : top, 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
}

// SavePreferences @ 0x00403980 (the sheet's OnDestroy).
void ObservationSheet::save_preferences() {
    if (registry_ == nullptr) {
        return;
    }
    // The restored rectangle, so closing while minimised or maximised keeps
    // the normal position and size.
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(&placement);
    const CRect window(placement.rcNormalPosition);
    const int max_left = GetSystemMetrics(SM_CXSCREEN) - window.Width();
    const int max_top = GetSystemMetrics(SM_CYSCREEN) - window.Height();
    WindowLocation location;
    location.left = window.left < 0 ? 0
                  : (max_left < window.left ? max_left : window.left);
    location.top = window.top < 0 ? 0
                 : (max_top < window.top ? max_top : window.top);
    const WindowSize size = {window.Width(), window.Height()};
    registry_->write_binary("Location", &location, sizeof(location));
    registry_->write_binary("Size", &size, sizeof(size));
    registry_->write_dword("On Top", always_on_top_);
    registry_->write_dword("Alert near Death", settings_.alert_near_death);
    registry_->write_dword("Alert on Pregnancy", settings_.alert_on_pregnancy);
    registry_->write_dword("Alert on Birth", settings_.alert_on_birth);
    registry_->write_dword("Message Box", settings_.message_box);
    registry_->write_dword("Warn Level", settings_.warn_level);
    registry_->write_dword("Page",
                           static_cast<std::uint32_t>(GetActiveIndex() + 1));
}

void ObservationSheet::OnTimer(UINT_PTR timer_id) {
    // COverviewSheet::OnTimer @ 0x00403c80, and the list page's poll timer
    // (OnTimer @ 0x00405590), which now lives here.
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        initialize_pages();
    } else if (timer_id == kTimerPoll && !paused()) {
        poll();
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// LoadOverviewData @ 0x004055f0.
bool ObservationSheet::load_overview() {
    c1kit::MacroTransport* game = transport();
    if (game == nullptr || quitting()) {
        return false;
    }
    if (!conversation_) {
        conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
    }
    // Fix (bug 4): one query holder for the kit's lifetime instead of a
    // Destroy/Create pair on every poll.
    std::string reply;
    if (!conversation_->query_reusing_holder(c1kit::kMacroModeQuery,
                                             kOverviewQuery, reply)) {
        return false;
    }
    // Fix (bug 2): every creature, not the first 13.
    records_ = c1kit::parse_overview(reply,
                                     (std::numeric_limits<std::size_t>::max)());
    return true;
}

void ObservationSheet::poll() {
    if (!load_overview()) {
        return;
    }
    // Fix (bug 15): alerts follow the saved settings; the original read the
    // Options page's controls, which held their defaults until that tab had
    // been opened.
    const std::vector<Alert> alerts =
        monitor_.update(records_, settings_, GetTickCount());
    overview_.show(records_, settings_);
    raise_alerts(alerts);
}

// Fix (bug 10): with "Use a Message Box" off (the default) the original did
// nothing at all; this build flashes the kit and beeps instead.
void ObservationSheet::raise_alerts(const std::vector<Alert>& alerts) {
    for (const Alert& alert : alerts) {
        const unsigned text_id = alert.type == kAlertPregnancy
                                     ? kStringAlertPregnant
                                 : alert.type == kAlertBirth
                                     ? kStringAlertBirth
                                     : kStringAlertDeath;
        const CString text =
            CString(alert.name.c_str()) + c1kitshell::load_string(text_id);
        if (settings_.message_box != 0) {
            AlertWindow::open(*this, text, alert.type);
        } else {
            FLASHWINFO flash = {sizeof(flash), GetSafeHwnd(),
                                FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
            ::FlashWindowEx(&flash);
            MessageBeep(MB_ICONASTERISK);
        }
    }
}

// InitializeSheetPages @ 0x00403660: connect (the pages already exist).
// With no connection the list stays empty.
void ObservationSheet::initialize_pages() {
    if (!connect_to_game(kCommandBufferBytes)) {
        return;
    }
    connected_ = true;
    if (saved_page_ > 0 && saved_page_ < GetPageCount()) {
        SetActivePage(saved_page_);
    }
    layout_pages();
    if (!paused()) {
        poll();
    }
    SetTimer(kTimerPoll, kPollIntervalMs, nullptr);
}

// ApplyOverviewAlwaysOnTop @ 0x00403cb0.
void ObservationSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Resuming polls at once rather than waiting for the next tick.
void ObservationSheet::update_pause() {
    if (paused()) {
        show_paused_title();
        return;
    }
    show_normal_title();
    if (connected_) {
        poll();
    }
}

// HandleHotkeyCommand @ 0x004037f0: control-state broadcasts.  State 9 is
// sent both when the game pauses and when it resumes.
void ObservationSheet::on_control_state(std::uint8_t state) {
    if (state == c1kit::kControlStateClose) {
        PostMessage(WM_CLOSE);
        return;
    }
    if (state == c1kit::kControlStatePause) {
        game_paused_ = !game_paused_;
        update_pause();
    }
}

// HandleWindowDisplayState @ 0x00404050: minimising pauses, restoring
// resumes.
void ObservationSheet::OnSize(UINT type, int cx, int cy) {
    c1kitshell::KitSheet::OnSize(type, cx, cy);
    const bool minimised = type == SIZE_MINIMIZED;
    if (minimised != minimised_) {
        minimised_ = minimised;
        update_pause();
    }
}

// The original saved only in WM_DESTROY, which never runs when the game
// terminates the kit while handling "app: quit".
void ObservationSheet::before_game_quit() {
    save_preferences();
}

// RequestGameQuitThunk @ 0x00403cf0 (WM_CLOSE).
void ObservationSheet::OnClose() {
    request_game_quit();
}

void ObservationSheet::OnDestroy() {
    KillTimer(kTimerPoll);
    if (conversation_ && !quitting()) {
        conversation_->close();
    }
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

// OnSysCommand @ 0x004024c0: IDM_ABOUTBOX (0x10) shows the About box.
void ObservationSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == 0x10) {
        AboutDialog about;
        about.DoModal();
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

} // namespace observation
