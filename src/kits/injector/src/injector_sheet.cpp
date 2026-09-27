// The Object Injector's sheet: the COB folder and its files, settings, the
// subject, and injecting and removing.  The original's behaviour is in
// ../ORIGINAL.md; deviations are marked "Fix (bug N)".

#include "injector.hpp"
#include "injector_ids.hpp"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iterator>
#include <shlobj.h>

namespace injector {
namespace {

// CRegistryHandler (the 2.0 kit's): its own key, not the 1996 kits'.
constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures\\Injector Kit";
constexpr char kVersion[] = "2.0";

struct WindowLocation {
    std::int32_t left;
    std::int32_t top;
};
struct WindowSize {
    std::int32_t width;
    std::int32_t height;
};

bool read_file(const std::string& path, std::vector<std::uint8_t>& bytes) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool file_exists(const std::string& path) {
    const DWORD attributes = ::GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string with_slash(std::string folder) {
    if (!folder.empty() && folder.back() != '\\' && folder.back() != '/') {
        folder.push_back('\\');
    }
    return folder;
}

// BuildCobReplacementPath @ 0x00403340: the COB's own name with .rcb, then
// .RCB.
std::string removal_file(const std::string& cob_path) {
    const std::size_t dot = cob_path.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    for (const char* extension : {".rcb", ".RCB"}) {
        const std::string candidate = cob_path.substr(0, dot) + extension;
        if (file_exists(candidate)) {
            return candidate;
        }
    }
    return std::string();
}

} // namespace

BEGIN_MESSAGE_MAP(InjectorSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
    ON_WM_INITMENUPOPUP()
    ON_COMMAND(kCommandSetFolder, &InjectorSheet::OnSetFolder)
    ON_COMMAND(kCommandRefresh, &InjectorSheet::OnRefreshCobs)
    ON_COMMAND(kCommandOnTop, &InjectorSheet::OnToggleOnTop)
    ON_COMMAND(kCommandHide, &InjectorSheet::OnHide)
    ON_COMMAND(kCommandIgnoreAmount, &InjectorSheet::OnToggleIgnoreAmount)
    ON_COMMAND(kCommandAllowWithout, &InjectorSheet::OnToggleAllowWithout)
    ON_COMMAND(kCommandMute, &InjectorSheet::OnToggleMute)
    ON_COMMAND(kCommandAbout, &InjectorSheet::OnAbout)
    ON_COMMAND(kCommandClose, &InjectorSheet::OnCloseCommand)
    ON_BN_CLICKED(kControlOnTopCheck, &InjectorSheet::OnToggleOnTop)
    ON_BN_CLICKED(kControlHide, &InjectorSheet::OnHide)
    ON_BN_CLICKED(kControlCloseKit, &InjectorSheet::OnCloseCommand)
END_MESSAGE_MAP()

InjectorSheet::InjectorSheet(CFont& default_font)
    : KitSheet(kStringToolName, kStringPausedMarker),
      default_font_(default_font),
      cobs_page_(*this),
      analysis_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    // The classic look: the original beside this one, and its cover picture
    // in the game's folder.
    classic_ = c1kitshell::ClassicArt::find({}, {}, {kIconKit});
    if (classic_ && !file_exists(game_file(kCoverPicture))) {
        classic_.reset();
    }
    if (classic_) {
        // The classic skin: its own pages, the 2.0 kit's, and none of these.
        cover_ = std::make_unique<c1kitshell::CoverPage>(kDialogCover, kCoverPicture);
        cover_->m_psp.dwFlags |= PSP_USETITLE;  // the template's caption is "Injector"
        cover_->m_psp.pszTitle = _T("Cover");
        classic_cobs_ = std::make_unique<ClassicCobsPage>(*this);
        classic_analysis_ = std::make_unique<ClassicAnalysisPage>(*this);
        AddPage(cover_.get());
        AddPage(classic_cobs_.get());
        AddPage(classic_analysis_.get());
        return;
    }
    AddPage(&cobs_page_);
    AddPage(&analysis_page_);
}

InjectorSheet::~InjectorSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool InjectorSheet::create_window() {
    const DWORD sizing = classic_ ? 0 : WS_MAXIMIZEBOX | WS_THICKFRAME;  // classic: fixed
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | sizing,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

std::string InjectorSheet::game_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting("Main Directory");
    return std::string(CStringA(directory)) + name;
}

int InjectorSheet::OnCreate(LPCREATESTRUCT create) {
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
    registry_->read_dword(c1kit::SettingsScope::user, "IgnoreAmount", ignore_amount_);
    registry_->read_dword(c1kit::SettingsScope::user, "AllowWithoutSubject", allow_without_);
    // The COB folder: as set, else the game's folder (where its COBs are).
    char folder[MAX_PATH] = {};
    if (registry_->read_string(c1kit::SettingsScope::user, "CobFolder", folder, sizeof(folder)) &&
        folder[0] != '\0') {
        folder_ = with_slash(folder);
    } else {
        folder_ = game_file("");
    }
    // The palette the pictures use, the chemical names the analysis shows
    // and the game's names for classifiers.
    const CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    palette_.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\")) : palettes)) +
                  "palette.dta");
    std::vector<std::uint8_t> bytes;
    if (read_file(game_file("allchemicals.str"), bytes)) {
        c1kit::ArchiveReader reader(bytes);
        const std::uint16_t count = reader.u16();
        for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
            chemical_names_.push_back(reader.cstring());
        }
    }
    if (read_file(game_file("ClassifierNames.txt"), bytes)) {
        classifier_names_ =
            c1kit::parse_classifier_names(std::string(bytes.begin(), bytes.end()));
    }
    reload();
    return 0;
}

BOOL InjectorSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    if (classic_) {
        set_up_classic_window();
    } else {
        enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    }
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &top"));
    }
    load_preferences();
    update_title();
    return result;
}

// ReloadCobFiles @ 0x00402bc0: every *.cob in the folder, by name.  (An
// .rcb is a removal; it is found next to its COB when needed.)
void InjectorSheet::reload() {
    entries_.clear();
    WIN32_FIND_DATAA found = {};
    HANDLE search = ::FindFirstFileA((folder_ + "*.cob").c_str(), &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }
            const std::string path = folder_ + found.cFileName;
            std::vector<std::uint8_t> bytes;
            std::vector<c1kit::Cob> cobs;
            if (read_file(path, bytes) && c1kit::parse_cob_file(bytes, cobs)) {
                for (c1kit::Cob& cob : cobs) {
                    entries_.push_back(CobEntry{path, std::move(cob)});
                }
            }
        } while (::FindNextFileA(search, &found));
        ::FindClose(search);
    }
    std::sort(entries_.begin(), entries_.end(), [](const CobEntry& a, const CobEntry& b) {
        return _stricmp(a.cob.name.c_str(), b.cob.name.c_str()) < 0;
    });
    if (classic_) {
        if (classic_cobs_) classic_cobs_->cobs_changed();
        if (classic_analysis_) classic_analysis_->cobs_changed();
    } else {
        cobs_page_.cobs_changed();
        analysis_page_.cobs_changed();
    }
}

void InjectorSheet::set_folder(const std::string& folder) {
    folder_ = with_slash(folder);
    if (registry_ != nullptr) {
        registry_->write_string("CobFolder", folder_.c_str());
    }
    reload();
}

void InjectorSheet::set_ignore_amount(bool on) {
    ignore_amount_ = on ? 1 : 0;
    if (registry_ != nullptr) registry_->write_dword("IgnoreAmount", ignore_amount_);
}

void InjectorSheet::set_allow_without_subject(bool on) {
    allow_without_ = on ? 1 : 0;
    if (registry_ != nullptr) registry_->write_dword("AllowWithoutSubject", allow_without_);
}

bool InjectorSheet::expired(const c1kit::Cob& cob) const {
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_s(&local, &now);
    return c1kit::cob_expired(cob, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

// CKitSheet::ExecuteDdeScript: a holder, ExecuteMacro, destroyed.
bool InjectorSheet::run(const std::string& script) {
    c1kit::MacroTransport* game = transport();
    return connected_ && game != nullptr && !quitting() &&
           c1kit::execute_scheduled(*game, script.c_str(), false);
}

bool InjectorSheet::creature_selected() {
    std::string reply;
    int norn = 0;
    return conversation_ &&
           conversation_->query_reusing_holder(c1kit::kMacroModeQuery, "dde: putv norn,endm", reply) &&
           c1kit::parse_first_value(reply, norn) && norn != 0;
}

// InjectSelectedCob @ 0x00405e60.
bool InjectorSheet::inject(CobEntry& entry, CString& why) {
    c1kit::Cob& cob = entry.cob;
    if (expired(cob)) {
        why = c1kitshell::load_string(kStringExpired);
        return false;
    }
    // Fix (bug 1): none left means none; the original let a COB with a count
    // of 0 be injected while showing it as used up.
    if (!ignore_amount() && !cob.unlimited() && cob.quantity <= 0) {
        why = c1kitshell::load_string(kStringNoneLeft);
        return false;
    }
    if (c1kit::needs_creature(cob) && !allow_without_subject() && !creature_selected()) {
        why = _T("Select a Norn first, then inject.");
        return false;
    }
    for (const std::string& script : cob.install_scripts) {
        if (!run(script)) {
            why = _T("Failed to send a script to Creatures.");
            return false;
        }
    }
    const int count = static_cast<int>(cob.inject_scripts.size());
    if (count > 0) {
        if (cob.mode == 0) {
            // One inject script per injection, from the last back.
            int index = count - cob.next_inject - 1;
            if (index < 0) index = 0;
            run(cob.inject_scripts[static_cast<std::size_t>(index)]);
            if (!ignore_amount()) {
                ++cob.next_inject;
                if (!cob.unlimited() && cob.quantity > 0) --cob.quantity;
            }
        } else {
            for (const std::string& script : cob.inject_scripts) {
                run(script);
            }
            if (!ignore_amount()) {
                cob.next_inject = count;
                if (!cob.unlimited()) cob.quantity = 0;
            }
        }
    }
    return true;
}

// RemoveSelectedCob @ 0x00406220: its .rcb if it has one, else a removal
// made from its own scripts, after saying so.
bool InjectorSheet::remove(CobEntry& entry, CString& why) {
    const std::string rcb = removal_file(entry.path);
    std::vector<std::string> scripts;
    if (!rcb.empty()) {
        std::vector<std::uint8_t> bytes;
        std::vector<c1kit::Cob> removals;
        if (!read_file(rcb, bytes) || !c1kit::parse_cob_file(bytes, removals)) {
            why = _T("Could not open the removal file (.rcb).");
            return false;
        }
        if (AfxMessageBox(_T("Are you sure you wish to remove this COB from the world?"),
                          MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return false;
        }
        for (const c1kit::Cob& removal : removals) {
            scripts.insert(scripts.end(), removal.install_scripts.begin(),
                           removal.install_scripts.end());
            scripts.insert(scripts.end(), removal.inject_scripts.begin(),
                           removal.inject_scripts.end());
        }
    } else {
        const std::string generated = c1kit::generated_removal(entry.cob);
        if (generated.empty()) {
            why = _T("This COB creates no objects and installs no scripts, so there is nothing to remove.");
            return false;
        }
        if (AfxMessageBox(
                _T("No COB removal script (.rcb) was found, so one will be generated automatically ")
                _T("from this COB's own scripts.\n\nNOTE: This is potentially unsafe. A generated ")
                _T("removal is best-effort. It removes the objects this COB creates and the scripts ")
                _T("it installs, but it cannot know the author's exact intent, so in rare cases it may ")
                _T("remove too much or too little. A proper .rcb supplied with the COB is always ")
                _T("safer.\n\nAre you sure you wish to remove this COB from the world?"),
                MB_YESNO | MB_ICONWARNING) != IDYES) {
            return false;
        }
        scripts.push_back(generated);
    }
    for (const std::string& script : scripts) {
        if (!run(script)) {
            why = _T("Failed to send a script to Creatures.");
            return false;
        }
    }
    return true;
}

void InjectorSheet::load_preferences() {
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
    if (!registry_->read_binary(c1kit::SettingsScope::user, "Location", &location,
                                sizeof(location))) {
        location = {0x100, 0x80};
    }
    registry_->read_dword(c1kit::SettingsScope::user, "Keep on top", always_on_top_);
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

void InjectorSheet::save_preferences() {
    if (registry_ == nullptr || GetSafeHwnd() == nullptr) {
        return;
    }
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(&placement);
    const CRect window(placement.rcNormalPosition);
    const WindowLocation location = {window.left < 0 ? 0 : window.left,
                                     window.top < 0 ? 0 : window.top};
    const WindowSize size = {window.Width(), window.Height()};
    registry_->write_binary("Location", &location, sizeof(location));
    if (!classic_) {
        registry_->write_binary("Size", &size, sizeof(size));  // the classic window is fixed
    }
    registry_->write_dword("Keep on top", always_on_top_);
    registry_->write_dword("Page",
                           static_cast<std::uint32_t>(GetActiveIndex() + (classic_ ? 0 : 1)));
}

// "Injector Kit - <name>", or "No Subject" (strings 500, 502).
void InjectorSheet::update_title() {
    SetWindowText(c1kitshell::load_string(kStringTitle) +
                  (subject_name_.empty() ? c1kitshell::load_string(kStringNoSubject)
                                         : CString(subject_name_.c_str())));
}

void InjectorSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connected_ = connect_to_game(kCommandBufferBytes);
        if (connected_ && transport() != nullptr) {
            conversation_ = std::make_unique<c1kit::MacroConversation>(*transport());
            on_control_state(6);
        }
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// 6 the selection changed and 7 its name: the title follows it.
void InjectorSheet::on_control_state(std::uint8_t state) {
    if (state == 6 || state == 7) {
        subject_name_.clear();
        std::string reply;
        int owner = 0;
        if (conversation_ &&
            conversation_->query_reusing_holder(c1kit::kMacroModeQuery, "dde: putv ownr,endm", reply) &&
            c1kit::parse_first_value(reply, owner) && owner != 0 &&
            conversation_->query_reusing_holder(c1kit::kMacroModeQuery, "dde: getb cnam,endm", reply)) {
            subject_name_ = reply.substr(0, reply.find('|'));
        }
        update_title();
    } else if (state == c1kit::kControlStateClose) {
        PostMessage(WM_CLOSE);
    }
}

void InjectorSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
    if (on_top_check_.GetSafeHwnd() != nullptr) {
        on_top_check_.SetCheck(on ? BST_CHECKED : BST_UNCHECKED);
    }
}

void InjectorSheet::browse_for_folder(CWnd* owner) {
    BROWSEINFOA browse = {};
    browse.hwndOwner = owner != nullptr ? owner->GetSafeHwnd() : GetSafeHwnd();
    browse.lpszTitle = "Select the folder containing your COBs (.cob files)";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST chosen = ::SHBrowseForFolderA(&browse);
    if (chosen == nullptr) return;
    char path[MAX_PATH] = {};
    if (::SHGetPathFromIDListA(chosen, path)) {
        set_folder(path);
    }
    ::CoTaskMemFree(chosen);
}

// ---------------------------------------------------------------------------
// The classic look's window: menu 130 (with Mute ambient sound added to
// Window), the pages at the templates' 240 x 250, and the 2.0 kit's row
// under them -- Keep on top, Hide and Close.
// ---------------------------------------------------------------------------

void InjectorSheet::set_up_classic_window() {
    CRect client;
    GetClientRect(&client);
    CWnd* tabs = GetTabControl();
    CRect tab_rect = client;
    if (tabs != nullptr) {
        tabs->GetWindowRect(&tab_rect);
        ScreenToClient(&tab_rect);
    }
    classic_menu_.CreateMenu();
    CMenu file, window, advanced, help;
    file.CreatePopupMenu();
    file.AppendMenu(MF_STRING, kCommandSetFolder, _T("Set COB &Folder..."));
    file.AppendMenu(MF_STRING, kCommandRefresh, _T("&Refresh"));
    file.AppendMenu(MF_SEPARATOR);
    file.AppendMenu(MF_STRING, kCommandClose, _T("&Close"));
    window.CreatePopupMenu();
    window.AppendMenu(MF_STRING, kCommandOnTop, _T("&Keep on Top"));
    window.AppendMenu(MF_STRING, kCommandHide, _T("&Hide"));
    window.AppendMenu(MF_SEPARATOR);
    window.AppendMenu(MF_STRING, kCommandMute, _T("&Mute Ambient Sound"));
    advanced.CreatePopupMenu();
    advanced.AppendMenu(MF_STRING, kCommandIgnoreAmount, _T("&Ignore Amount Remaining"));
    advanced.AppendMenu(MF_STRING, kCommandAllowWithout, _T("&Allow Without Active Creature"));
    help.CreatePopupMenu();
    help.AppendMenu(MF_STRING, kCommandAbout, _T("&About Injector Kit..."));
    classic_menu_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(file.Detach()), _T("&File"));
    classic_menu_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(window.Detach()), _T("&Window"));
    classic_menu_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(advanced.Detach()),
                             _T("&Advanced"));
    classic_menu_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(help.Detach()), _T("&Help"));
    SetMenu(&classic_menu_);

    // The row under the pages, in dialog units as the pages are.
    CRect row(0, 0, 50, 14);
    CRect gap(0, 0, 4, 4);
    if (CPropertyPage* page = GetActivePage()) {
        ::MapDialogRect(page->GetSafeHwnd(), &row);
        ::MapDialogRect(page->GetSafeHwnd(), &gap);
    }
    const int top = tab_rect.bottom + gap.Height();
    CFont* font = GetFont();
    on_top_check_.Create(_T("Keep on top"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                         CRect(CPoint(tab_rect.left, top), CSize(row.Width() * 2, row.Height())),
                         this, kControlOnTopCheck);
    close_kit_.Create(_T("Close"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(CPoint(tab_rect.right - row.Width(), top), row.Size()), this,
                      kControlCloseKit);
    hide_.Create(_T("Hide"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                 CRect(CPoint(tab_rect.right - 2 * row.Width() - gap.Width(), top), row.Size()),
                 this, kControlHide);
    for (CButton* control : {&on_top_check_, &close_kit_, &hide_}) {
        control->SetFont(font);
    }

    // The window: the same width, tall enough for the menu, the pages and
    // the row (sized from the styles, as the menu leaves the client
    // rectangle stale until the window is next sized).
    CRect wanted(0, 0, client.Width(), top + row.Height() + gap.Height());
    ::AdjustWindowRectEx(&wanted, GetStyle(), TRUE, GetExStyle());
    SetWindowPos(nullptr, 0, 0, wanted.Width(), wanted.Height(),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    enable_ambience(registry_, kAmbience, kAmbienceVolume, false);
}

void InjectorSheet::OnInitMenuPopup(CMenu* menu, UINT index, BOOL system_menu) {
    c1kitshell::KitSheet::OnInitMenuPopup(menu, index, system_menu);
    if (system_menu || menu == nullptr) return;
    menu->CheckMenuItem(kCommandOnTop, always_on_top_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    menu->CheckMenuItem(kCommandIgnoreAmount, ignore_amount_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    menu->CheckMenuItem(kCommandAllowWithout, allow_without_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    menu->CheckMenuItem(kCommandMute, ambience_muted() ? MF_CHECKED : MF_UNCHECKED);
}

void InjectorSheet::OnSetFolder() { browse_for_folder(this); }
void InjectorSheet::OnRefreshCobs() { reload(); }
void InjectorSheet::OnToggleOnTop() { set_always_on_top(always_on_top_ == 0); }
// The 2.0 kit's Hide; here it minimises, so the kit can be brought back.
void InjectorSheet::OnHide() { ShowWindow(SW_MINIMIZE); }
void InjectorSheet::OnToggleIgnoreAmount() { set_ignore_amount(ignore_amount_ == 0); }
void InjectorSheet::OnToggleAllowWithout() { set_allow_without_subject(allow_without_ == 0); }
void InjectorSheet::OnToggleMute() { set_ambience_muted(!ambience_muted()); }
void InjectorSheet::OnCloseCommand() { PostMessage(WM_CLOSE); }

void InjectorSheet::OnAbout() {
    MessageBox(_T("Injector Kit\n\nLibreCreatures' Object Injector, wearing the look of the ")
               _T("original beside it."),
               _T("About Injector Kit"), MB_OK | MB_ICONINFORMATION);
}

void InjectorSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

void InjectorSheet::before_game_quit() {
    save_preferences();
}

void InjectorSheet::OnClose() {
    request_game_quit();
}

void InjectorSheet::OnDestroy() {
    if (conversation_ && !quitting()) conversation_->close();
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace injector

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_injector_window(CFont& font) {
    auto* sheet = new injector::InjectorSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "ObjectInjector.OLE";
    // {B07C9809-2CE6-11D0-AB39-0020AF71E433}
    const GUID clsid = {0xb07c9809, 0x2ce6, 0x11d0,
                        {0xab, 0x39, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 7;
    kit.tool_value_prog_id = "ObjectInjector.OLE";
    kit.tool_name_string = injector::kStringToolName;
    kit.tool_help_string = injector::kStringToolHelp;
    kit.ole_init_failed_string = injector::kStringOleInitFailed;
    kit.create_main_window = &create_injector_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
