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
        // The classic skin: its own pages, the v1.2 kit's, and none of these.
        cover_ = std::make_unique<c1kitshell::CoverPage>(kDialogCover, kCoverPicture, kIconKit);
        classic_monitor_ = std::make_unique<ClassicMonitorPage>(*this);
        classic_inject_ = std::make_unique<ClassicInjectPage>(*this);
        classic_names_ = std::make_unique<ClassicNamesPage>(*this);
        AddPage(cover_.get());
        AddPage(classic_monitor_.get());
        AddPage(classic_inject_.get());
        AddPage(classic_names_.get());
        return;
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
    install_options(registry_, classic());
    enable_ambience(kAmbience, kAmbienceVolume);
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
    if (classic_) {
        classic_monitor_->names_changed();
        classic_inject_->names_changed();
        classic_names_->names_changed();
        return;
    }
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
    if (classic_) {
        classic_monitor_->subject_changed();
    } else {
        monitor_page_.subject_changed();
    }
}

// The classic window.  The v1.2 kit's row under the pages (Always on Top,
// Show Tooltips, the version) is gone: those are on the system menu's
// Options now, and the version is in Help > About.
void BiochemSheet::set_up_classic_window() {
    registry_->read_dword(c1kit::SettingsScope::user, "Show Tooltips", tooltips_);
    classic_monitor_->set_tooltips(tooltips_ != 0);
}

void BiochemSheet::add_kit_options(CMenu& options) {
    if (classic_) {  // the classic Biochemistry page's pointer readout
        options.AppendMenu(MF_STRING | (tooltips_ != 0 ? MF_CHECKED : 0), kOptionTooltips,
                           _T("Show tool&tips"));
    }
}

bool BiochemSheet::on_kit_option(UINT id) {
    if (id != kOptionTooltips || !classic_) {
        return false;
    }
    tooltips_ = tooltips_ != 0 ? 0 : 1;
    registry_->write_dword("Show Tooltips", tooltips_);
    classic_monitor_->set_tooltips(tooltips_ != 0);
    check_kit_option(kOptionTooltips, tooltips_ != 0);
    return true;
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
    // "Page" counts the original's cover as page 0, as the classic look's
    // pages do.
    std::uint32_t page = 1;
    registry_->read_dword(c1kit::SettingsScope::user, "Page", page);
    saved_page_ = classic_ ? static_cast<int>(page) : page > 0 ? static_cast<int>(page) - 1 : 0;
    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top() ? &wndTopMost : &wndNoTopMost, left < 0 ? 0 : left,
                 top < 0 ? 0 : top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
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
        if (classic_) {
            classic_monitor_->sample();
        } else {
            monitor_page_.sample();
        }
    } else if (timer_id == kTimerRepeat && !quitting()) {
        if (classic_) {
            classic_inject_->repeat_tick();
        } else {
            inject_page_.repeat_tick();
        }
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

void BiochemSheet::before_skin_change() {
    save_preferences();
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
    kit.original_file_name = "BiochemKit";
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
