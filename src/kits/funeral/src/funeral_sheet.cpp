// The Funeral Kit's sheet: deaths from the game, the graves file,
// preferences.  The original's behaviour is in ../ORIGINAL.md; deviations are
// marked "Fix (bug N)".

#include "funeral.hpp"
#include "funeral_ids.hpp"

#include <ctime>
#include <fstream>
#include <iterator>

namespace funeral {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Funeral Kit";
constexpr char kVersion[] = "1.0";

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

// Writes via a temporary file, so a failed write never leaves half a file.
bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    const std::string temporary = path + ".new";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            return false;
        }
    }
    return MoveFileExA(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING) != FALSE;
}

// "%H:%M %b %d %Y", as the Register stores a birth.
std::string now_for_grave() {
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_s(&local, &now);
    char text[64];
    std::strftime(text, sizeof(text), "%H:%M %b %d %Y", &local);
    return text;
}

} // namespace

BEGIN_MESSAGE_MAP(FuneralSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// The original opened on a cover page and played sound; neither is in this
// build.  The graveyard is always the first page; a page is added for each
// dead creature.
FuneralSheet::FuneralSheet(CFont& default_font)
    : KitSheet(kStringTitle, 0),
      default_font_(default_font),
      graveyard_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&graveyard_page_);
}

FuneralSheet::~FuneralSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

// The Register and the graves are read before the window exists, and the
// graves no headstone has been made for yet each get their page back: pages
// added from the sheet's OnInitDialog hang it.
bool FuneralSheet::create_window() {
    load_register();
    load_graves();
    for (const c1kit::Grave& grave : graves_) {
        if (!grave.has_headstone()) {
            add_memorial_page(grave[c1kit::kGraveMoniker]);
        }
    }
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

int FuneralSheet::OnCreate(LPCREATESTRUCT create) {
    // Fix (bug 1): the tabs on one row, scrolling if they do not fit.
    EnableStackedTabs(FALSE);
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
    const CString palettes =
        c1kitshell::game_directory_setting("Palette Directory");
    palette_.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\"))
                                                          : palettes)) +
                  kPaletteFile);
    return 0;
}

BOOL FuneralSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &top"));
    }
    load_preferences();
    return result;
}

void FuneralSheet::load_preferences() {
    WindowSize size = {};
    if (registry_->read_binary(c1kit::SettingsScope::user, "Size", &size,
                               sizeof(size)) &&
        size.width > 0 && size.height > 0) {
        set_window_size(CSize(size.width, size.height));
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

// The original also kept "Page"; the pages here come and go with the dead,
// so the kit always opens on the graveyard or the newest death.
void FuneralSheet::save_preferences() {
    if (registry_ == nullptr) {
        return;
    }
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(&placement);
    const CRect window(placement.rcNormalPosition);
    const WindowLocation location = {window.left < 0 ? 0 : window.left,
                                     window.top < 0 ? 0 : window.top};
    const WindowSize size = {window.Width(), window.Height()};
    registry_->write_binary("Location", &location, sizeof(location));
    registry_->write_binary("Size", &size, sizeof(size));
    registry_->write_dword("On Top", always_on_top_);
}

// The world's folder, where the Owner's Kit keeps "The Register" and the
// albums (the per-user "Main Directory", which follows the world).
std::string FuneralSheet::world_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting(
        "Main Directory", c1kit::GameDirectory::world);
    return std::string(CStringA(directory)) + name;
}

// The kit sends the game nothing but "app: quit"; it connects so it can.
void FuneralSheet::connect() {
    connect_to_game(kCommandBufferBytes);
}

void FuneralSheet::load_register() {
    std::vector<std::uint8_t> bytes;
    register_.clear();
    const std::string path = world_file(c1kit::kRegisterFileName);
    if (read_file(path, bytes) && !c1kit::parse_register(bytes, register_)) {
        AfxMessageBox(CString(_T("The Funeral Kit could not read ")) +
                      CString(path.c_str()) + _T("."));
        register_.clear();
    }
}

void FuneralSheet::load_graves() {
    std::vector<std::uint8_t> bytes;
    graves_.clear();
    const std::string path = world_file(c1kit::kGravesFileName);
    if (read_file(path, bytes) && !c1kit::parse_graves(bytes, graves_)) {
        AfxMessageBox(CString(_T("The Funeral Kit could not read ")) +
                      CString(path.c_str()) + _T(".\nIt will not be changed."));
        graves_.clear();
    }
}

// Fix (bug 2): the graves are written as soon as they change; the original
// wrote its files only when it shut down, which does not happen when the
// game terminates the kit on "app: quit".
bool FuneralSheet::save_graves() {
    const std::string path = world_file(c1kit::kGravesFileName);
    if (!write_file(path, c1kit::serialize_graves(graves_))) {
        AfxMessageBox(CString(_T("The Funeral Kit could not write ")) +
                      CString(path.c_str()) + _T("."));
        return false;
    }
    return true;
}

c1kit::Grave* FuneralSheet::grave(const std::string& moniker) {
    return c1kit::find_grave(graves_, moniker);
}

CString FuneralSheet::name_for_moniker(const std::string& moniker) const {
    for (const c1kit::Grave& grave : graves_) {
        if (grave[c1kit::kGraveMoniker] == moniker &&
            !grave[c1kit::kGraveCreatureName].empty()) {
            return CString(grave[c1kit::kGraveCreatureName].c_str());
        }
    }
    if (const c1kit::OwnerRecord* record = c1kit::find_record(register_, moniker)) {
        if (!(*record)[c1kit::kOwnerCreatureName].empty()) {
            return CString((*record)[c1kit::kOwnerCreatureName].c_str());
        }
    }
    return CString(moniker.c_str());
}

// The game reports a death with an integer message carrying the moniker id,
// when a dead creature's pane on the event bar is clicked (which launches
// the kit) and, for each corpse that leaves the event bar, when the kit next
// connects.
void FuneralSheet::on_integer_message(std::int32_t payload) {
    if (!ready_) {
        early_deaths_.push_back(payload);
        return;
    }
    creature_died(c1kit::moniker_from_id(static_cast<std::uint32_t>(payload)));
}

// A registered creature gets a grave (once: the same death can be reported
// by both routes) and a memorial page; one the Register does not know gets
// an unmarked grave.  The Register is read afresh for each death, as the
// original's LoadRegisterPhotoCollection @ 0x00409b20 does.
void FuneralSheet::creature_died(const std::string& moniker) {
    commit_epitaphs();
    int page = page_index_for(moniker);
    if (page < 0) {
        load_register();
        if (grave(moniker) == nullptr) {
            if (const c1kit::OwnerRecord* record =
                    c1kit::find_record(register_, moniker)) {
                graves_.push_back(c1kit::grave_from_register(*record, now_for_grave()));
                save_graves();
            }
        }
        page = grave(moniker) != nullptr
                   ? add_memorial_page(moniker)
                   : add_unmarked_page(moniker, now_for_grave());
    }
    SetActivePage(page);
    layout_pages();
    if (IsIconic()) {
        ShowWindow(SW_RESTORE);
    }
}

int FuneralSheet::page_index_for(const std::string& moniker) {
    for (const auto& page : memorial_pages_) {
        if (page->moniker() == moniker) {
            return GetPageIndex(page.get());
        }
    }
    for (const auto& page : unmarked_pages_) {
        if (page->moniker() == moniker) {
            return GetPageIndex(page.get());
        }
    }
    return -1;
}

// The tab is the creature's name, as the original's.
int FuneralSheet::add_memorial_page(const std::string& moniker) {
    const int existing = page_index_for(moniker);
    if (existing >= 0) {
        return existing;
    }
    memorial_pages_.push_back(
        std::make_unique<MemorialPage>(*this, moniker, name_for_moniker(moniker)));
    AddPage(memorial_pages_.back().get());
    return GetPageCount() - 1;
}

int FuneralSheet::add_unmarked_page(const std::string& moniker,
                                    const std::string& death_time) {
    unmarked_pages_.push_back(
        std::make_unique<UnmarkedPage>(*this, moniker, death_time));
    AddPage(unmarked_pages_.back().get());
    return GetPageCount() - 1;
}

void FuneralSheet::commit_epitaphs() {
    for (const auto& page : memorial_pages_) {
        page->commit_epitaph();
    }
}

// Fix (bug 3): Make Headstone puts the creature's headstone in the graveyard
// and shows it there; the original's button had no visible effect.
void FuneralSheet::make_headstone(const std::string& moniker) {
    commit_epitaphs();
    c1kit::Grave* made = grave(moniker);
    if (made == nullptr) {
        return;
    }
    if (!made->has_headstone()) {
        (*made)[c1kit::kGraveHeadstone] = "1";
        save_graves();
    }
    graveyard_page_.show_grave(moniker);
    SetActivePage(&graveyard_page_);
    layout_pages();
}

void FuneralSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        // Deaths reported before now (they add pages, which cannot be done
        // while the sheet initialises).
        ready_ = true;
        for (const std::int32_t payload : early_deaths_) {
            on_integer_message(payload);
        }
        early_deaths_.clear();
        connect();
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

void FuneralSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
}

// Fix (bug 6): "Always on top" on the system menu; the original's menu for
// it was never loaded.
void FuneralSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

// Save before "app: quit", during which the game may terminate the kit.
void FuneralSheet::before_game_quit() {
    commit_epitaphs();
    save_preferences();
}

void FuneralSheet::OnClose() {
    request_game_quit();
}

void FuneralSheet::OnDestroy() {
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace funeral

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_funeral_window(CFont& font) {
    auto* sheet = new funeral::FuneralSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "Funeral.OLE";
    // {D3BCF121-A4D8-11CF-BBF2-0020AF71E433}
    const GUID clsid = {0xd3bcf121, 0xa4d8, 0x11cf,
                        {0xbb, 0xf2, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 9;
    kit.tool_value_prog_id = "Funeral.OLE";
    kit.tool_name_string = funeral::kStringToolName;
    kit.tool_help_string = funeral::kStringToolHelp;
    kit.ole_init_failed_string = funeral::kStringOleInitFailed;
    kit.create_main_window = &create_funeral_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
