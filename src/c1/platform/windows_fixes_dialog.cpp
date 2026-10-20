#include "windows_fixes_dialog.hpp"

#if defined(LC_COMPAT_RUNTIME)

#include "windows_shell.hpp"
#include "../common/compat.hpp"
#include "../common/fix_catalog.hpp"
#include "../common/lab_trace.hpp"

#include <afxcmn.h>

#include <chrono>
#include <vector>

namespace creatures1::platform {
namespace {

constexpr UINT kControlList = 1001;
constexpr UINT kControlReset = 1002;
constexpr UINT kControlNote = 1003;

constexpr int kWidth = 340;
constexpr int kHeight = 240;
constexpr int kMargin = 7;
constexpr int kButtonWidth = 60;
constexpr int kButtonHeight = 14;
constexpr int kResetButtonWidth = 110;

// Checked/unchecked through CListCtrl's own item-state API (no listview
// macro header needed beyond afxcmn.h, which the whole project already
// includes): state image 1 is the box empty, 2 is checked.
void set_row_checked(CListCtrl& list, int index, bool checked) {
    list.SetItemState(index, INDEXTOSTATEIMAGEMASK(checked ? 2 : 1),
                      LVIS_STATEIMAGEMASK);
}

bool row_checked(CListCtrl& list, int index) {
    return ((list.GetItemState(index, LVIS_STATEIMAGEMASK) >> 12) - 1) == 1;
}

// An empty frame; OnInitDialog makes the controls, as windows_error_dialog
// does, so there is no .rc entry (and so no per-language dialog work) for
// this window.
std::vector<WORD> make_template() {
    std::vector<WORD> words;
    const auto push_dword = [&words](DWORD value) {
        words.push_back(LOWORD(value));
        words.push_back(HIWORD(value));
    };
    push_dword(DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION |
               WS_SYSMENU | WS_THICKFRAME);
    push_dword(0);
    words.push_back(0);
    words.push_back(0);
    words.push_back(0);
    words.push_back(kWidth);
    words.push_back(kHeight);
    words.push_back(0);
    words.push_back(0);
    words.push_back(0);
    words.push_back(8);
    for (const wchar_t* face = L"MS Shell Dlg"; *face != L'\0'; ++face) {
        words.push_back(static_cast<WORD>(*face));
    }
    words.push_back(0);
    return words;
}

class FixesDialog : public CDialog {
public:
    explicit FixesDialog(CWnd* parent) : CDialog(), parent_(parent) {}

    INT_PTR run() {
        // Not native: InitModalIndirect actually creates the HWND;
        // lab_trace's own timestamp column (already on every line, the same
        // clock everywhere) lets this be compared against OnLibreFixes'
        // "command handler entered" line (before this) and OnInitDialog's
        // "begin"/"end" lines (during DoModal, below) to see where time
        // goes when opening is slow even with no slow world tick nearby.
        const auto start = std::chrono::steady_clock::now();
        const std::vector<WORD> words = make_template();
        if (!InitModalIndirect(reinterpret_cast<LPCDLGTEMPLATE>(words.data()),
                               parent_)) {
            return -1;
        }
        const double init_ms = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        creatures1::common::lab_trace(
            creatures1::common::LabTrace::timing,
            "LibreFixes: InitModalIndirect took %.1fms", init_ms);
        return DoModal();
    }

protected:
    BOOL OnInitDialog() override {
        creatures1::common::lab_trace(creatures1::common::LabTrace::timing,
                                      "LibreFixes: OnInitDialog begin");
        const auto start = std::chrono::steady_clock::now();
        CDialog::OnInitDialog();
        SetWindowText(_T("LibreFixes"));
        CFont* font = GetFont();

        // Pixel margins, converted once from the template's dialog units;
        // layout() below repeats with these for every size the window is
        // resized to, dialog units only make sense against the original
        // template rect.
        CRect margin_rect(0, 0, kMargin, kButtonHeight);
        MapDialogRect(&margin_rect);
        margin_px_ = margin_rect.left;
        button_height_px_ = margin_rect.Height();
        CRect width_rect(0, 0, kButtonWidth, 18);
        MapDialogRect(&width_rect);
        button_width_px_ = width_rect.Width();
        note_height_px_ = width_rect.Height();
        CRect reset_width_rect(0, 0, kResetButtonWidth, 0);
        MapDialogRect(&reset_width_rect);
        reset_button_width_px_ = reset_width_rect.Width();

        note_.Create(_T("Fixes for defects in the original game. Off runs ")
                     _T("the original behaviour. docs/original-bugs.md has ")
                     _T("the story on each."),
                     WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(), this,
                     kControlNote);
        note_.SetFont(font);

        list_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                         LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
                     CRect(), this, kControlList);
        list_.SetExtendedStyle(list_.GetExtendedStyle() |
                               LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        list_.SetFont(font);
        list_.InsertColumn(0, _T("Fix"), LVCFMT_LEFT, kWidth);

        // Not native: WM_SETREDRAW off for the whole insert -- otherwise a
        // visible, LVS_EX_CHECKBOXES-styled CListCtrl relays out and
        // repaints on every single InsertItem, turning 62 rows into tens of
        // milliseconds (measured: ~80ms) every time this dialog opens.
        list_.SetRedraw(FALSE);
        for (std::size_t i = 0; i < creatures1::common::kFixCatalogSize; ++i) {
            const creatures1::common::FixCatalogEntry& entry =
                creatures1::common::kFixCatalog[i];
            CString text;
            if (entry.tier == creatures1::common::FixTier::locked) {
                text.Format(_T("%hs: %hs (always on)"), entry.category,
                           entry.label);
            } else {
                text.Format(_T("%hs: %hs"), entry.category, entry.label);
            }
            const int index = list_.InsertItem(
                static_cast<int>(i), text);
            list_.SetItemData(index, static_cast<DWORD_PTR>(entry.row));
            set_row_checked(list_, index,
                            creatures1::common::compat_fixed(entry.row));
        }
        list_.SetRedraw(TRUE);

        reset_.Create(_T("Reset to recommended"),
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      CRect(), this, kControlReset);
        reset_.SetFont(font);

        cancel_.Create(_T("Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                         BS_PUSHBUTTON,
                       CRect(), this, IDCANCEL);
        cancel_.SetFont(font);

        ok_.Create(_T("OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 BS_DEFPUSHBUTTON,
                   CRect(), this, IDOK);
        ok_.SetFont(font);

        CRect client;
        GetClientRect(&client);
        min_size_ = client.Size();
        layout(client.Width(), client.Height());
        const double body_ms = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        creatures1::common::lab_trace(
            creatures1::common::LabTrace::timing,
            "LibreFixes: OnInitDialog end (body took %.1fms)", body_ms);
        return TRUE;
    }

    // Repositions every control for a client area of `width` by `height`
    // pixels: called once from OnInitDialog (the template's own size) and
    // again on every WM_SIZE, so the window can be resized (the list is the
    // one control worth more room; the note and buttons just follow the
    // edges they're pinned to).
    void layout(int width, int height) {
        if (!::IsWindow(note_.GetSafeHwnd())) {
            return;
        }
        const int buttons_top = height - margin_px_ - button_height_px_;
        note_.MoveWindow(margin_px_, margin_px_, width - margin_px_ * 2,
                         note_height_px_);
        const int list_top = margin_px_ + note_height_px_ + margin_px_ / 2;
        list_.MoveWindow(margin_px_, list_top, width - margin_px_ * 2,
                         buttons_top - margin_px_ - list_top);
        CRect list_client;
        list_.GetClientRect(&list_client);
        list_.SetColumnWidth(0, list_client.Width());
        reset_.MoveWindow(margin_px_, buttons_top, reset_button_width_px_,
                          button_height_px_);
        cancel_.MoveWindow(width - margin_px_ - button_width_px_ * 2 - margin_px_,
                           buttons_top, button_width_px_, button_height_px_);
        ok_.MoveWindow(width - margin_px_ - button_width_px_, buttons_top,
                      button_width_px_, button_height_px_);
    }

    afx_msg void OnSize(UINT type, int cx, int cy) {
        CDialog::OnSize(type, cx, cy);
        layout(cx, cy);
    }

    afx_msg void OnGetMinMaxInfo(MINMAXINFO* info) {
        if (min_size_.cx > 0 && min_size_.cy > 0) {
            info->ptMinTrackSize.x = min_size_.cx;
            info->ptMinTrackSize.y = min_size_.cy;
        }
    }

    // Changes only the checkboxes shown; nothing is applied or written
    // until OK, so this is safe to try and back out of with Cancel.  A
    // locked row is already checked and stays checked.
    afx_msg void OnReset() {
        for (int index = 0; index < list_.GetItemCount(); ++index) {
            const int row = static_cast<int>(list_.GetItemData(index));
            const creatures1::common::FixCatalogEntry* entry =
                creatures1::common::find_fix_catalog_entry(row);
            if (entry != nullptr) {
                set_row_checked(
                    list_, index,
                    creatures1::common::fix_tier_default_fixed(entry->tier));
            }
        }
    }

    // A locked row's checkbox cannot be unchecked: veto any state-image
    // change the list control is about to make to it.  The row is still a
    // normal list item otherwise (selectable, readable), so this is the one
    // targeted way to stop just the checkbox without disabling the control.
    afx_msg void OnListItemChanging(NMHDR* header, LRESULT* result) {
        *result = FALSE;
        const auto* view = reinterpret_cast<NMLISTVIEW*>(header);
        // Only a transition away from checked is ever vetoed: the
        // population loop's own first SetItemState checks a locked row
        // (old state has no image yet) and must go through, same as
        // Reset's.  Only a later attempt -- a click -- to uncheck it is
        // blocked.
        if ((view->uChanged & LVIF_STATE) == 0 ||
            (view->uNewState & LVIS_STATEIMAGEMASK) ==
                INDEXTOSTATEIMAGEMASK(2)) {
            return;
        }
        const int row = static_cast<int>(list_.GetItemData(view->iItem));
        const creatures1::common::FixCatalogEntry* entry =
            creatures1::common::find_fix_catalog_entry(row);
        *result = entry != nullptr &&
                 entry->tier == creatures1::common::FixTier::locked;
    }

    // Greys a locked row's text so "always on" reads as disabled, not just
    // a label.
    afx_msg void OnListCustomDraw(NMHDR* header, LRESULT* result) {
        auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
        switch (draw->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            *result = CDRF_NOTIFYITEMDRAW;
            return;
        case CDDS_ITEMPREPAINT: {
            const int row = static_cast<int>(
                list_.GetItemData(static_cast<int>(draw->nmcd.dwItemSpec)));
            const creatures1::common::FixCatalogEntry* entry =
                creatures1::common::find_fix_catalog_entry(row);
            if (entry != nullptr &&
                entry->tier == creatures1::common::FixTier::locked) {
                draw->clrText = ::GetSysColor(COLOR_GRAYTEXT);
            }
            *result = CDRF_DODEFAULT;
            return;
        }
        default:
            *result = CDRF_DODEFAULT;
            return;
        }
    }

    // A locked row never reaches set_compat_fixed: OnListItemChanging
    // already keeps its checkbox at the default, but skipping it here too
    // keeps the intent explicit rather than relying only on that veto.
    void OnOK() override {
        bool changed = false;
        for (int index = 0; index < list_.GetItemCount(); ++index) {
            const int row = static_cast<int>(list_.GetItemData(index));
            const creatures1::common::FixCatalogEntry* entry =
                creatures1::common::find_fix_catalog_entry(row);
            if (entry != nullptr &&
                entry->tier == creatures1::common::FixTier::locked) {
                continue;
            }
            const bool checked = row_checked(list_, index);
            if (checked != creatures1::common::compat_fixed(row)) {
                creatures1::common::set_compat_fixed(row, checked);
                changed = true;
            }
        }
        if (changed) {
            write_fix_config(fix_config_path());
        }
        CDialog::OnOK();
    }

    DECLARE_MESSAGE_MAP()

private:
    CWnd* parent_;
    CStatic note_;
    CListCtrl list_;
    CButton reset_;
    CButton cancel_;
    CButton ok_;
    int margin_px_ = 0;
    int button_width_px_ = 0;
    int button_height_px_ = 0;
    int reset_button_width_px_ = 0;
    int note_height_px_ = 0;
    CSize min_size_{0, 0};
};

BEGIN_MESSAGE_MAP(FixesDialog, CDialog)
    ON_BN_CLICKED(kControlReset, &FixesDialog::OnReset)
    ON_NOTIFY(LVN_ITEMCHANGING, kControlList, &FixesDialog::OnListItemChanging)
    ON_NOTIFY(NM_CUSTOMDRAW, kControlList, &FixesDialog::OnListCustomDraw)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

} // namespace

void show_fixes_dialog(CWnd* parent) {
    FixesDialog dialog(parent);
    dialog.run();
}

} // namespace creatures1::platform

#endif // LC_COMPAT_RUNTIME
