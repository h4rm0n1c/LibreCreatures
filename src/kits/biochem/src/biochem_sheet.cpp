// The Biochemistry Kit's sheet: the subject, the chemical names, timers and
// preferences.  The original's behaviour is in ../ORIGINAL.md.

#include "biochem.hpp"
#include "biochem_ids.hpp"

#include <fstream>
#include <iterator>

namespace biochem {
namespace {

// CRegistryHandler (the v1.2 kit's own key).
constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures\\Biochemistry Kit";
constexpr char kVersion[] = "1.2";

struct WindowLocation {
    std::int32_t left;
    std::int32_t top;
};
struct WindowSize {
    std::int32_t width;
    std::int32_t height;
};

} // namespace

BEGIN_MESSAGE_MAP(BiochemSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
    ON_BN_CLICKED(kControlOnTopCheck, &BiochemSheet::OnOnTopClicked)
    ON_BN_CLICKED(kControlTooltipsCheck, &BiochemSheet::OnTooltipsClicked)
    ON_BN_CLICKED(kControlMuteCheck, &BiochemSheet::OnMuteClicked)
END_MESSAGE_MAP()

BiochemSheet::BiochemSheet(CFont& default_font)
    : KitSheet(kStringToolName, kStringPausedMarker),
      default_font_(default_font),
      monitor_page_(*this),
      inject_page_(*this),
      names_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    // The classic look: the original beside this one, and its cover picture
    // in the game's folder.
    classic_ = c1kitshell::ClassicArt::find({}, {}, {kIconKit});
    if (classic_ && ::GetFileAttributesA(game_file(kCoverPicture).c_str()) == INVALID_FILE_ATTRIBUTES) {
        classic_.reset();
    }
    if (classic_) {
        cover_ = std::make_unique<c1kitshell::CoverPage>(kDialogCover, kCoverPicture);
        AddPage(cover_.get());
    }
    AddPage(&monitor_page_);
    AddPage(&inject_page_);
    AddPage(&names_page_);
}

BiochemSheet::~BiochemSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool BiochemSheet::create_window() {
    const DWORD sizing = classic_ ? 0 : WS_MAXIMIZEBOX | WS_THICKFRAME;  // classic: fixed
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | sizing,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

std::string BiochemSheet::game_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting("Main Directory");
    return std::string(CStringA(directory)) + name;
}

// BuildSavedBiochemDataPath: beside the executable.
std::string BiochemSheet::kit_file(const std::string& name) const {
    char path[MAX_PATH] = {};
    ::GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string folder(path);
    const std::size_t slash = folder.find_last_of("\\/");
    folder = slash == std::string::npos ? std::string() : folder.substr(0, slash + 1);
    return folder + name;
}

int BiochemSheet::OnCreate(LPCREATESTRUCT create) {
    EnableStackedTabs(FALSE);
    if (c1kitshell::KitSheet::OnCreate(create) == -1) {
        return -1;
    }
    SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(default_font_.GetSafeHandle()), 0);
    registry_ = c1kit::open_kit_settings(kCompany, kProduct, kVersion,
                                         c1kit::SettingsOpenPolicy::user_key_only);
    if (registry_ == nullptr || !registry_->is_open()) {
        return -1;
    }
    std::ifstream in(game_file(c1kit::kAllChemicalsFileName), std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    if (bytes.empty() || !c1kit::parse_chemical_names(bytes, chemical_names_)) {
        AfxMessageBox(_T("Cannot open allchemicals.str"));
        chemical_names_.assign(c1kit::kChemicalCount, std::string());
    }
    return 0;
}

BOOL BiochemSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    if (classic_) {
        set_up_classic_window();
    } else {
        enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    }
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &Top"));
    }
    load_preferences();
    SetWindowText(c1kitshell::load_string(kStringToolName));
    return result;
}

// CChemicalsPage::SaveChemicalNames @ 0x00409580, written via a temporary
// file so a failed write leaves the old one.
bool BiochemSheet::save_chemical_names() {
    const std::string path = game_file(c1kit::kAllChemicalsFileName);
    const std::string temporary = path + ".new";
    const std::vector<std::uint8_t> bytes = c1kit::serialize_chemical_names(chemical_names_);
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            AfxMessageBox(_T("Cannot write allchemicals.str"));
            return false;
        }
    }
    if (!::MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        AfxMessageBox(_T("Cannot write allchemicals.str"));
        return false;
    }
    return true;
}

void BiochemSheet::names_changed() {
    monitor_page_.names_changed();
    inject_page_.names_changed();
}

bool BiochemSheet::query(const std::string& script, std::string& reply) {
    reply.clear();
    return conversation_ && !quitting() &&
           conversation_->query_reusing_holder(c1kit::kMacroModeQuery, script.c_str(), reply);
}

void BiochemSheet::take_subject() {
    subject_present_ = false;
    subject_name_.clear();
    std::string reply;
    int owner = 0;
    if (query("dde: putv ownr,endm", reply) && c1kit::parse_first_value(reply, owner) && owner != 0) {
        subject_present_ = true;
        if (query("dde: getb cnam,endm", reply)) {
            subject_name_ = reply.substr(0, reply.find('|'));
        }
    }
    SetWindowText(c1kitshell::load_string(kStringTitle) +
                  (subject_present_ ? CString(subject_name_.c_str()) : CString(_T("no creature selected"))));
    monitor_page_.subject_changed();
}

// The classic window: the v1.2 kit's row under the pages -- Always on Top,
// Show Tooltips (with the Biochemistry page) and the version -- with Mute
// ambient sound added.
void BiochemSheet::set_up_classic_window() {
    CWnd* tabs = GetTabControl();
    CRect tab_rect;
    tabs->GetWindowRect(&tab_rect);
    ScreenToClient(&tab_rect);
    CRect row(0, 0, 60, 10);
    CRect gap(0, 0, 4, 4);
    ::MapDialogRect(GetActivePage()->GetSafeHwnd(), &row);
    ::MapDialogRect(GetActivePage()->GetSafeHwnd(), &gap);
    const int top = tab_rect.bottom + gap.Height();
    CWnd* parent = this;
    CFont* font = GetFont();
    int x = tab_rect.left;
    const auto make_check = [&](CButton& check, LPCTSTR label, UINT id) {
        CClientDC dc(this);
        CFont* previous = dc.SelectObject(font);
        const int width = dc.GetTextExtent(label).cx + row.Height() + 8;
        dc.SelectObject(previous);
        check.Create(label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                     CRect(CPoint(x, top), CSize(width, row.Height())), parent, id);
        check.SetFont(font);
        x += width + gap.Width() * 2;
    };
    make_check(on_top_check_, _T("Always on Top"), kControlOnTopCheck);
    make_check(tooltips_check_, _T("Show Tooltips"), kControlTooltipsCheck);
    make_check(mute_check_, _T("Mute ambient sound"), kControlMuteCheck);
    version_.Create(_T("v1.2"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
                    CRect(CPoint(tab_rect.right - row.Width(), top), row.Size()), parent,
                    kControlVersion);
    version_.SetFont(font);
    registry_->read_dword(c1kit::SettingsScope::user, "Show Tooltips", tooltips_);
    tooltips_check_.SetCheck(tooltips_ != 0 ? BST_CHECKED : BST_UNCHECKED);
    monitor_page_.set_tooltips(tooltips_ != 0);
    show_tooltips_check(false);  // the cover comes first
    CRect window;
    GetWindowRect(&window);
    SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() + row.Height() + gap.Height() * 2,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    enable_ambience(registry_, kAmbience, kAmbienceVolume, false);
    mute_check_.SetCheck(ambience_muted() ? BST_CHECKED : BST_UNCHECKED);
}

void BiochemSheet::show_tooltips_check(bool shown) {
    if (tooltips_check_.GetSafeHwnd() != nullptr) {
        tooltips_check_.ShowWindow(shown ? SW_SHOW : SW_HIDE);
    }
}

void BiochemSheet::OnOnTopClicked() {
    set_always_on_top(on_top_check_.GetCheck() == BST_CHECKED);
}

void BiochemSheet::OnTooltipsClicked() {
    tooltips_ = tooltips_check_.GetCheck() == BST_CHECKED ? 1 : 0;
    registry_->write_dword("Show Tooltips", tooltips_);
    monitor_page_.set_tooltips(tooltips_ != 0);
}

void BiochemSheet::OnMuteClicked() {
    set_ambience_muted(mute_check_.GetCheck() == BST_CHECKED);
}

void BiochemSheet::load_preferences() {
    WindowSize size = {};
    if (!classic_ &&
        registry_->read_binary(c1kit::SettingsScope::user, "Size", &size, sizeof(size)) &&
        size.width > 0 && size.height > 0) {
        set_window_size(CSize(size.width, size.height));
    }
    CRect window;
    GetWindowRect(&window);
    const int max_left = GetSystemMetrics(SM_CXSCREEN) - window.Width();
    const int max_top = GetSystemMetrics(SM_CYSCREEN) - window.Height();
    WindowLocation location = {0x100, 0x80};
    if (!registry_->read_binary(c1kit::SettingsScope::user, "Location", &location, sizeof(location))) {
        location = {0x100, 0x80};
    }
    registry_->read_dword(c1kit::SettingsScope::user, "Always on Top", always_on_top_);
    // "Page" counts the original's cover as page 0, as the classic look's
    // pages do.
    std::uint32_t page = 1;
    registry_->read_dword(c1kit::SettingsScope::user, "Page", page);
    saved_page_ = classic_ ? static_cast<int>(page) : page > 0 ? static_cast<int>(page) - 1 : 0;
    if (on_top_check_.GetSafeHwnd() != nullptr) {
        on_top_check_.SetCheck(always_on_top_ != 0 ? BST_CHECKED : BST_UNCHECKED);
    }
    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top_ != 0 ? &wndTopMost : &wndNoTopMost, left < 0 ? 0 : left,
                 top < 0 ? 0 : top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, always_on_top_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    }
    if (saved_page_ > 0 && saved_page_ < GetPageCount()) {
        SetActivePage(saved_page_);
        layout_pages();
    }
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
}

void BiochemSheet::save_preferences() {
    if (registry_ == nullptr || GetSafeHwnd() == nullptr) {
        return;
    }
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(&placement);
    const CRect window(placement.rcNormalPosition);
    const WindowLocation location = {window.left < 0 ? 0 : window.left, window.top < 0 ? 0 : window.top};
    const WindowSize size = {window.Width(), window.Height()};
    registry_->write_binary("Location", &location, sizeof(location));
    if (!classic_) {
        registry_->write_binary("Size", &size, sizeof(size));  // the classic window is fixed
    }
    registry_->write_dword("Always on Top", always_on_top_);
    registry_->write_dword("Page",
                           static_cast<std::uint32_t>(GetActiveIndex() + (classic_ ? 0 : 1)));
}

// The graph samples every second whichever page is showing, so its history
// has no gaps; a repeat injection runs on its own timer.
void BiochemSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connected_ = connect_to_game(kCommandBufferBytes);
        if (connected_ && transport() != nullptr) {
            conversation_ = std::make_unique<c1kit::MacroConversation>(*transport());
            take_subject();
            SetTimer(kTimerMonitor, kMonitorMs, nullptr);
        }
    } else if (timer_id == kTimerMonitor && !game_paused_ && !quitting()) {
        monitor_page_.sample();
    } else if (timer_id == kTimerRepeat && !quitting()) {
        inject_page_.repeat_tick();
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

void BiochemSheet::on_control_state(std::uint8_t state) {
    switch (state) {
    case 6:
    case 7:
        take_subject();
        return;
    case c1kit::kControlStateClose:
        PostMessage(WM_CLOSE);
        return;
    case c1kit::kControlStatePause:
        game_paused_ = !game_paused_;
        return;
    default:
        return;
    }
}

void BiochemSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
    if (on_top_check_.GetSafeHwnd() != nullptr) {
        on_top_check_.SetCheck(on ? BST_CHECKED : BST_UNCHECKED);
    }
}

void BiochemSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

void BiochemSheet::before_game_quit() {
    save_preferences();
}

void BiochemSheet::OnClose() {
    request_game_quit();
}

void BiochemSheet::OnDestroy() {
    KillTimer(kTimerMonitor);
    KillTimer(kTimerRepeat);
    if (conversation_ && !quitting()) conversation_->close();
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace biochem

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_biochem_window(CFont& font) {
    auto* sheet = new biochem::BiochemSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "BiochemKit.OLE";
    // {A3F2B711-4E82-4D1A-9C63-7B8E1D5F3A20}
    const GUID clsid = {0xa3f2b711, 0x4e82, 0x4d1a,
                        {0x9c, 0x63, 0x7b, 0x8e, 0x1d, 0x5f, 0x3a, 0x20}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 1;
    kit.tool_value_prog_id = "BiochemKit.OLE";
    kit.tool_name_string = biochem::kStringToolName;
    kit.tool_help_string = biochem::kStringToolHelp;
    kit.ole_init_failed_string = biochem::kStringOleInitFailed;
    kit.create_main_window = &create_biochem_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
