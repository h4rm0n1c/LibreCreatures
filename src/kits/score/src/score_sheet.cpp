// The Score Kit's sheet: start-up, preferences, polling and pausing.  The
// original's behaviour is in ../ORIGINAL.md; deviations are marked
// "Fix (bug N)".

#include "score.hpp"
#include "score_ids.hpp"

namespace score {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Score Kit";
constexpr char kVersion[] = "1.0";

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

BEGIN_MESSAGE_MAP(ScoreSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// The original opened on a cover page (a picture) with the score page
// second; this build has only the score page.  Its looping start-up sound
// ("kitp") and the rest of its sound player are left out.
ScoreSheet::ScoreSheet(CFont& default_font)
    : KitSheet(kStringKitName, kStringPausedSuffix),
      default_font_(default_font),
      page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&page_);
}

ScoreSheet::~ScoreSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool ScoreSheet::create_window() {
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

// ApplyDefaultFont @ 0x00401c30 and the registry handler's OpenKeys
// @ 0x00405120.  Fix (bug 8): only the per-user key is needed; the original
// also required an HKLM "Score Kit" key that installs do not create.
int ScoreSheet::OnCreate(LPCREATESTRUCT create) {
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

BOOL ScoreSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));

    // Fix (bug 12): "Always on top" on the system menu.  The original had
    // only the saved setting; its menu resource for it was never loaded.
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &top"));
    }
    load_preferences();
    return result;
}

// InitializeWindowState @ 0x00401e00.
void ScoreSheet::load_preferences() {
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
    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top_ != 0 ? &wndTopMost : &wndNoTopMost,
                 left < 0 ? 0 : left, top < 0 ? 0 : top, 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop,
                            always_on_top_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    }
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
}

// PersistWindowLocationAndShutdown @ 0x00401c80.  "Page" is not written:
// with the cover gone there is one page.
void ScoreSheet::save_preferences() {
    if (registry_ == nullptr) {
        return;
    }
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
}

// InitializeDdeConnection @ 0x00401fa0.  The original also asked for the
// selected creature's owner and name here (and, on control states 6 and 7,
// again later, with a "There is no subject" box when none was selected);
// the Score Kit never uses either, so this build does not ask.
void ScoreSheet::connect() {
    if (!connect_to_game(kCommandBufferBytes)) {
        return;
    }
    connected_ = true;
    if (c1kit::MacroTransport* game = transport()) {
        conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
    }
    refresh();
    SetTimer(kTimerTick, kTickMs, nullptr);
}

// RefreshDisplay @ 0x004084e0: the five counters in one script, then hours
// and minutes, all on one query holder (as the original, which kept one).
// A failed query keeps the previous figures.
void ScoreSheet::refresh() {
    refresh_pending_ = false;
    ticks_until_refresh_ = kRefreshTicks;
    if (!conversation_ || quitting()) {
        return;
    }
    std::string reply;
    if (conversation_->query_reusing_holder(c1kit::kMacroModeQuery,
                                            c1kit::kScoreQuery, reply)) {
        c1kit::parse_score_values(reply, state_.values);
    }
    if (conversation_->query_reusing_holder(c1kit::kMacroModeQuery,
                                            c1kit::kHourQuery, reply)) {
        c1kit::parse_first_value(reply, state_.hours);
    }
    if (conversation_->query_reusing_holder(c1kit::kMacroModeQuery,
                                            c1kit::kMinuteQuery, reply)) {
        c1kit::parse_first_value(reply, state_.minutes);
    }
    page_.show(state_);
}

void ScoreSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connect();
    } else if (timer_id == kTimerTick && !paused()) {
        // CScorePage::OnTimer @ 0x00407e90: blink the colon each second.
        // Fix (bug 7): refresh every few seconds, not every fifty.
        colon_visible_ = !colon_visible_;
        page_.set_colon_visible(colon_visible_);
        if (--ticks_until_refresh_ <= 0) {
            refresh();
        }
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// Kind 1 / code 4: the game reports a score change (NotifyDDEScoreChanged,
// UpdateWorld and the creature load/removal paths send it to slot 8; the
// payload is meaningless).  HandleScoreEvent @ 0x00402610 refreshes at once.
void ScoreSheet::on_integer_message(std::int32_t) {
    if (paused()) {
        refresh_pending_ = true;
        return;
    }
    refresh();
}

void ScoreSheet::update_pause() {
    if (paused()) {
        show_paused_title();
        return;
    }
    show_normal_title();
    if (connected_) {
        refresh();
    }
}

// HandleScoreCommand @ 0x00402660: 8 closes, 9 toggles the game's pause.
// Fix (bug 10): pausing follows only the game's pause and minimising; the
// original also paused when the window was activated from the keyboard, and
// its toggling could fall out of step after a minimise (as Observation's).
void ScoreSheet::on_control_state(std::uint8_t state) {
    if (state == c1kit::kControlStateClose) {
        PostMessage(WM_CLOSE);
        return;
    }
    if (state == c1kit::kControlStatePause) {
        game_paused_ = !game_paused_;
        update_pause();
    }
}

void ScoreSheet::OnSize(UINT type, int cx, int cy) {
    c1kitshell::KitSheet::OnSize(type, cx, cy);
    const bool minimised = type == SIZE_MINIMIZED;
    if (minimised != minimised_) {
        minimised_ = minimised;
        update_pause();
    }
}

void ScoreSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
}

void ScoreSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

// Fix (bug 8): save before "app: quit", during which the game may
// terminate the kit; the original saved only when its window was destroyed.
void ScoreSheet::before_game_quit() {
    save_preferences();
}

// HandleQuitRequest @ 0x00401d80 (WM_CLOSE).
void ScoreSheet::OnClose() {
    request_game_quit();
}

void ScoreSheet::OnDestroy() {
    KillTimer(kTimerTick);
    if (conversation_ && !quitting()) {
        conversation_->close();
    }
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace score

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_score_window(CFont& font) {
    auto* sheet = new score::ScoreSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    // The original's server identity, so the game's Tools menu opens this
    // build in its place.
    kit.identity.prog_id = "Score.OLE";
    // {3DC4BDA1-B95B-11CF-BBF2-0020AF71E433}
    const GUID clsid = {0x3dc4bda1, 0xb95b, 0x11cf,
                        {0xbb, 0xf2, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 8;
    kit.tool_value_prog_id = "Score.OLE";
    kit.tool_name_string = score::kStringToolName;
    kit.tool_help_string = score::kStringToolHelp;
    kit.ole_init_failed_string = score::kStringOleInitFailed;
    kit.create_main_window = &create_score_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
