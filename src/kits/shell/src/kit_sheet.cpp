// KitSheet: the connected main window shared by the property-sheet kits.

#include "c1kitshell/kit_shell.hpp"

#include "c1kit/conversation.hpp"

#include <cstdio>

namespace c1kitshell {
namespace {

// Posted after a tab switch: comctl32 shows the new page only once the
// sheet's WM_NOTIFY handling returns, and places it at the template size.
constexpr UINT kDeferredLayoutMessage = WM_APP + 0x4c;

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
    // CException::ReportError first; its own fallback when it has nothing
    // to say is MFC's "No error message is available".
    CString message;
    LPTSTR system_text = nullptr;
    if (hresult != 0 &&
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      nullptr, static_cast<DWORD>(hresult), 0,
                      reinterpret_cast<LPTSTR>(&system_text), 0, nullptr) &&
        system_text != nullptr) {
        message = system_text;
        LocalFree(system_text);
    }
    if (message.IsEmpty()) {
        message = _T("Can not communicate with application");
    }
    AfxMessageBox(message);
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
    tab_margins_ = CRect(tab_rect.left - client.left, tab_rect.top - client.top,
                         client.right - tab_rect.right,
                         client.bottom - tab_rect.bottom);
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

LRESULT KitSheet::OnDeferredLayout(WPARAM, LPARAM) {
    layout_pages();
    return 0;
}

} // namespace c1kitshell
