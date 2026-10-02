// The Ecology Kit's sheet: start-up, preferences, polling and pausing.

#include "ecology.hpp"
#include "ecology_ids.hpp"

#include "c1kitshell/kit_art.hpp"

#include <algorithm>

namespace ecology {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Ecology Kit";
constexpr char kVersion[] = "1.0";

struct WindowLocation {
    std::int32_t left;
    std::int32_t top;
};
struct WindowSize {
    std::int32_t width;
    std::int32_t height;
};

} // namespace

BEGIN_MESSAGE_MAP(EcologySheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

EcologySheet::EcologySheet(CFont& default_font)
    : KitSheet(kStringKitName, kStringPausedSuffix),
      default_font_(default_font),
      page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&page_);
}

EcologySheet::~EcologySheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool EcologySheet::create_window() {
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                      WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

int EcologySheet::OnCreate(LPCREATESTRUCT create) {
    if (c1kitshell::KitSheet::OnCreate(create) == -1) {
        return -1;
    }
    SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(default_font_.GetSafeHandle()), 0);
    registry_ = c1kit::open_kit_settings(kCompany, kProduct, kVersion,
                                         c1kit::SettingsOpenPolicy::user_key_only);
    if (registry_ == nullptr || !registry_->is_open()) {
        return -1;
    }
    std::uint32_t layer = static_cast<std::uint32_t>(c1kit::EcologyLayer::temperature);
    registry_->read_dword(c1kit::SettingsScope::user, "Layer", layer);
    if (layer < static_cast<std::uint32_t>(c1kit::EcologyLayer::count)) {
        page_.set_layer(static_cast<c1kit::EcologyLayer>(layer));
    }
    return 0;
}

BOOL EcologySheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    install_options(registry_, false);
    load_preferences();
    return result;
}

void EcologySheet::load_preferences() {
    WindowSize size = {};
    if (registry_->read_binary(c1kit::SettingsScope::user, "Size", &size, sizeof(size)) &&
        size.width > 0 && size.height > 0) {
        set_window_size(CSize((std::min)(size.width, static_cast<std::int32_t>(GetSystemMetrics(SM_CXSCREEN))),
                              (std::min)(size.height, static_cast<std::int32_t>(GetSystemMetrics(SM_CYSCREEN)))));
    }
    CRect window;
    GetWindowRect(&window);
    WindowLocation location = {0x40, 0x200};
    registry_->read_binary(c1kit::SettingsScope::user, "Location", &location, sizeof(location));
    const int max_left = GetSystemMetrics(SM_CXSCREEN) - window.Width();
    const int max_top = GetSystemMetrics(SM_CYSCREEN) - window.Height();
    SetWindowPos(always_on_top() ? &wndTopMost : &wndNoTopMost,
                 (std::max)(0, (std::min)(static_cast<int>(location.left), max_left)),
                 (std::max)(0, (std::min)(static_cast<int>(location.top), max_top)), 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
}

void EcologySheet::save_preferences() {
    if (registry_ == nullptr || GetSafeHwnd() == nullptr) {
        return;
    }
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(&placement);
    const CRect window(placement.rcNormalPosition);
    const WindowLocation location = {(std::max)(0L, window.left), (std::max)(0L, window.top)};
    const WindowSize size = {window.Width(), window.Height()};
    registry_->write_binary("Location", &location, sizeof(location));
    registry_->write_binary("Size", &size, sizeof(size));
    save_layer();
}

void EcologySheet::save_layer() {
    if (registry_ != nullptr) {
        registry_->write_dword("Layer", static_cast<std::uint32_t>(page_.layer()));
    }
}

std::string EcologySheet::game_file(const std::string& name) const {
    return std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))) + name;
}

std::vector<std::string> EcologySheet::image_directories() const {
    std::vector<std::string> directories;
    for (const c1kit::GameDirectory which :
         {c1kit::GameDirectory::world, c1kit::GameDirectory::installation}) {
        const std::string directory(
            CStringA(c1kitshell::game_directory_setting("Image Directory", which)));
        if (!directory.empty() &&
            std::find(directories.begin(), directories.end(), directory) == directories.end()) {
            directories.push_back(directory);
        }
    }
    directories.push_back(game_file("Images\\"));
    return directories;
}

// Asks the game whether it answers `dde: ecol` (`dde: dcap`, which a game
// without it leaves unanswered), then polls.
void EcologySheet::connect() {
    if (!connect_to_game(kCommandBufferBytes)) {
        return;
    }
    connected_ = true;
    if (c1kit::MacroTransport* game = transport()) {
        conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
    }
    std::string reply;
    int capabilities = 0;
    reports_ecology_ = run("dde: dcap,endm", reply) && c1kit::parse_first_value(reply, capabilities) &&
                       (capabilities & c1kit::kDdeCapabilityEcology) != 0;
    poll();
    SetTimer(kTimerPoll, kPollMs, nullptr);
}

bool EcologySheet::run(const std::string& script, std::string& reply) {
    reply.clear();
    return conversation_ && !quitting() &&
           conversation_->query_reusing_holder(c1kit::kMacroModeQuery, script.c_str(), reply);
}

// The rooms and creatures, then where the food and toys are, counted by
// room.  A failed query keeps the last figures.
void EcologySheet::poll() {
    if (!reports_ecology_) {
        page_.refresh();
        return;
    }
    std::string reply;
    c1kit::EcologySnapshot world;
    if (run("dde: ecol,endm", reply) && c1kit::parse_ecology(reply, world)) {
        world_ = std::move(world);
    }
    std::vector<int> food(world_.rooms.size(), 0);
    std::vector<int> toys(world_.rooms.size(), 0);
    for (const c1kit::EcologyObjectKind& kind : c1kit::kEcologyObjectKinds) {
        if (!run(c1kit::object_places_script(kind.genus), reply)) {
            continue;
        }
        for (const auto& place : c1kit::parse_object_places(reply)) {
            const int room = c1kit::room_at(world_.rooms, place.first, place.second);
            if (room >= 0) {
                ++(kind.layer == c1kit::EcologyLayer::food ? food : toys)[static_cast<std::size_t>(room)];
            }
        }
    }
    food_ = std::move(food);
    toys_ = std::move(toys);
    page_.refresh();
}

// `setv norn` is the game's own selection (the Creatures menu's), and
// `dde: panc` pans the view to the selected creature.  Both run on the
// game's scheduler, not in a query: a selection is announced to the kits,
// this one too, and a query waits for its reply.
void EcologySheet::select_creature(std::uint32_t handle) {
    if (c1kit::MacroTransport* game = transport()) {
        c1kit::execute_scheduled(
            *game, ("inst,setv norn " + std::to_string(handle) + ",dde: panc").c_str(), false);
    }
}

void EcologySheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connect();
    } else if (timer_id == kTimerPoll && !paused()) {
        poll();
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

void EcologySheet::update_pause() {
    if (paused()) {
        show_paused_title();
        return;
    }
    show_normal_title();
    if (connected_) {
        poll();
    }
}

void EcologySheet::on_control_state(std::uint8_t state) {
    if (state == c1kit::kControlStateClose) {
        PostMessage(WM_CLOSE);
        return;
    }
    if (state == c1kit::kControlStatePause) {
        game_paused_ = !game_paused_;
        update_pause();
    }
}

void EcologySheet::OnSize(UINT type, int cx, int cy) {
    c1kitshell::KitSheet::OnSize(type, cx, cy);
    const bool minimised = type == SIZE_MINIMIZED;
    if (minimised != minimised_) {
        minimised_ = minimised;
        update_pause();
    }
}

void EcologySheet::before_game_quit() {
    save_preferences();
}

void EcologySheet::OnClose() {
    request_game_quit();
}

void EcologySheet::OnDestroy() {
    KillTimer(kTimerPoll);
    if (conversation_ && !quitting()) {
        conversation_->close();
    }
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace ecology

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_ecology_window(CFont& font) {
    auto* sheet = new ecology::EcologySheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "Ecology.OLE";
    // {6E16D129-07C3-4D8D-9061-17D3EDD61144}
    const GUID clsid = {0x6e16d129, 0x07c3, 0x4d8d,
                        {0x90, 0x61, 0x17, 0xd3, 0xed, 0xd6, 0x11, 0x44}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 10;  // after the ten 1996 kits
    kit.original_file_name = nullptr;  // new: there is no 1996 look
    kit.tool_value_prog_id = "Ecology.OLE";
    kit.tool_name_string = ecology::kStringToolName;
    kit.tool_help_string = ecology::kStringToolHelp;
    kit.ole_init_failed_string = ecology::kStringOleInitFailed;
    kit.create_main_window = &create_ecology_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
