// KitSheet: the connected main window shared by the property-sheet kits.

#include "c1kitshell/kit_shell.hpp"

#include "c1kit/conversation.hpp"
#include "c1kitshell/kit_art.hpp"

#include <cstdio>

namespace c1kitshell {
namespace {

// Posted after a tab switch: comctl32 shows the new page only once the
// sheet's WM_NOTIFY handling returns, and places it at the template size.
constexpr UINT kDeferredLayoutMessage = WM_APP + 0x4c;
// Posted by Options > Skin: the window is rebuilt once the menu command has
// returned, not from inside it.
constexpr UINT kSkinChangeMessage = WM_APP + 0x4d;

// Options and Help.  The same ids arrive as WM_COMMAND from the menu bar
// and as WM_SYSCOMMAND from the system menu, so they are multiples of 16
// below 0xF000 (the system menu keeps the low four bits).
constexpr UINT kOptionOnTop = 0x0110;
constexpr UINT kOptionMute = 0x0120;
constexpr UINT kOptionSkinLibre = 0x0130;
constexpr UINT kOptionSkinClassic = 0x0140;
constexpr UINT kHelpAbout = 0x0150;
constexpr UINT kKitIcon = 128;  // every kit's icon, as in 1996

// The setting, or failing that the first of the names kits used for it
// before Options existed.
bool read_setting(c1kit::KitSettings* settings, std::initializer_list<const char*> names,
                  std::uint32_t& value) {
    if (settings == nullptr) {
        return false;
    }
    for (const char* name : names) {
        if (settings->read_dword(c1kit::SettingsScope::user, name, value)) {
            return true;
        }
    }
    return false;
}

bool g_classic_art_available = false;

CString skin_key() {
    const char* prog_id = kit_definition().identity.prog_id;
    return CString(_T("Software\\LibreCreatures\\Kits\\")) +
           CString(prog_id != nullptr ? prog_id : "kit");
}

} // namespace

BEGIN_MESSAGE_MAP(KitSheet, CPropertySheet)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_MESSAGE(kDeferredLayoutMessage, &KitSheet::OnDeferredLayout)
END_MESSAGE_MAP()

// Options > Skin, kept per kit.
KitSkin skin_preference() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValue(HKEY_CURRENT_USER, skin_key(), _T("Skin"), RRF_RT_REG_DWORD, nullptr,
                    &value, &size) != ERROR_SUCCESS ||
        value > static_cast<DWORD>(KitSkin::classic)) {
        return KitSkin::automatic;
    }
    return static_cast<KitSkin>(value);
}

void set_skin_preference(KitSkin skin) {
    HKEY key = nullptr;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, skin_key(), 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                       &key, nullptr) == ERROR_SUCCESS) {
        const DWORD value = static_cast<DWORD>(skin);
        RegSetValueEx(key, _T("Skin"), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value),
                      sizeof(value));
        RegCloseKey(key);
    }
}

bool classic_art_available() {
    return g_classic_art_available;
}

void note_classic_art_available(bool available) {
    g_classic_art_available = available;
}

KitSheet::KitSheet(UINT caption_string, UINT paused_suffix_string)
    : CPropertySheet(caption_string),
      caption_string_(caption_string),
      paused_suffix_string_(paused_suffix_string) {}

KitSheet::~KitSheet() {
    if (transport_ != nullptr) {
        transport_->release();
        transport_ = nullptr;
    }
}

bool KitSheet::connect_to_game(std::size_t buffer_bytes) {
    c1kit::ConnectResult result = c1kit::ConnectResult::not_registered;
    long hresult = 0;
    transport_ = c1kit::connect_sfc_ole(buffer_bytes, &result, &hresult);
    if (transport_ != nullptr) {
        return true;
    }
    char message[512];
    c1kit::describe_connect_failure(result, hresult, message, sizeof(message));
    AfxMessageBox(CString(message));
    return false;
}

void KitSheet::handle_kit_message(const c1kit::KitMessage& message) {
    if (message.kind == c1kit::kMessageKindIdentity) {
        if (message.code == c1kit::kIdentityCodeYourIdIs) {
            // The 1996 kits keep the low byte of aux.
            tool_id_ = message.aux & 0xff;
        } else if (message.code == c1kit::kIdentityCodeInteger) {
            on_integer_message(message.payload);
        }
        return;
    }
    if (message.kind == c1kit::kMessageKindControl) {
        on_control_state(message.code);
    }
}

void KitSheet::request_game_quit() {
    if (!quitting_) {
        before_game_quit();
    }
    quitting_ = true;
    if (transport_ == nullptr) {
        PostMessage(WM_COMMAND, ID_APP_EXIT, 0);
    } else {
        char script[96];
        std::snprintf(script, sizeof(script), "inst,app: quit %d,endm",
                      tool_id_);
        c1kit::execute_scheduled(*transport_, script, /*keep_holder=*/true);
    }
    DestroyWindow();
}

void KitSheet::show_paused_title() {
    SetWindowText(load_string(caption_string_) +
                  load_string(paused_suffix_string_));
}

void KitSheet::show_normal_title() {
    SetWindowText(load_string(caption_string_));
}

void KitSheet::enable_resizing(CSize default_page_dlu) {
    CTabCtrl* tab = GetTabControl();
    CPropertyPage* page = GetActivePage();
    if (tab == nullptr || page == nullptr || page->GetSafeHwnd() == nullptr) {
        return;
    }
    CRect window;
    GetWindowRect(&window);
    min_track_ = window.Size();

    CRect client;
    GetClientRect(&client);
    CRect tab_rect;
    tab->GetWindowRect(&tab_rect);
    ScreenToClient(&tab_rect);
    // The left and top insets are taken as laid out; the right and bottom
    // repeat the left one.  MFC trims a modeless sheet at the top of its
    // hidden OK button, and on Windows the tab control can end below that
    // line, so copying the bottom inset would leave the page hanging off
    // the window.
    const int inset = tab_rect.left > client.left ? tab_rect.left - client.left
                                                  : 0;
    const int top = tab_rect.top > client.top ? tab_rect.top - client.top : 0;
    tab_margins_ = CRect(inset, top, inset, inset);
    resizable_ = true;

    CRect current;
    page->GetWindowRect(&current);
    CRect wanted(0, 0, default_page_dlu.cx, default_page_dlu.cy);
    page->MapDialogRect(&wanted);
    const int grow_x = wanted.Width() > current.Width()
                           ? wanted.Width() - current.Width() : 0;
    const int grow_y = wanted.Height() > current.Height()
                           ? wanted.Height() - current.Height() : 0;
    set_window_size(CSize(window.Width() + grow_x, window.Height() + grow_y));
}

void KitSheet::set_window_size(CSize size) {
    if (!resizable_) {
        return;
    }
    const int cx = size.cx > min_track_.cx ? size.cx : min_track_.cx;
    const int cy = size.cy > min_track_.cy ? size.cy : min_track_.cy;
    SetWindowPos(nullptr, 0, 0, cx, cy,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    layout_pages();
}

void KitSheet::layout_pages() {
    CTabCtrl* tab = GetTabControl();
    if (!resizable_ || IsIconic() || tab == nullptr ||
        tab->GetSafeHwnd() == nullptr) {
        return;
    }
    CRect client;
    GetClientRect(&client);
    CRect tab_rect(client.left + tab_margins_.left,
                   client.top + tab_margins_.top,
                   client.right - tab_margins_.right,
                   client.bottom - tab_margins_.bottom);
    if (tab_rect.Width() <= 0 || tab_rect.Height() <= 0) {
        return;
    }
    tab->MoveWindow(&tab_rect);
    CRect page_rect = tab_rect;
    tab->AdjustRect(FALSE, &page_rect);
    CPropertyPage* page = GetActivePage();
    if (page != nullptr && page->GetSafeHwnd() != nullptr) {
        page->MoveWindow(&page_rect);
    }
}

BOOL KitSheet::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const BOOL handled = CPropertySheet::OnNotify(wparam, lparam, result);
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (resizable_ && header != nullptr && header->code == TCN_SELCHANGE) {
        PostMessage(kDeferredLayoutMessage);
    }
    return handled;
}

void KitSheet::OnSize(UINT type, int cx, int cy) {
    CPropertySheet::OnSize(type, cx, cy);
    if (type != SIZE_MINIMIZED) {
        layout_pages();
    }
}

void KitSheet::OnGetMinMaxInfo(MINMAXINFO* info) {
    CPropertySheet::OnGetMinMaxInfo(info);
    if (resizable_) {
        info->ptMinTrackSize.x = min_track_.cx;
        info->ptMinTrackSize.y = min_track_.cy;
    }
}

void KitSheet::enable_ambience(const char* sound, int volume) {
    ambience_sound_ = sound;
    ambience_volume_ = volume;
    ambience_ = std::make_unique<KitSound>();
    if (ambience_->open(GetSafeHwnd())) {
        const CString directory = game_directory_setting("Main Directory");
        ambience_->load(ambience_sound_, std::string(CStringA(directory)) + "Sounds\\" +
                                             ambience_sound_ + ".wav");
    }
    apply_ambience();
}

void KitSheet::fit_tabs() {
    CTabCtrl* tabs = GetTabControl();
    const int count = tabs != nullptr ? tabs->GetItemCount() : 0;
    if (count == 0) return;
    CRect last, strip;
    tabs->GetItemRect(count - 1, &last);
    tabs->GetClientRect(&strip);
    const int missing = last.right + 4 - strip.Width();
    if (missing <= 0) return;
    CRect window, tab_window;
    GetWindowRect(&window);
    tabs->GetWindowRect(&tab_window);
    ScreenToClient(&tab_window);
    SetWindowPos(nullptr, 0, 0, window.Width() + missing, window.Height(),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    tabs->SetWindowPos(nullptr, 0, 0, tab_window.Width() + missing, tab_window.Height(),
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void KitSheet::apply_ambience() {
    if (!ambience_) {
        return;
    }
    if (KitSound::muted()) {
        if (ambience_channel_ >= 0) {
            ambience_->stop(ambience_channel_);
        }
        ambience_channel_ = -1;
    } else if (ambience_channel_ < 0) {
        ambience_channel_ = ambience_->play(ambience_sound_, true, ambience_volume_);
    }
}

// ---------------------------------------------------------------------------
// Options and Help
// ---------------------------------------------------------------------------

void KitSheet::install_options(c1kit::KitSettings* settings, bool classic,
                               bool on_top_by_default) {
    option_settings_ = settings;
    classic_look_ = classic;
    KitSound::set_volume_offset(classic ? 0 : KitSound::kLibreVolumeOffset);

    std::uint32_t value = on_top_by_default ? 1 : 0;
    read_setting(settings, {"On Top", "Keep on top", "Always on Top"}, value);
    always_on_top_ = value != 0;
    value = 0;
    KitSound::set_muted(read_setting(settings, {"Mute Sounds", "Mute Ambient"}, value) &&
                        value != 0);

    CMenu options;
    options.CreatePopupMenu();
    options.AppendMenu(MF_STRING, kOptionOnTop, _T("Always on &top"));
    options.AppendMenu(MF_STRING, kOptionMute, _T("&Mute sounds"));
    CMenu skins;
    skins.CreatePopupMenu();
    skins.AppendMenu(MF_STRING, kOptionSkinLibre, _T("&LibreCreatures"));
    skins.AppendMenu(MF_STRING | (classic_art_available() ? 0 : MF_GRAYED), kOptionSkinClassic,
                     classic_art_available()
                         ? _T("&Creatures (1996)")
                         : _T("&Creatures (1996): needs the original kit beside this one"));
    options.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(skins.Detach()), _T("&Skin"));
    CMenu kit_items;
    kit_items.CreatePopupMenu();
    add_kit_options(kit_items);
    const int kit_item_count = kit_items.GetMenuItemCount();
    if (kit_item_count > 0) {
        options.AppendMenu(MF_SEPARATOR);
        for (int index = 0; index < kit_item_count; ++index) {
            CString label;
            kit_items.GetMenuString(index, label, MF_BYPOSITION);
            const UINT state = kit_items.GetMenuState(index, MF_BYPOSITION);
            const UINT id = kit_items.GetMenuItemID(index);
            if (id == 0 || (state & MF_SEPARATOR) != 0) {
                options.AppendMenu(MF_SEPARATOR);
            } else {
                options.AppendMenu(MF_STRING | (state & (MF_CHECKED | MF_GRAYED)), id, label);
            }
        }
    }
    CMenu help;
    help.CreatePopupMenu();
    help.AppendMenu(MF_STRING, kHelpAbout,
                    _T("&About ") + load_string(caption_string_) + _T("..."));

    if (classic) {
        // The 1996 windows had no menu bar: the system menu carries these.
        if (CMenu* system = GetSystemMenu(FALSE)) {
            system->AppendMenu(MF_SEPARATOR);
            system->AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(options.Detach()),
                               _T("&Options"));
            system->AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(help.Detach()), _T("&Help"));
        }
    } else {
        menu_bar_.CreateMenu();
        menu_bar_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(options.Detach()),
                             _T("&Options"));
        menu_bar_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(help.Detach()), _T("&Help"));
        CRect before;
        GetClientRect(&before);
        SetMenu(&menu_bar_);
        CRect after;
        GetClientRect(&after);
        // The menu bar takes its height from the client area: give it back.
        const int menu_height = before.Height() - after.Height();
        CRect window;
        GetWindowRect(&window);
        SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() + menu_height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (resizable_) {
            min_track_.cy += menu_height;
            layout_pages();
        }
    }
    SetWindowPos(always_on_top_ ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    refresh_option_checks();
}

CMenu* KitSheet::options_menu() {
    return classic_look_ ? GetSystemMenu(FALSE) : GetMenu();
}

void KitSheet::refresh_option_checks() {
    CMenu* menu = options_menu();
    if (menu == nullptr) {
        return;
    }
    menu->CheckMenuItem(kOptionOnTop,
                        MF_BYCOMMAND | (always_on_top_ ? MF_CHECKED : MF_UNCHECKED));
    menu->CheckMenuItem(kOptionMute,
                        MF_BYCOMMAND | (KitSound::muted() ? MF_CHECKED : MF_UNCHECKED));
    menu->CheckMenuItem(kOptionSkinLibre,
                        MF_BYCOMMAND | (classic_look_ ? MF_UNCHECKED : MF_CHECKED));
    menu->CheckMenuItem(kOptionSkinClassic,
                        MF_BYCOMMAND | (classic_look_ ? MF_CHECKED : MF_UNCHECKED));
}

void KitSheet::check_kit_option(UINT id, bool checked) {
    if (CMenu* menu = options_menu()) {
        menu->CheckMenuItem(id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    }
}

void KitSheet::enable_kit_option(UINT id, bool enabled) {
    if (CMenu* menu = options_menu()) {
        menu->EnableMenuItem(id, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
    }
}

void KitSheet::set_always_on_top(bool on_top) {
    always_on_top_ = on_top;
    SetWindowPos(on_top ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (option_settings_ != nullptr) {
        option_settings_->write_dword("On Top", on_top ? 1u : 0u);
    }
    refresh_option_checks();
}

void KitSheet::set_sounds_muted(bool muted) {
    KitSound::set_muted(muted);
    if (muted) {
        ambience_channel_ = -1;  // stopped with everything else
    }
    apply_ambience();
    if (option_settings_ != nullptr) {
        option_settings_->write_dword("Mute Sounds", muted ? 1u : 0u);
    }
    refresh_option_checks();
    on_sounds_muted_changed();
}

void KitSheet::show_about() {
    const CString name = load_string(caption_string_);
    const CString text = name +
                         _T("\n\nPart of LibreCreatures, a rebuild of the Creatures 1 kits.")
                         _T("\nBuilt ") +
                         CString(__DATE__) +
                         _T(".\n\nThe Creatures (1996) look shows the original kit's own art, ")
                         _T("read from the original beside this one.");
    const CString caption = _T("About ") + name;
    MSGBOXPARAMS params = {sizeof(params)};
    params.hwndOwner = GetSafeHwnd();
    params.hInstance = AfxGetResourceHandle();
    params.lpszText = text;
    params.lpszCaption = caption;
    params.dwStyle = MB_OK | MB_USERICON;
    params.lpszIcon = MAKEINTRESOURCE(kKitIcon);
    ::MessageBoxIndirect(&params);
}

void KitSheet::change_skin(KitSkin skin) {
    const bool want_classic = skin == KitSkin::classic;
    if (want_classic == classic_look_ || (want_classic && !classic_art_available())) {
        return;
    }
    set_skin_preference(skin);
    PostMessage(kSkinChangeMessage);
}

bool KitSheet::handle_option(UINT id) {
    switch (id) {
    case kOptionOnTop:
        set_always_on_top(!always_on_top_);
        return true;
    case kOptionMute:
        set_sounds_muted(!KitSound::muted());
        return true;
    case kOptionSkinLibre:
        change_skin(KitSkin::libre);
        return true;
    case kOptionSkinClassic:
        change_skin(KitSkin::classic);
        return true;
    case kHelpAbout:
        show_about();
        return true;
    default:
        return id >= kKitOptionFirst && id < 0xF000 && on_kit_option(id);
    }
}

LRESULT KitSheet::WindowProc(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_SYSCOMMAND && handle_option(static_cast<UINT>(wparam & 0xFFF0))) {
        return 0;
    }
    if (message == WM_COMMAND && lparam == 0 && HIWORD(wparam) == 0 &&
        handle_option(LOWORD(wparam))) {
        return 0;
    }
    if (message == kSkinChangeMessage) {
        before_skin_change();
        static_cast<KitApp*>(AfxGetApp())->rebuild_main_window(*this);
        return 0;
    }
    return CPropertySheet::WindowProc(message, wparam, lparam);
}

BOOL KitSheet::OnCommand(WPARAM wparam, LPARAM lparam) {
    return CPropertySheet::OnCommand(wparam, lparam);
}

LRESULT KitSheet::OnDeferredLayout(WPARAM, LPARAM) {
    layout_pages();
    return 0;
}

} // namespace c1kitshell
