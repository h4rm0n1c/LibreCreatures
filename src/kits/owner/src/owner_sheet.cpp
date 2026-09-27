// The Owner's Kit's sheet: the subject creature, the register and album
// files, preferences and pausing.  The original's behaviour is in
// ../ORIGINAL.md; deviations are marked "Fix (bug N)".

#include "owner.hpp"
#include "owner_ids.hpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>

namespace owner {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Owners Kit";
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

std::string trim(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\0')) {
        text.pop_back();
    }
    while (!text.empty() && text.front() == ' ') {
        text.erase(text.begin());
    }
    return text;
}

// "%H:%M %d %B %Y", as the album stores when a photo was taken.
std::string now_for_album() {
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_s(&local, &now);
    char text[64];
    std::strftime(text, sizeof(text), "%H:%M %d %B %Y", &local);
    return text;
}

} // namespace

BEGIN_MESSAGE_MAP(OwnerSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// The original opened on a cover page (a key) and played sound; neither is
// in this build.  All three pages are always present.  Fix (bug 8): the
// original removed and re-added the Certificate page as the subject changed
// between registered and unregistered creatures.
OwnerSheet::OwnerSheet(CFont& default_font)
    : KitSheet(kStringToolName, kStringPausedMarker),
      default_font_(default_font),
      register_page_(*this),
      album_page_(*this),
      certificate_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&register_page_);
    AddPage(&album_page_);
    AddPage(&certificate_page_);
}

OwnerSheet::~OwnerSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool OwnerSheet::create_window() {
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

int OwnerSheet::OnCreate(LPCREATESTRUCT create) {
    // Fix (bug 1): the tabs on one row, scrolling if they do not fit; the
    // original stacked them in two rows that swapped places when clicked.
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

BOOL OwnerSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &top"));
    }
    load_preferences();
    return result;
}

void OwnerSheet::load_preferences() {
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
    registry_->read_dword(c1kit::SettingsScope::user, "Photo", saved_photo_);
    // "Page" counts the original's cover as page 0 (the key is shared).
    std::uint32_t page = 1;
    registry_->read_dword(c1kit::SettingsScope::user, "Page", page);
    saved_page_ = page > 0 ? static_cast<int>(page) - 1 : 0;
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

void OwnerSheet::save_preferences() {
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
    registry_->write_dword("Page",
                           static_cast<std::uint32_t>(GetActiveIndex() + 1));
    registry_->write_dword("Photo", static_cast<std::uint32_t>(selected_photo_));
}

// The world's folder: "The Register" and the albums live beside the world,
// as the 1996 kits kept them (their "Main Directory", from the per-user key
// that follows the world being played).
std::string OwnerSheet::world_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting(
        "Main Directory", c1kit::GameDirectory::world);
    return std::string(CStringA(directory)) + name;
}

// Fix (bug 6): one query holder for the kit's lifetime; the original created
// a new holder for almost every query and destroyed few of them.
bool OwnerSheet::query(const char* script, std::string& reply) {
    reply.clear();
    return conversation_ && !quitting() &&
           conversation_->query_reusing_holder(c1kit::kMacroModeQuery, script,
                                               reply);
}

void OwnerSheet::connect() {
    if (!connect_to_game(kCommandBufferBytes)) {
        return;
    }
    connected_ = true;
    if (c1kit::MacroTransport* game = transport()) {
        conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
    }
    load_register();
    take_subject();
    if (saved_page_ > 0 && saved_page_ < GetPageCount()) {
        SetActivePage(saved_page_);
        layout_pages();
    }
    SetTimer(kTimerAge, kAgeRefreshMs, nullptr);
}

// SetSelectedCreatureOwner @ 0x00406220 and HandleSelectedCreatureChanged
// @ 0x004021a0: `putv ownr` makes the creature selected in the game the
// kit's subject for the queries that follow.  Fix (bug 5): with no creature
// selected the kit says so on its pages; the original looped on a modal
// "There is no subject" retry box.
void OwnerSheet::take_subject() {
    album_page_.commit_caption();
    std::string reply;
    int owner = 0;
    subject_ = Subject();
    if (query("dde: putv ownr,endm", reply) &&
        c1kit::parse_first_value(reply, owner) && owner != 0) {
        subject_.present = true;
        refresh_subject();
    }
    load_album();
    show_all();
}

void OwnerSheet::refresh_subject() {
    std::string reply;
    if (query(c1kit::kMonikerQuery, reply)) {
        subject_.moniker = trim(reply.substr(0, reply.find('|')));
    }
    if (query("dde: getb cnam,endm", reply)) {
        subject_.name = trim(reply.substr(0, reply.find('|')));
    }
    subject_.has_history = query(c1kit::kOwnerDataQuery, reply) &&
                           c1kit::parse_owner_data(reply, subject_.history);
    int sex = 0;
    if (query(c1kit::kSexQuery, reply) && c1kit::parse_first_value(reply, sex)) {
        subject_.sex = sex;
    }
    refresh_age();
}

void OwnerSheet::refresh_age() {
    std::string reply;
    if (subject_.present && query(c1kit::kAgeQuery, reply)) {
        subject_.age = trim(reply.substr(0, reply.find('|')));
    }
}

const c1kit::OwnerRecord* OwnerSheet::registered_record() const {
    if (!subject_.present || subject_.moniker.empty()) {
        return nullptr;
    }
    return c1kit::find_record(register_, subject_.moniker);
}

CString OwnerSheet::name_for_moniker(const std::string& moniker) const {
    if (const c1kit::OwnerRecord* record = c1kit::find_record(register_, moniker)) {
        if (!(*record)[c1kit::kOwnerCreatureName].empty()) {
            return CString((*record)[c1kit::kOwnerCreatureName].c_str());
        }
    }
    return c1kitshell::load_string(kStringUnknown);
}

// LoadOwnerRecordsFromRegister @ 0x00401630.  A missing or empty file is an
// empty register.
void OwnerSheet::load_register() {
    std::vector<std::uint8_t> bytes;
    register_.clear();
    const std::string path = world_file(c1kit::kRegisterFileName);
    if (read_file(path, bytes) && !c1kit::parse_register(bytes, register_)) {
        AfxMessageBox(CString(_T("The Owner's Kit could not read ")) +
                      CString(path.c_str()) +
                      _T(".\nIt will not be changed."));
        register_.clear();
    }
}

// Fix (bug 2): the register is written as soon as a birth is registered;
// the original wrote it only when it shut down, which does not happen when
// the game terminates the kit on "app: quit".
bool OwnerSheet::save_register() {
    const std::string path = world_file(c1kit::kRegisterFileName);
    if (!write_file(path, c1kit::serialize_register(register_))) {
        AfxMessageBox(CString(_T("The Owner's Kit could not write ")) +
                      CString(path.c_str()) + _T("."));
        return false;
    }
    return true;
}

// TransferOwnerDataFields @ 0x00409ac0 (direction 1): store the record on
// the creature with `putb [...] data`, which also gives it its name, then
// read it back; add or replace the register entry.
bool OwnerSheet::register_birth(const c1kit::OwnerRecord& record) {
    if (!subject_.present) {
        return false;
    }
    std::string reply;
    const std::string script =
        "dde: putb [" + c1kit::format_owner_data(record) + "] data,endm";
    if (!query(script.c_str(), reply)) {
        AfxMessageBox(_T("The game did not accept the registration."));
        return false;
    }
    refresh_subject();
    const c1kit::OwnerRecord stored =
        subject_.has_history ? subject_.history : record;
    bool replaced = false;
    for (c1kit::OwnerRecord& entry : register_) {
        if (entry[c1kit::kOwnerMoniker] == stored[c1kit::kOwnerMoniker]) {
            entry = stored;
            replaced = true;
        }
    }
    if (!replaced) {
        register_.push_back(stored);
    }
    const bool saved = save_register();
    show_all();
    return saved;
}

// LoadPhotoAlbumForSelectedCreature @ 0x004031a0.
void OwnerSheet::load_album() {
    photos_.clear();
    selected_photo_ = 0;
    if (!subject_.present || subject_.moniker.empty()) {
        return;
    }
    std::vector<std::uint8_t> bytes;
    const std::string path = world_file(c1kit::album_file_name(subject_.moniker));
    if (read_file(path, bytes) && !c1kit::parse_album(bytes, photos_)) {
        AfxMessageBox(CString(_T("The Owner's Kit could not read ")) +
                      CString(path.c_str()) + _T("."));
        photos_.clear();
    }
    if (!photos_.empty()) {
        selected_photo_ = static_cast<int>(
            saved_photo_ < photos_.size() ? saved_photo_ : photos_.size() - 1);
    }
}

// Fix (bug 2): albums are written after every change; the original wrote an
// album only when its page was destroyed or the subject changed.
bool OwnerSheet::save_album() {
    if (!subject_.present || subject_.moniker.empty()) {
        return false;
    }
    const std::string path = world_file(c1kit::album_file_name(subject_.moniker));
    if (!write_file(path, c1kit::serialize_album(photos_))) {
        AfxMessageBox(CString(_T("The Owner's Kit could not write ")) +
                      CString(path.c_str()) + _T("."));
        return false;
    }
    return true;
}

void OwnerSheet::select_photo(int index) {
    if (photos_.empty()) {
        selected_photo_ = 0;
        return;
    }
    selected_photo_ = (std::max)(0, (std::min)(index, static_cast<int>(photos_.size()) - 1));
}

// Take photo: `dde: panc` pans the camera to the creature and `dde: pict`
// writes a 120 x 140 snapshot (temp.spr in the game's directory) and replies
// with its path.  Fix (bug 3): failures say what failed and name the file;
// the original reported a bare " was not found.".
bool OwnerSheet::take_photo() {
    if (!subject_.present) {
        return false;
    }
    std::string reply;
    if (!query(c1kit::photo_query().c_str(), reply) || trim(reply).empty()) {
        AfxMessageBox(_T("The game could not take a photograph.\n\n")
                      _T("The creature has to be in view."));
        return false;
    }
    const std::string path = trim(reply.substr(0, reply.find('|')));
    std::vector<std::uint8_t> bytes;
    c1kit::Photo photo;
    if (!read_file(path, bytes) || !c1kit::parse_snapshot(bytes, photo.bitmap)) {
        AfxMessageBox(CString(_T("The Owner's Kit could not read the photograph "))
                      + CString(path.c_str()) + _T("."));
        return false;
    }
    DeleteFileA(path.c_str());
    photo.taken = now_for_album();
    album_page_.commit_caption();
    photos_.push_back(std::move(photo));
    selected_photo_ = static_cast<int>(photos_.size()) - 1;
    save_album();
    album_page_.show();
    return true;
}

void OwnerSheet::delete_photo(int index) {
    if (index < 0 || index >= static_cast<int>(photos_.size())) {
        return;
    }
    photos_.erase(photos_.begin() + index);
    select_photo(index);
    save_album();
    album_page_.show();
}

void OwnerSheet::show_all() {
    update_title();
    register_page_.show();
    album_page_.show();
    certificate_page_.show();
}

// "Owner's Kit - <name>"; paused, "Owner's Kit...  Paused  -  <name>"
// (strings 108, and 106 + 107).
void OwnerSheet::update_title() {
    const CString name = subject_.present ? CString(subject_.name.c_str())
                                          : CString(_T("no creature selected"));
    if (paused()) {
        SetWindowText(c1kitshell::load_string(kStringTitlePaused) +
                      c1kitshell::load_string(kStringPausedMarker) + name);
    } else {
        SetWindowText(c1kitshell::load_string(kStringTitle) + name);
    }
}

void OwnerSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connect();
    } else if (timer_id == kTimerAge && !paused() && subject_.present) {
        refresh_age();
        register_page_.show();
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// HandleWindowMessage @ 0x00406940, the framework's control states: 6 the
// selection changed (take the newly selected creature), 7 re-read its name,
// 8 close, 9 the game paused or resumed.  Fix (bug 10): one pass over one
// holder; the original re-read the moniker three times, each on a new holder.
void OwnerSheet::on_control_state(std::uint8_t state) {
    switch (state) {
    case 6:
        if (!paused()) {
            take_subject();
        }
        return;
    case 7:
        if (!paused() && subject_.present) {
            refresh_subject();
            show_all();
        }
        return;
    case c1kit::kControlStateClose:
        PostMessage(WM_CLOSE);
        return;
    case c1kit::kControlStatePause:
        game_paused_ = !game_paused_;
        update_title();
        if (!paused() && connected_) {
            take_subject();
        }
        return;
    default:
        return;
    }
}

void OwnerSheet::OnSize(UINT type, int cx, int cy) {
    c1kitshell::KitSheet::OnSize(type, cx, cy);
    const bool minimised = type == SIZE_MINIMIZED;
    if (minimised != minimised_) {
        minimised_ = minimised;
        update_title();
        if (!paused() && connected_) {
            take_subject();
        }
    }
}

void OwnerSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
}

// Fix (bug 9): "Always on top" on the system menu; the original's menu for
// it was never loaded.
void OwnerSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

// Save before "app: quit", during which the game may terminate the kit.
void OwnerSheet::before_game_quit() {
    album_page_.commit_caption();
    save_preferences();
}

void OwnerSheet::OnClose() {
    request_game_quit();
}

void OwnerSheet::OnDestroy() {
    KillTimer(kTimerAge);
    if (conversation_ && !quitting()) {
        conversation_->close();
    }
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace owner

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_owner_window(CFont& font) {
    auto* sheet = new owner::OwnerSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "Owner.OLE";
    // {4388EF01-A35C-11CF-BBF2-0020AF71E433}
    const GUID clsid = {0x4388ef01, 0xa35c, 0x11cf,
                        {0xbb, 0xf2, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 2;
    kit.tool_value_prog_id = "Owner.OLE";
    kit.tool_name_string = owner::kStringToolName;
    kit.tool_help_string = owner::kStringToolHelp;
    kit.ole_init_failed_string = owner::kStringOleInitFailed;
    kit.create_main_window = &create_owner_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
