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
constexpr UINT kMuteCheckbox = 0x7f4d;  // clear of the kits' own control ids

} // namespace

BEGIN_MESSAGE_MAP(KitSheet, CPropertySheet)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_MESSAGE(kDeferredLayoutMessage, &KitSheet::OnDeferredLayout)
END_MESSAGE_MAP()

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

void KitSheet::enable_ambience(c1kit::KitSettings* settings, const char* sound, int volume,
                               bool checkbox) {
    ambience_settings_ = settings;
    ambience_sound_ = sound;
    ambience_volume_ = volume;
    std::uint32_t muted = 0;
    if (settings != nullptr) {
        settings->read_dword(c1kit::SettingsScope::user, "Mute Ambient", muted);
    }
    ambience_muted_ = muted != 0;
    if (checkbox) {
        add_mute_checkbox();
    }
    ambience_ = std::make_unique<KitSound>();
    if (ambience_->open(GetSafeHwnd())) {
        const CString directory = game_directory_setting("Main Directory");
        ambience_->load(ambience_sound_, std::string(CStringA(directory)) + "Sounds\\" +
                                             ambience_sound_ + ".wav");
    }
    apply_ambience();
}

// Under the pages, in a row the window grows by.
void KitSheet::add_mute_checkbox() {
    CWnd* tabs = GetTabControl();
    CRect tab_rect;
    if (tabs != nullptr) {
        tabs->GetWindowRect(&tab_rect);
        ScreenToClient(&tab_rect);
    }
    CRect row(0, 0, 4, 12);  // dialog units: margin, then the checkbox's height
    ::MapDialogRect(GetActivePage() != nullptr ? GetActivePage()->GetSafeHwnd() : GetSafeHwnd(),
                    &row);
    const int height = row.Height();
    CRect window;
    GetWindowRect(&window);
    SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() + height + row.Width(),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    const CString label = _T("Mute ambient sound");
    CClientDC dc(this);
    CFont* previous = dc.SelectObject(GetFont());
    const int width = dc.GetTextExtent(label).cx + height + 8;
    dc.SelectObject(previous);
    mute_check_.Create(label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                       CRect(CPoint(tab_rect.left, tab_rect.bottom + row.Width() / 2),
                             CSize(width, height)),
                       this, kMuteCheckbox);
    mute_check_.SetFont(GetFont());
    mute_check_.SetCheck(ambience_muted_ ? BST_CHECKED : BST_UNCHECKED);
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

void KitSheet::set_ambience_muted(bool muted) {
    ambience_muted_ = muted;
    if (ambience_settings_ != nullptr) {
        ambience_settings_->write_dword("Mute Ambient", muted ? 1u : 0u);
    }
    if (mute_check_.GetSafeHwnd() != nullptr) {
        mute_check_.SetCheck(muted ? BST_CHECKED : BST_UNCHECKED);
    }
    apply_ambience();
}

void KitSheet::apply_ambience() {
    if (!ambience_) {
        return;
    }
    if (ambience_muted_ && ambience_channel_ >= 0) {
        ambience_->stop(ambience_channel_);
        ambience_channel_ = -1;
    } else if (!ambience_muted_ && ambience_channel_ < 0) {
        ambience_channel_ = ambience_->play(ambience_sound_, true, ambience_volume_);
    }
}

BOOL KitSheet::OnCommand(WPARAM wparam, LPARAM lparam) {
    if (LOWORD(wparam) == kMuteCheckbox && HIWORD(wparam) == BN_CLICKED && ambience_) {
        set_ambience_muted(mute_check_.GetCheck() == BST_CHECKED);
        return TRUE;
    }
    return CPropertySheet::OnCommand(wparam, lparam);
}

LRESULT KitSheet::OnDeferredLayout(WPARAM, LPARAM) {
    layout_pages();
    return 0;
}

} // namespace c1kitshell
