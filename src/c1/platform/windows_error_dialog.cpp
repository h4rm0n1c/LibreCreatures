#include "windows_error_dialog.hpp"

#include <cstring>
#include <vector>

namespace creatures1::platform {
namespace {

constexpr UINT kControlIcon = 1001;
constexpr UINT kControlSummary = 1002;
constexpr UINT kControlDetails = 1003;
constexpr UINT kControlCopy = 1004;

// The dialog's size in dialog units, and its controls' places.
constexpr int kWidth = 280;
constexpr int kHeight = 150;
constexpr int kMargin = 7;
constexpr int kButtonWidth = 60;
constexpr int kButtonHeight = 14;

// Windows line breaks, for the edit box and the clipboard.
CString windows_lines(const std::string& text) {
    CString out;
    for (const char character : text) {
        if (character == '\n') {
            out += _T("\r\n");
        } else if (character != '\r') {
            out += static_cast<TCHAR>(static_cast<unsigned char>(character));
        }
    }
    return out;
}

// An empty frame (no controls; OnInitDialog makes them) in the shell
// dialog font, so the resource script needs no new entry.
std::vector<WORD> make_template() {
    std::vector<WORD> words;
    const auto push_dword = [&words](DWORD value) {
        words.push_back(LOWORD(value));
        words.push_back(HIWORD(value));
    };
    push_dword(DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION |
               WS_SYSMENU);
    push_dword(0);                       // extended style
    words.push_back(0);                  // controls
    words.push_back(0);                  // x
    words.push_back(0);                  // y
    words.push_back(kWidth);
    words.push_back(kHeight);
    words.push_back(0);                  // no menu
    words.push_back(0);                  // default class
    words.push_back(0);                  // title, set later
    words.push_back(8);                  // point size
    for (const wchar_t* face = L"MS Shell Dlg"; *face != L'\0'; ++face) {
        words.push_back(static_cast<WORD>(*face));
    }
    words.push_back(0);
    return words;
}

class ErrorReportDialog : public CDialog {
public:
    ErrorReportDialog(CWnd* parent, const std::string& title,
                      const std::string& summary, const std::string& details)
        : CDialog(), parent_(parent), title_(title.c_str()),
          summary_text_(windows_lines(summary)),
          details_text_(windows_lines(details)) {}

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
        SetWindowText(title_);
        CFont* font = GetFont();

        CRect icon_rect(kMargin, kMargin, kMargin + 20, kMargin + 20);
        MapDialogRect(&icon_rect);
        icon_.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_ICON, icon_rect, this,
                     kControlIcon);
        icon_.SetIcon(::LoadIcon(nullptr, IDI_WARNING));

        CRect summary_rect(kMargin + 26, kMargin + 4, kWidth - kMargin,
                           kMargin + 20);
        MapDialogRect(&summary_rect);
        summary_.Create(summary_text_, WS_CHILD | WS_VISIBLE | SS_LEFT,
                        summary_rect, this, kControlSummary);
        summary_.SetFont(font);

        const int buttons_top = kHeight - kMargin - kButtonHeight;
        CRect details_rect(kMargin, kMargin + 26, kWidth - kMargin,
                           buttons_top - kMargin);
        MapDialogRect(&details_rect);
        details_.CreateEx(WS_EX_CLIENTEDGE, _T("EDIT"), details_text_,
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                              ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                          details_rect, this, kControlDetails);
        details_.SetFont(font);

        CRect copy_rect(kMargin, buttons_top, kMargin + kButtonWidth,
                        buttons_top + kButtonHeight);
        MapDialogRect(&copy_rect);
        copy_.Create(_T("&Copy"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                      BS_PUSHBUTTON,
                     copy_rect, this, kControlCopy);
        copy_.SetFont(font);

        CRect ok_rect(kWidth - kMargin - kButtonWidth, buttons_top,
                      kWidth - kMargin, buttons_top + kButtonHeight);
        MapDialogRect(&ok_rect);
        ok_.Create(_T("OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 BS_DEFPUSHBUTTON,
                   ok_rect, this, IDOK);
        ok_.SetFont(font);

        ::MessageBeep(MB_ICONWARNING);
        ok_.SetFocus();
        return FALSE;
    }

    afx_msg void OnCopy() {
        const CStringA text(summary_text_ + _T("\r\n\r\n") + details_text_);
        if (!OpenClipboard()) {
            return;
        }
        ::EmptyClipboard();
        const SIZE_T bytes = static_cast<SIZE_T>(text.GetLength()) + 1;
        if (HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* destination = ::GlobalLock(memory)) {
                std::memcpy(destination, text.GetString(), bytes);
                ::GlobalUnlock(memory);
                if (::SetClipboardData(CF_TEXT, memory) == nullptr) {
                    ::GlobalFree(memory);
                }
            } else {
                ::GlobalFree(memory);
            }
        }
        ::CloseClipboard();
    }

    DECLARE_MESSAGE_MAP()

private:
    CWnd* parent_;
    CString title_;
    CString summary_text_;
    CString details_text_;
    CStatic icon_;
    CStatic summary_;
    CEdit details_;
    CButton copy_;
    CButton ok_;
};

BEGIN_MESSAGE_MAP(ErrorReportDialog, CDialog)
    ON_BN_CLICKED(kControlCopy, &ErrorReportDialog::OnCopy)
END_MESSAGE_MAP()

} // namespace

namespace {

// Takes any WM_QUIT out of the thread's queue; true, and its exit code, if
// there was one.
bool take_pending_quit(int& exit_code) {
    bool found = false;
    MSG message{};
    while (::PeekMessage(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
        exit_code = static_cast<int>(message.wParam);
        found = true;
    }
    return found;
}

} // namespace

// The report must stay up until it is dismissed.  It is most needed while
// the game closes -- a world that will not save on exit -- and by then the
// game has stopped the kits and revoked its OLE factories, and MFC queues
// WM_QUIT as the last OLE object goes.  A modal loop that meets WM_QUIT ends
// at once, so the report flashed and vanished.  The quit is held back while
// the report is up and posted again once it has been dismissed.
void show_error_report(CWnd* parent, const std::string& title,
                       const std::string& summary, const std::string& details) {
    int quit_code = 0;
    bool quit_pending = take_pending_quit(quit_code);
    INT_PTR result = -1;
    for (int attempt = 0; attempt < 8; ++attempt) {
        ErrorReportDialog dialog(parent, title, summary, details);
        result = dialog.run();
        if (result == IDOK || result == IDCANCEL) {
            break;
        }
        // Ended by a quit that arrived while it was up, or never made.
        if (!take_pending_quit(quit_code)) {
            break;
        }
        quit_pending = true;
    }
    if (result != IDOK && result != IDCANCEL) {
        // The dialog could not be made: the plain box still says it all.
        quit_pending = take_pending_quit(quit_code) || quit_pending;
        AfxMessageBox(CString(summary.c_str()) + _T("\n\n") +
                          CString(details.c_str()),
                      MB_ICONWARNING);
    }
    if (quit_pending) {
        ::PostQuitMessage(quit_code);
    }
}

} // namespace creatures1::platform
