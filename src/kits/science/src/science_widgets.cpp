// The Science Kit's drawn views and the page base its pages share.

#include "science.hpp"
#include "science_ids.hpp"

namespace science {

// ===========================================================================
// PaintedView
// ===========================================================================

BEGIN_MESSAGE_MAP(PaintedView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

bool PaintedView::create(CWnd& parent, UINT id, Painter painter) {
    painter_ = std::move(painter);
    const CString window_class = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), nullptr);
    return CreateEx(0, window_class, _T(""), WS_CHILD | WS_VISIBLE,
                    CRect(0, 0, 10, 10), &parent, id) != FALSE;
}

void PaintedView::OnPaint() {
    CPaintDC paint(this);
    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty()) {
        return;
    }
    CDC memory;
    memory.CreateCompatibleDC(&paint);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&paint, client.Width(), client.Height());
    CBitmap* previous_bitmap = memory.SelectObject(&bitmap);
    CFont* previous_font = memory.SelectObject(GetParent()->GetFont());
    if (painter_) {
        painter_(memory, client);
    }
    paint.BitBlt(0, 0, client.Width(), client.Height(), &memory, 0, 0, SRCCOPY);
    memory.SelectObject(previous_font);
    memory.SelectObject(previous_bitmap);
}

void PaintedView::OnMouseMove(UINT flags, CPoint point) {
    if (!tracking_) {
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, GetSafeHwnd(), 0};
        tracking_ = ::TrackMouseEvent(&track) != FALSE;
    }
    if (mouse_) {
        mouse_(point, false);
    }
    CWnd::OnMouseMove(flags, point);
}

void PaintedView::OnLButtonDown(UINT flags, CPoint point) {
    if (mouse_) {
        mouse_(point, true);
    }
    CWnd::OnLButtonDown(flags, point);
}

void PaintedView::OnMouseLeave() {
    tracking_ = false;
    if (mouse_) {
        mouse_(CPoint(-1, -1), false);
    }
    CWnd::OnMouseLeave();
}

COLORREF series_colour(int index) {
    // The 1996 kit's four (red, blue, green, purple: bitmaps 176-179), then
    // more that stay apart on white.
    static const COLORREF kColours[] = {
        RGB(230, 0, 0),    RGB(0, 0, 230),     RGB(0, 150, 0),
        RGB(140, 0, 160),  RGB(230, 120, 0),   RGB(0, 160, 170),
        RGB(160, 100, 40), RGB(230, 0, 160),   RGB(100, 100, 100),
        RGB(120, 170, 0),  RGB(0, 90, 150),    RGB(200, 60, 60),
        RGB(80, 40, 160),  RGB(0, 120, 80),    RGB(200, 170, 0),
        RGB(20, 20, 20)};
    return kColours[static_cast<unsigned>(index) % (sizeof(kColours) / sizeof(kColours[0]))];
}

COLORREF lobe_colour(int lobe) {
    static const COLORREF kColours[] = {
        RGB(80, 160, 255),  // perception
        RGB(255, 90, 90),   // drive
        RGB(255, 170, 60),  // stimulus source
        RGB(120, 220, 120), // verb
        RGB(60, 200, 200),  // noun
        RGB(230, 120, 230), // general sense
        RGB(255, 230, 70),  // decision
        RGB(170, 140, 255), // attention
        RGB(150, 150, 150), // concept
    };
    return kColours[static_cast<unsigned>(lobe) % (sizeof(kColours) / sizeof(kColours[0]))];
}

// ===========================================================================
// SciencePage
// ===========================================================================

BEGIN_MESSAGE_MAP(SciencePage, CPropertyPage)
    ON_WM_SIZE()
    ON_BN_CLICKED(IDCLOSE, &SciencePage::OnCloseKit)
END_MESSAGE_MAP()

SciencePage::SciencePage(ScienceSheet& sheet, UINT title_string)
    : CPropertyPage(kDialogPage),
      sheet_(sheet),
      title_(c1kitshell::load_string(title_string)) {
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

BOOL SciencePage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    CRect unit(0, 0, 4, 8);
    MapDialogRect(&unit);
    text_height_ = unit.Height() + 5;
    make(close_, _T("BUTTON"), _T("Close"), BS_PUSHBUTTON | WS_TABSTOP, IDCLOSE);
    create_controls();
    created_ = true;
    CRect client;
    GetClientRect(&client);
    layout(client.Width(), client.Height());
    return TRUE;
}

BOOL SciencePage::OnSetActive() {
    if (created_) {
        CRect client;
        GetClientRect(&client);
        layout(client.Width(), client.Height());
    }
    return CPropertyPage::OnSetActive();
}

void SciencePage::OnSize(UINT type, int cx, int cy) {
    CPropertyPage::OnSize(type, cx, cy);
    if (created_ && cx > 0 && cy > 0) {
        layout(cx, cy);
    }
}

CWnd* SciencePage::make(CWnd& control, LPCTSTR window_class, LPCTSTR text,
                        DWORD style, UINT id, DWORD ex_style) {
    control.CreateEx(ex_style, window_class, text, style | WS_CHILD | WS_VISIBLE,
                     CRect(0, 0, 10, 10), this, id);
    // The page's dialog font: the kits' default font (12 x 6 MS Sans Serif)
    // spaces list and edit text out.
    control.SetFont(GetFont());
    return &control;
}

void SciencePage::place(CWnd& control, int x, int y, int width, int height) {
    if (control.GetSafeHwnd() != nullptr) {
        control.MoveWindow(x, y, width < 0 ? 0 : width, height < 0 ? 0 : height, TRUE);
    }
}

void SciencePage::OnCloseKit() {
    sheet_.request_game_quit();
}

} // namespace science
