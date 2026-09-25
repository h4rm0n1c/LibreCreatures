// The Science Kit's sheet: the subject creature, the data files, the game
// conversations, polling and preferences.  The original's behaviour is in
// ../ORIGINAL.md.

#include "science.hpp"
#include "science_ids.hpp"

#include <fstream>
#include <iterator>

namespace science {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Science Kit";
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
    return MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
}

std::string first_field(const std::string& reply) {
    std::string field = reply.substr(0, reply.find('|'));
    while (!field.empty() && (field.back() == ' ' || field.back() == '\0')) {
        field.pop_back();
    }
    while (!field.empty() && field.front() == ' ') {
        field.erase(field.begin());
    }
    return field;
}

} // namespace

BEGIN_MESSAGE_MAP(ScienceSheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

// The original opened on a cover page and played sound; neither is in this
// build.
ScienceSheet::ScienceSheet(CFont& default_font)
    : KitSheet(kStringToolName, kStringPausedMarker),
      default_font_(default_font),
      biochemistry_page_(*this),
      genetics_page_(*this),
      brain_page_(*this),
      decisions_page_(*this),
      injections_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&biochemistry_page_);
    AddPage(&genetics_page_);
    AddPage(&brain_page_);
    AddPage(&decisions_page_);
    AddPage(&injections_page_);
}

ScienceSheet::~ScienceSheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool ScienceSheet::create_window() {
    load_data_files();
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

int ScienceSheet::OnCreate(LPCREATESTRUCT create) {
    // The tabs on one row, scrolling if the window is too narrow for them.
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
    return 0;
}

BOOL ScienceSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &top"));
    }
    load_preferences();
    return result;
}

// The game's installation folder, where the data files are.
std::string ScienceSheet::game_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting("Main Directory");
    return std::string(CStringA(directory)) + name;
}

// The world's folder (the Register, the Genetics folder).
std::string ScienceSheet::world_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting(
        "Main Directory", c1kit::GameDirectory::world);
    return std::string(CStringA(directory)) + name;
}

// allchemicals.str, decision.str, injections.str and the themes
// (CMonitorPage::LoadAllChemicalDefinitions @ 0x00403e50 and friends).
void ScienceSheet::load_data_files() {
    std::vector<std::uint8_t> bytes;
    if (!read_file(game_file(c1kit::kAllChemicalsFileName), bytes) ||
        !c1kit::parse_chemical_names(bytes, chemical_names_)) {
        chemical_names_.assign(c1kit::kChemicalCount, std::string());
    }
    neuron_names_.chemicals = chemical_names_;
    if (!read_file(game_file(c1kit::kDecisionNamesFileName), bytes) ||
        !c1kit::parse_counted_strings(bytes, neuron_names_.decisions)) {
        neuron_names_.decisions.clear();
    }
    if (!read_file(game_file(c1kit::kInjectionsFileName), bytes) ||
        !c1kit::parse_uncounted_strings(bytes, medicines_)) {
        medicines_.clear();
    }
    themes_.clear();
    if (read_file(game_file(c1kit::kThemesFileName), bytes)) {
        c1kit::parse_themes(bytes, themes_);
    } else if (read_file(game_file(c1kit::kOriginalThemesFileName), bytes)) {
        c1kit::parse_original_themes(bytes, themes_);
    }
}

bool ScienceSheet::save_themes() {
    const std::string path = game_file(c1kit::kThemesFileName);
    if (!write_file(path, c1kit::serialize_themes(themes_))) {
        AfxMessageBox(CString(_T("The Science Kit could not write ")) + CString(path.c_str()) + _T("."));
        return false;
    }
    return true;
}

CString ScienceSheet::name_for_moniker(const std::string& moniker) const {
    if (const c1kit::OwnerRecord* record = c1kit::find_record(register_, moniker)) {
        if (!(*record)[c1kit::kOwnerCreatureName].empty()) {
            return CString((*record)[c1kit::kOwnerCreatureName].c_str());
        }
    }
    return CString();
}

void ScienceSheet::load_preferences() {
    WindowSize size = {};
    if (registry_->read_binary(c1kit::SettingsScope::user, "Size", &size, sizeof(size)) &&
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
    registry_->read_dword(c1kit::SettingsScope::user, "On Top", always_on_top_);
    // "Page" counts the original's cover as page 0 (the key is shared).
    std::uint32_t page = 1;
    registry_->read_dword(c1kit::SettingsScope::user, "Page", page);
    saved_page_ = page > 0 ? static_cast<int>(page) - 1 : 0;
    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top_ != 0 ? &wndTopMost : &wndNoTopMost, left < 0 ? 0 : left,
                 top < 0 ? 0 : top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, always_on_top_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    }
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
}

void ScienceSheet::save_preferences() {
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
    registry_->write_binary("Size", &size, sizeof(size));
    registry_->write_dword("On Top", always_on_top_);
    registry_->write_dword("Page", static_cast<std::uint32_t>(GetActiveIndex() + 1));
}

// Fix (bug 1): one query holder, and one brain report holder, for the kit's
// lifetime; the original made a holder for almost every query.
bool ScienceSheet::query(const std::string& script, std::string& reply) {
    reply.clear();
    return conversation_ && !quitting() &&
           conversation_->query_reusing_holder(c1kit::kMacroModeQuery, script.c_str(), reply);
}

bool ScienceSheet::query_binary(const std::string& script, std::string& reply) {
    reply.clear();
    return conversation_ && !quitting() &&
           conversation_->query_binary(c1kit::kMacroModeQuery, script.c_str(), reply);
}

bool ScienceSheet::brain_report(int mode, int rule, std::string& reply) {
    reply.clear();
    return report_conversation_ && !quitting() &&
           report_conversation_->query_reusing_holder(
               c1kit::kMacroModeBrainReport, c1kit::brain_report_script(mode, rule).c_str(),
               reply);
}

bool ScienceSheet::run(const std::string& script) {
    std::string reply;
    return query(script, reply);
}

void ScienceSheet::connect() {
    if (!connect_to_game(kCommandBufferBytes)) {
        return;
    }
    connected_ = true;
    if (c1kit::MacroTransport* game = transport()) {
        conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
        report_conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
    }
    take_subject();
    if (saved_page_ > 0 && saved_page_ < GetPageCount()) {
        SetActivePage(saved_page_);
        layout_pages();
    }
    SetTimer(kTimerPoll, kPollMs, nullptr);
}

// `putv ownr` makes the creature selected in the game the subject of the
// queries that follow (0: none).  Fix (bug 2): with none selected, the pages
// say so; the original looped on a modal "There is no subject" box.
void ScienceSheet::take_subject() {
    subject_ = Subject();
    lobes_.clear();
    std::string reply;
    int owner = 0;
    if (query("dde: putv ownr,endm", reply) && c1kit::parse_first_value(reply, owner) &&
        owner != 0) {
        subject_.present = true;
        if (query(c1kit::kMonikerQuery, reply)) {
            subject_.moniker = first_field(reply);
        }
        if (query("dde: getb cnam,endm", reply)) {
            subject_.name = first_field(reply);
        }
        int sex = 0;
        if (query(c1kit::kSexQuery, reply) && c1kit::parse_first_value(reply, sex)) {
            subject_.sex = sex;
        }
        if (query(c1kit::kAgeQuery, reply)) {
            subject_.age = first_field(reply);
        }
        if (query_binary(c1kit::kLobeQuery, reply)) {
            c1kit::parse_lobe_reply(reply, lobes_);
        }
    }
    std::vector<std::uint8_t> bytes;
    register_.clear();
    if (read_file(world_file(c1kit::kRegisterFileName), bytes)) {
        c1kit::parse_register(bytes, register_);
    }
    update_title();
    biochemistry_page_.subject_changed();
    genetics_page_.subject_changed();
    brain_page_.subject_changed();
    decisions_page_.subject_changed();
    injections_page_.subject_changed();
}

// "Science Kit - <name>"; paused, "Science Kit...  Paused - <name>".
void ScienceSheet::update_title() {
    const CString name = subject_.present ? CString(subject_.name.c_str())
                                          : CString(_T("no creature selected"));
    if (paused()) {
        SetWindowText(c1kitshell::load_string(kStringTitlePaused) +
                      c1kitshell::load_string(kStringPausedMarker) + name);
    } else {
        SetWindowText(c1kitshell::load_string(kStringTitle) + name);
    }
}

// The biochemistry graph samples every other tick whichever page is
// showing, so its history has no gaps; the other pages poll only while they
// are showing.
void ScienceSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connect();
    } else if (timer_id == kTimerPoll && !paused() && subject_.present && !quitting()) {
        ++poll_count_;
        if (poll_count_ % 2 == 0) {
            biochemistry_page_.sample();
        }
        auto* page = dynamic_cast<SciencePage*>(GetActivePage());
        if (page != nullptr && page != &biochemistry_page_) {
            page->poll();
        }
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// Control states: 6 the selection changed, 7 its name changed, 8 close,
// 9 the game paused or resumed.
void ScienceSheet::on_control_state(std::uint8_t state) {
    switch (state) {
    case 6:
        if (!paused()) {
            take_subject();
        }
        return;
    case 7:
        if (!paused() && subject_.present) {
            std::string reply;
            if (query("dde: getb cnam,endm", reply)) {
                subject_.name = first_field(reply);
            }
            update_title();
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

void ScienceSheet::OnSize(UINT type, int cx, int cy) {
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

void ScienceSheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
}

// Fix (bug 3): "Always on top" on the system menu; the original's menu for
// it (143) was never loaded.
void ScienceSheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

void ScienceSheet::before_game_quit() {
    save_preferences();
}

void ScienceSheet::OnClose() {
    request_game_quit();
}

void ScienceSheet::OnDestroy() {
    KillTimer(kTimerPoll);
    if (!quitting()) {
        if (conversation_) conversation_->close();
        if (report_conversation_) report_conversation_->close();
    }
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace science

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_science_window(CFont& font) {
    auto* sheet = new science::ScienceSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "Science.OLE";
    // {D7885C00-9F7D-11CF-BBF2-0020AF71E433}
    const GUID clsid = {0xd7885c00, 0x9f7d, 0x11cf,
                        {0xbb, 0xf2, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 4;
    kit.tool_value_prog_id = "Science.OLE";
    kit.tool_name_string = science::kStringToolName;
    kit.tool_help_string = science::kStringToolHelp;
    kit.ole_init_failed_string = science::kStringOleInitFailed;
    kit.create_main_window = &create_science_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
