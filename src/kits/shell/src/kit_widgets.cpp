// Widgets for kit pages laid out in code.  See kit_widgets.hpp.

#include "c1kitshell/kit_widgets.hpp"

#include <algorithm>
#include <climits>

namespace c1kitshell {

// ===========================================================================
// PaintedView
// ===========================================================================

BEGIN_MESSAGE_MAP(PaintedView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MOUSELEAVE()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

bool PaintedView::create(CWnd& parent, UINT id, Painter painter) {
    painter_ = std::move(painter);
    const CString window_class = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), nullptr);
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

void PaintedView::set_content_size(CSize size) {
    if (size != content_) {
        content_ = size;
        update_scroll_bars();
    }
}

// Each bar's range is the painting, its page the view; Windows hides a bar
// whose page covers its range.  Only changes are applied, since showing or
// hiding a bar resizes the view and repaints it.
void PaintedView::update_scroll_bars() {
    if (GetSafeHwnd() == nullptr) {
        return;
    }
    CRect client;
    GetClientRect(&client);
    const int extents[2] = {content_.cx, content_.cy};
    const int pages[2] = {client.Width(), client.Height()};
    LONG* positions[2] = {&scroll_.x, &scroll_.y};
    for (int i = 0; i < 2; ++i) {
        const int bar = i == 0 ? SB_HORZ : SB_VERT;
        const int most = extents[i] > pages[i] ? extents[i] - pages[i] : 0;
        *positions[i] = (std::min)((std::max)(*positions[i], 0L), static_cast<LONG>(most));
        SCROLLINFO info = {sizeof(info), SIF_ALL};
        GetScrollInfo(bar, &info, SIF_ALL);
        const int max = most > 0 ? extents[i] - 1 : 0;
        const UINT page = most > 0 ? static_cast<UINT>(pages[i]) : 0;
        if (info.nMin != 0 || info.nMax != max || info.nPage != page ||
            info.nPos != *positions[i]) {
            SCROLLINFO wanted = {sizeof(wanted), SIF_RANGE | SIF_PAGE | SIF_POS, 0, max, page,
                                 static_cast<int>(*positions[i])};
            SetScrollInfo(bar, &wanted, TRUE);
        }
    }
}

void PaintedView::scroll_to(int bar, int position) {
    SCROLLINFO info = {sizeof(info), SIF_ALL};
    GetScrollInfo(bar, &info, SIF_ALL);
    const int most = info.nMax - static_cast<int>(info.nPage) + 1;
    position = (std::max)(0, (std::min)(position, most > 0 ? most : 0));
    LONG& current = bar == SB_HORZ ? scroll_.x : scroll_.y;
    if (position != current) {
        current = position;
        SetScrollPos(bar, position, TRUE);
        redraw();
    }
}

void PaintedView::OnSize(UINT type, int cx, int cy) {
    CWnd::OnSize(type, cx, cy);
    update_scroll_bars();
}

namespace {

int scrolled(int bar_code, int current, int page, int track) {
    switch (bar_code) {
    case SB_LINEUP: return current - 16;
    case SB_LINEDOWN: return current + 16;
    case SB_PAGEUP: return current - page;
    case SB_PAGEDOWN: return current + page;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: return track;
    case SB_TOP: return 0;
    case SB_BOTTOM: return INT_MAX / 2;
    default: return current;
    }
}

} // namespace

void PaintedView::OnHScroll(UINT code, UINT, CScrollBar*) {
    SCROLLINFO info = {sizeof(info), SIF_ALL};
    GetScrollInfo(SB_HORZ, &info, SIF_ALL);
    scroll_to(SB_HORZ, scrolled(code, scroll_.x, static_cast<int>(info.nPage), info.nTrackPos));
}

void PaintedView::OnVScroll(UINT code, UINT, CScrollBar*) {
    SCROLLINFO info = {sizeof(info), SIF_ALL};
    GetScrollInfo(SB_VERT, &info, SIF_ALL);
    scroll_to(SB_VERT, scrolled(code, scroll_.y, static_cast<int>(info.nPage), info.nTrackPos));
}

// The wheel scrolls down and up, or across with Shift (or when the painting
// is only too wide).
BOOL PaintedView::OnMouseWheel(UINT flags, short delta, CPoint) {
    CRect client;
    GetClientRect(&client);
    const bool across = (flags & MK_SHIFT) != 0 || content_.cy <= client.Height();
    const int step = -delta * 48 / WHEEL_DELTA;
    if (across) {
        scroll_to(SB_HORZ, scroll_.x + step);
    } else {
        scroll_to(SB_VERT, scroll_.y + step);
    }
    return TRUE;
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
    if (content_ != CSize(0, 0)) {
        SetFocus();  // for the wheel
    }
    if (mouse_) {
        mouse_(point, true);
    }
    CWnd::OnLButtonDown(flags, point);
}

void PaintedView::OnLButtonDblClk(UINT flags, CPoint point) {
    if (double_click_) {
        double_click_(point);
    }
    CWnd::OnLButtonDblClk(flags, point);
}

void PaintedView::OnMouseLeave() {
    tracking_ = false;
    if (mouse_) {
        mouse_(CPoint(-1, -1), false);
    }
    CWnd::OnMouseLeave();
}

COLORREF series_colour(int index) {
    // The Biochemistry Kit v1.2's eight channel colours (its table at
    // 0x00418a30; the first four are the 1996 Science Kit's too), then more
    // that stay apart on white where it picked random ones.
    static const COLORREF kColours[] = {
        RGB(255, 0, 0),    RGB(0, 0, 255),     RGB(0, 128, 0),
        RGB(128, 0, 128),  RGB(255, 128, 0),   RGB(0, 192, 192),
        RGB(128, 64, 0),   RGB(192, 0, 192),   RGB(100, 100, 100),
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

COLORREF blend(COLORREF a, COLORREF b, int b_percent) {
    const auto mix = [&](int x, int y) { return (x * (100 - b_percent) + y * b_percent) / 100; };
    return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

// ===========================================================================
// LayoutPage
// ===========================================================================

BEGIN_MESSAGE_MAP(LayoutPage, CPropertyPage)
    ON_WM_SIZE()
    ON_BN_CLICKED(IDCLOSE, &LayoutPage::OnCloseKit)
END_MESSAGE_MAP()

LayoutPage::LayoutPage(KitSheet& sheet, UINT blank_dialog, UINT title_string)
    : CPropertyPage(blank_dialog), kit_sheet_(sheet), title_(load_string(title_string)) {
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

BOOL LayoutPage::OnInitDialog() {
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

BOOL LayoutPage::OnSetActive() {
    if (created_) {
        CRect client;
        GetClientRect(&client);
        layout(client.Width(), client.Height());
    }
    return CPropertyPage::OnSetActive();
}

void LayoutPage::OnSize(UINT type, int cx, int cy) {
    CPropertyPage::OnSize(type, cx, cy);
    if (created_ && cx > 0 && cy > 0) {
        layout(cx, cy);
    }
}

CWnd* LayoutPage::make(CWnd& control, LPCTSTR window_class, LPCTSTR text,
                       DWORD style, UINT id, DWORD ex_style) {
    control.CreateEx(ex_style, window_class, text, style | WS_CHILD | WS_VISIBLE,
                     CRect(0, 0, 10, 10), this, id);
    // The page's dialog font: the kits' default font (12 x 6 MS Sans Serif)
    // spaces list and edit text out.
    control.SetFont(GetFont());
    return &control;
}

void LayoutPage::place(CWnd& control, int x, int y, int width, int height) {
    if (control.GetSafeHwnd() != nullptr) {
        control.MoveWindow(x, y, width < 0 ? 0 : width, height < 0 ? 0 : height, TRUE);
    }
}

void LayoutPage::place_dlu(CWnd& control, int x, int y, int width, int height) {
    CRect rect(x, y, x + width, y + height);
    ::MapDialogRect(GetSafeHwnd(), &rect);
    place(control, rect.left, rect.top, rect.Width(), rect.Height());
}

void LayoutPage::place_close(int width, int height) {
    place(close_, width - kMargin - 84, height - kMargin - button_height(), 84,
          button_height());
}

void LayoutPage::OnCloseKit() {
    kit_sheet_.request_game_quit();
}

} // namespace c1kitshell
