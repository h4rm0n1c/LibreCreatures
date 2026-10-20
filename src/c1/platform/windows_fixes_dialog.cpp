#include "windows_fixes_dialog.hpp"

#if defined(LC_COMPAT_RUNTIME)

#include "windows_shell.hpp"
#include "../common/compat.hpp"
#include "../common/fix_catalog.hpp"

#include <afxcmn.h>

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
               WS_SYSMENU);
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
        const std::vector<WORD> words = make_template();
        if (!InitModalIndirect(reinterpret_cast<LPCDLGTEMPLATE>(words.data()),
                               parent_)) {
            return -1;
        }
        return DoModal();
    }

protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetWindowText(_T("LibreFixes"));
        CFont* font = GetFont();

        CRect note_rect(kMargin, kMargin, kWidth - kMargin, kMargin + 18);
        MapDialogRect(&note_rect);
        note_.Create(_T("Fixes for defects in the original game. Off runs ")
                     _T("the original behaviour. docs/original-bugs.md has ")
                     _T("the story on each."),
                     WS_CHILD | WS_VISIBLE | SS_LEFT, note_rect, this,
                     kControlNote);
        note_.SetFont(font);

        const int buttons_top = kHeight - kMargin - kButtonHeight;
        CRect list_rect(kMargin, kMargin + 24, kWidth - kMargin,
                        buttons_top - kMargin);
        MapDialogRect(&list_rect);
        list_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                         LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
                     list_rect, this, kControlList);
        list_.SetExtendedStyle(list_.GetExtendedStyle() |
                               LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        CRect list_client;
        list_.GetClientRect(&list_client);
        list_.InsertColumn(0, _T("Fix"), LVCFMT_LEFT, list_client.Width());

        for (std::size_t i = 0; i < creatures1::common::kFixCatalogSize; ++i) {
            const creatures1::common::FixCatalogEntry& entry =
                creatures1::common::kFixCatalog[i];
            CString text;
            text.Format(_T("%hs: %hs"), entry.category, entry.label);
            const int index = list_.InsertItem(
                static_cast<int>(i), text);
            list_.SetItemData(index, static_cast<DWORD_PTR>(entry.row));
            set_row_checked(list_, index,
                            creatures1::common::compat_fixed(entry.row));
        }
        list_.SetFont(font);

        CRect reset_rect(kMargin, buttons_top, kMargin + kResetButtonWidth,
                         buttons_top + kButtonHeight);
        MapDialogRect(&reset_rect);
        reset_.Create(_T("Reset to recommended"),
                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      reset_rect, this, kControlReset);
        reset_.SetFont(font);

        CRect cancel_rect(kWidth - kMargin - kButtonWidth * 2 - kMargin,
                          buttons_top, kWidth - kMargin - kButtonWidth,
                          buttons_top + kButtonHeight);
        MapDialogRect(&cancel_rect);
        cancel_.Create(_T("Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                         BS_PUSHBUTTON,
                       cancel_rect, this, IDCANCEL);
        cancel_.SetFont(font);

        CRect ok_rect(kWidth - kMargin - kButtonWidth, buttons_top,
                      kWidth - kMargin, buttons_top + kButtonHeight);
        MapDialogRect(&ok_rect);
        ok_.Create(_T("OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 BS_DEFPUSHBUTTON,
                   ok_rect, this, IDOK);
        ok_.SetFont(font);
        return TRUE;
    }

    // Changes only the checkboxes shown; nothing is applied or written
    // until OK, so this is safe to try and back out of with Cancel.
    afx_msg void OnReset() {
        for (int index = 0; index < list_.GetItemCount(); ++index) {
            const int row = static_cast<int>(list_.GetItemData(index));
            const creatures1::common::FixCatalogEntry* entry =
                creatures1::common::find_fix_catalog_entry(row);
            if (entry != nullptr) {
                set_row_checked(list_, index, !entry->changes_simulation);
            }
        }
    }

    void OnOK() override {
        bool changed = false;
        for (int index = 0; index < list_.GetItemCount(); ++index) {
            const int row = static_cast<int>(list_.GetItemData(index));
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
};

BEGIN_MESSAGE_MAP(FixesDialog, CDialog)
    ON_BN_CLICKED(kControlReset, &FixesDialog::OnReset)
END_MESSAGE_MAP()

} // namespace

void show_fixes_dialog(CWnd* parent) {
    FixesDialog dialog(parent);
    dialog.run();
}

} // namespace creatures1::platform

#endif // LC_COMPAT_RUNTIME
