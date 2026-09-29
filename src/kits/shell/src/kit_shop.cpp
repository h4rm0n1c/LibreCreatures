// The shared shop page.  See kit_shop.hpp.

#include "c1kitshell/kit_shop.hpp"

#include <algorithm>

namespace c1kitshell {
namespace {

enum : UINT {
    kControlShopShelf = 3900,
    kControlShopStatus,
};

// The shelf's look.
constexpr int kCardGap = 10;
constexpr int kCardPadding = 10;
constexpr int kMinimumCardWidth = 140;
constexpr int kMaximumCardWidth = 260;
constexpr int kMaximumPictureScale = 3;
constexpr COLORREF kCardFace = RGB(255, 255, 255);
constexpr COLORREF kCardEdge = RGB(200, 200, 205);
constexpr COLORREF kCardEdgeHot = RGB(0, 120, 215);
constexpr COLORREF kWell = RGB(24, 24, 28);
constexpr COLORREF kNameColour = RGB(20, 40, 90);
constexpr COLORREF kWordsColour = RGB(90, 90, 90);
constexpr COLORREF kStockColour = RGB(40, 120, 60);
constexpr COLORREF kSoldOutColour = RGB(200, 40, 40);

CString text(const std::string& value) {
    return CString(value.c_str());
}

CString stock_text(int quantity) {
    CString line;
    if (quantity > 0) {
        line.Format(_T("%d left"), quantity);
    } else {
        line = _T("None left");
    }
    return line;
}

const TCHAR kPutOne[] = _T("Put one in the world");

} // namespace

ShopPage::ShopPage(KitSheet& sheet, ShopHost& host, UINT blank_dialog, UINT title_string)
    : LayoutPage(sheet, blank_dialog, title_string), host_(host) {}

void ShopPage::create_controls() {
    LOGFONT font = {};
    GetFont()->GetLogFont(&font);
    font.lfWeight = FW_BOLD;
    name_font_.CreateFontIndirect(&font);
    shelf_.create(*this, kControlShopShelf,
                  [this](CDC& dc, const CRect& rect) { draw_shelf(dc, rect); });
    shelf_.set_mouse_handler([this](CPoint point, bool clicked) { on_mouse(point, clicked); });
    make(status_, _T("STATIC"), _T(""), SS_LEFT | SS_ENDELLIPSIS, kControlShopStatus);
}

void ShopPage::layout(int width, int height) {
    const int bottom = height - 2 * kMargin - button_height();
    place(shelf_, kMargin, kMargin, width - 2 * kMargin,
          bottom - 2 * kMargin - text_height());
    place(status_, kMargin, bottom - text_height(), width - 2 * kMargin, text_height());
    place_close(width, height);
    arrange_cards();
}

void ShopPage::refresh() {
    if (created_) {
        arrange_cards();
        shelf_.redraw();
    }
}

int ShopPage::picture_scale(const c1kit::ShopItem& item) const {
    const c1kit::PhotoBitmap& picture = item.picture;
    if (picture.width <= 0 || picture.height <= 0) {
        return 0;
    }
    return (std::max)(1, (std::min)(kMaximumPictureScale,
                                    (card_width_ - 2 * kCardPadding) / picture.width));
}

void ShopPage::arrange_cards() {
    cards_.clear();
    buttons_.clear();
    if (shelf_.GetSafeHwnd() == nullptr) {
        return;
    }
    const std::vector<c1kit::ShopItem>& items = host_.shop_items();
    CClientDC dc(&shelf_);
    CFont* previous = dc.SelectObject(GetFont());
    // Laid out once to see whether a scroll bar is needed, then again in the
    // width that leaves.
    CRect client;
    shelf_.GetClientRect(&client);
    int available = client.Width() + (shelf_.GetStyle() & WS_VSCROLL
                                          ? ::GetSystemMetrics(SM_CXVSCROLL)
                                          : 0);
    int content_height = 0;
    for (int pass = 0; pass < 2; ++pass) {
        cards_.clear();
        buttons_.clear();
        // As many columns as fit, but no more than there are items, so a
        // short shelf is centred rather than left half empty.
        const int columns = (std::max)(
            1, (std::min)(static_cast<int>(items.size()),
                          (available - kCardGap) / (kMinimumCardWidth + kCardGap)));
        card_width_ = (std::min)(kMaximumCardWidth,
                                 (available - kCardGap * (columns + 1)) / columns);
        picture_height_ = 0;
        int words_height = 0;
        for (const c1kit::ShopItem& item : items) {
            const int scale = picture_scale(item);
            picture_height_ = (std::max)(picture_height_, item.picture.height * scale);
            CRect words(0, 0, card_width_ - 2 * kCardPadding, 0);
            dc.DrawText(text(item.description), words, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            words_height = (std::max)(words_height, words.Height());
        }
        picture_height_ += 2 * kCardPadding;
        const int line = text_height();
        const int card_height = kCardPadding + picture_height_ + kCardPadding + line + 4 +
                                words_height + 6 + line + 8 + button_height() + kCardPadding;
        // Centred as a block when there is room to spare.
        const int used = columns * card_width_ + (columns - 1) * kCardGap;
        const int left = (std::max)(kCardGap, (available - used) / 2);
        for (std::size_t i = 0; i < items.size(); ++i) {
            const int column = static_cast<int>(i) % columns;
            const int row = static_cast<int>(i) / columns;
            const int x = left + column * (card_width_ + kCardGap);
            const int y = kCardGap + row * (card_height + kCardGap);
            const CRect card(x, y, x + card_width_, y + card_height);
            cards_.push_back(card);
            buttons_.emplace_back(card.left + kCardPadding,
                                  card.bottom - kCardPadding - button_height(),
                                  card.right - kCardPadding, card.bottom - kCardPadding);
        }
        const int rows = items.empty() ? 0 : (static_cast<int>(items.size()) + columns - 1) / columns;
        content_height = kCardGap + rows * (card_height + kCardGap);
        if (content_height <= client.Height() || pass == 1) {
            break;
        }
        available -= ::GetSystemMetrics(SM_CXVSCROLL);
    }
    dc.SelectObject(previous);
    shelf_.set_content_size(content_height > client.Height() ? CSize(0, content_height)
                                                              : CSize(0, 0));
}

void ShopPage::draw_shelf(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, ::GetSysColor(COLOR_3DFACE));
    dc.SetBkMode(TRANSPARENT);
    const std::vector<c1kit::ShopItem>& items = host_.shop_items();
    if (items.empty()) {
        dc.SetTextColor(kWordsColour);
        dc.TextOut(rect.left + 12, rect.top + 12,
                   CString(_T("The shop file (")) + CString(host_.shop_file_name()) +
                       _T(") could not be read, or has nothing in it."));
        return;
    }
    const CPoint scroll = shelf_.scroll_position();
    for (std::size_t i = 0; i < cards_.size() && i < items.size(); ++i) {
        CRect card = cards_[i];
        card.OffsetRect(-scroll);
        if (card.bottom >= rect.top && card.top <= rect.bottom) {
            draw_card(dc, i, card);
        }
    }
}

void ShopPage::draw_card(CDC& dc, std::size_t index, const CRect& card) {
    const c1kit::ShopItem& item = host_.shop_items()[index];
    const bool hot = static_cast<int>(index) == hover_;
    const bool in_stock = item.quantity > 0;
    // The button where this card is drawn (the layout is unscrolled).
    CRect button = buttons_[index];
    button.OffsetRect(card.TopLeft() - cards_[index].TopLeft());

    // The card, its edge lit while its button is under the mouse.
    CPen edge(PS_SOLID, 1, hot && in_stock ? kCardEdgeHot : kCardEdge);
    CBrush face(kCardFace);
    CPen* previous_pen = dc.SelectObject(&edge);
    CBrush* previous_brush = dc.SelectObject(&face);
    dc.RoundRect(card, CPoint(8, 8));
    dc.SelectObject(previous_brush);
    dc.SelectObject(previous_pen);

    // The picture in a well of its own background colour (its corner
    // pixel), so the art's frame doesn't show.
    const CRect well(card.left + kCardPadding, card.top + kCardPadding,
                     card.right - kCardPadding, card.top + kCardPadding + picture_height_);
    const c1kit::PhotoBitmap& picture = item.picture;
    COLORREF ground = kWell;
    if (picture.width > 0 && picture.height > 0 &&
        picture.pixels.size() >= static_cast<std::size_t>(picture.stride)) {
        const std::uint32_t colour = host_.shop_palette().colour(picture.pixels[0]);
        ground = RGB((colour >> 16) & 0xff, (colour >> 8) & 0xff, colour & 0xff);
    }
    dc.FillSolidRect(well, ground);
    const int scale = picture_scale(item);
    if (scale > 0 && canvas_.create(picture.width, picture.height)) {
        canvas_.fill(0);
        canvas_.draw_indexed(picture.pixels.data(), picture.width, picture.height, picture.stride,
                             true, 0, 0, host_.shop_palette());
        CDC source;
        source.CreateCompatibleDC(&dc);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(&dc, picture.width, picture.height);
        CBitmap* previous = source.SelectObject(&bitmap);
        canvas_.present(source, 0, 0, picture.width, picture.height);
        const int w = picture.width * scale;
        const int h = picture.height * scale;
        dc.SetStretchBltMode(COLORONCOLOR);
        dc.StretchBlt(well.left + (well.Width() - w) / 2, well.top + (well.Height() - h) / 2, w,
                      h, &source, 0, 0, picture.width, picture.height, SRCCOPY);
        source.SelectObject(previous);
    }

    // Name, what it does, how many are left.
    const int line = text_height();
    int y = well.bottom + kCardPadding;
    CFont* previous_font = dc.SelectObject(&name_font_);
    dc.SetTextColor(kNameColour);
    dc.DrawText(text(item.name), CRect(well.left, y, well.right, y + line),
                DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    dc.SelectObject(previous_font);
    y += line + 4;
    const CRect stock_line(well.left, button.top - 8 - line, well.right, button.top - 8);
    dc.SetTextColor(kWordsColour);
    dc.DrawText(text(item.description), CRect(well.left, y, well.right, stock_line.top - 6),
                DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    dc.SetTextColor(in_stock ? kStockColour : kSoldOutColour);
    dc.DrawText(stock_text(item.quantity), CRect(stock_line), DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    // Its button.
    UINT state = DFCS_BUTTONPUSH;
    if (!in_stock) {
        state |= DFCS_INACTIVE;
    } else if (hot) {
        state |= DFCS_HOT;
    }
    dc.DrawFrameControl(button, DFC_BUTTON, state);
    dc.SetTextColor(::GetSysColor(in_stock ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
    dc.DrawText(kPutOne, button, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void ShopPage::on_mouse(CPoint point, bool clicked) {
    int over = -1;
    if (point.x >= 0) {
        const CPoint at = point + shelf_.scroll_position();
        for (std::size_t i = 0; i < buttons_.size(); ++i) {
            if (buttons_[i].PtInRect(at)) {
                over = static_cast<int>(i);
                break;
            }
        }
    }
    if (over != hover_) {
        hover_ = over;
        shelf_.redraw();
    }
    if (clicked && over >= 0) {
        put_one_in_world(static_cast<std::size_t>(over));
    }
}

// SubmitSelectedHealthValue (Health Kit @ 0x00405c80): run the item's CAOS,
// which makes the object and puts it in the pointer's hand, then take one
// off and save the stock.
void ShopPage::put_one_in_world(std::size_t index) {
    std::vector<c1kit::ShopItem>& items = host_.shop_items();
    if (index >= items.size()) {
        return;
    }
    c1kit::ShopItem& item = items[index];
    if (item.quantity <= 0) {
        return;
    }
    if (!host_.run_shop_command(item.command)) {
        status_.SetWindowText(_T("The game did not take it."));
        return;
    }
    --item.quantity;
    host_.save_shop();
    shelf_.redraw();
    status_.SetWindowText(text(item.name) + _T(" is on the pointer: click in the world to drop it."));
}

// ===========================================================================
// ClassicShopPage
// ===========================================================================

namespace {

// CAddObjectPage's controls (dialog 142).
constexpr UINT kShopFrame = 1071;
constexpr UINT kShopPrevious = 1115;
constexpr UINT kShopNext = 1120;
constexpr UINT kShopEarth = 1121;
constexpr UINT kShopNotes = 1119;
constexpr UINT kShopTitle = 1116;
constexpr UINT kShopCount = 1118;
constexpr UINT kShopClose = 1123;
constexpr UINT kShopBoard = 0x7f50;  // the drawn board, over the frame
constexpr int kBackdropX = 0x68;     // Addbgd.bmp, on the Breeder's board
constexpr int kBackdropY = 0x73;

} // namespace

BEGIN_MESSAGE_MAP(ClassicShopPage, CPropertyPage)
    ON_BN_CLICKED(kShopPrevious, &ClassicShopPage::OnPrevious)
    ON_BN_CLICKED(kShopNext, &ClassicShopPage::OnNext)
    ON_BN_CLICKED(kShopEarth, &ClassicShopPage::OnEarth)
    ON_BN_CLICKED(kShopClose, &ClassicShopPage::OnCloseKit)
    ON_WM_DRAWITEM()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

ClassicShopPage::ClassicShopPage(KitSheet& sheet, ShopHost& host, const ClassicArt& art,
                                 UINT dialog, UINT title_string, const char* board,
                                 bool item_backdrop)
    : CPropertyPage(dialog), kit_sheet_(sheet), host_(host), art_(art), board_(board),
      item_backdrop_(item_backdrop) {
    title_ = load_string(title_string);
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

BOOL ClassicShopPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    // The board takes the frame's place, under the buttons.
    if (CWnd* frame = GetDlgItem(kShopFrame)) {
        CRect area;
        frame->GetWindowRect(&area);
        ScreenToClient(&area);
        frame->ShowWindow(SW_HIDE);
        board_view_.create(*this, kShopBoard, [this](CDC& dc, const CRect& rect) { draw_board(dc, rect); });
        board_view_.ModifyStyle(0, WS_CLIPSIBLINGS);  // it must not paint over the controls on it
        board_view_.SetWindowPos(&wndBottom, area.left, area.top, area.Width(), area.Height(),
                                 SWP_NOACTIVATE);
    }
    LOGFONT description = {};
    GetFont()->GetLogFont(&description);
    description.lfWeight = FW_BOLD;
    notes_font_.CreateFontIndirect(&description);
    description.lfHeight = description.lfHeight * 5 / 4;
    lstrcpy(description.lfFaceName, _T("Arial"));  // the dialog font does not embolden larger
    title_font_.CreateFontIndirect(&description);
    for (UINT id : {kShopTitle, kShopCount}) {
        if (CWnd* box = GetDlgItem(id)) box->SetFont(&title_font_);
    }
    if (CWnd* box = GetDlgItem(kShopNotes)) box->SetFont(&notes_font_);
    white_.CreateSolidBrush(RGB(255, 255, 255));
    show();
    return TRUE;
}

void ClassicShopPage::refresh() {
    if (GetSafeHwnd() != nullptr) show();
}

void ClassicShopPage::show() {
    std::vector<c1kit::ShopItem>& items = host_.shop_items();
    if (selected_ >= static_cast<int>(items.size())) selected_ = 0;
    const bool any = !items.empty();
    const c1kit::ShopItem* item = any ? &items[static_cast<std::size_t>(selected_)] : nullptr;
    CString count;
    if (item != nullptr) count.Format(_T("%d"), item->quantity);
    SetDlgItemText(kShopTitle, item != nullptr ? CString(item->name.c_str()) : CString());
    SetDlgItemText(kShopCount, count);
    SetDlgItemText(kShopNotes, item != nullptr ? CString(item->description.c_str()) : CString());
    if (CWnd* earth = GetDlgItem(kShopEarth)) earth->EnableWindow(item != nullptr && item->quantity > 0);
    board_view_.redraw();
}

void ClassicShopPage::draw_board(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    const std::string directory = std::string(CStringA(game_directory_setting("Main Directory")));
    PaletteBitmap board;
    if (!board.load_file(CString((directory + board_).c_str()))) return;
    if (!canvas_.create(board.width(), board.height())) return;
    canvas_.draw_bitmap_file(directory + board_, 0, 0);
    if (item_backdrop_) canvas_.draw_bitmap_file(directory + "Addbgd.bmp", kBackdropX, kBackdropY);
    const std::vector<c1kit::ShopItem>& items = host_.shop_items();
    if (selected_ < static_cast<int>(items.size())) {
        const c1kit::PhotoBitmap& picture = items[static_cast<std::size_t>(selected_)].picture;
        if (picture.width > 0 && picture.height > 0) {
            canvas_.draw_indexed_keyed(picture.pixels.data(), picture.width, picture.height,
                                       picture.stride, true, (board.width() - picture.width) / 2,
                                       (board.height() - picture.height) / 2, host_.shop_palette());
        }
    }
    // From the frame's top left, as CDIBStatic drew it (the Breeder's board
    // is taller than its frame, and shows its top).
    canvas_.present(dc, rect.left, rect.top, (std::min)(board.width(), rect.Width()),
                    (std::min)(board.height(), rect.Height()));
}

void ClassicShopPage::OnPrevious() {
    const int count = static_cast<int>(host_.shop_items().size());
    if (count > 0) selected_ = (selected_ + count - 1) % count;
    show();
}

void ClassicShopPage::OnNext() {
    const int count = static_cast<int>(host_.shop_items().size());
    if (count > 0) selected_ = (selected_ + 1) % count;
    show();
}

// SubmitSelectedHealthValue @ 0x00405c80: the item's CAOS, and one fewer.
void ClassicShopPage::OnEarth() {
    std::vector<c1kit::ShopItem>& items = host_.shop_items();
    if (selected_ >= static_cast<int>(items.size())) return;
    c1kit::ShopItem& item = items[static_cast<std::size_t>(selected_)];
    if (item.quantity <= 0) return;
    if (!host_.run_shop_command(item.command)) {
        AfxMessageBox(_T("The game did not take it."), MB_ICONINFORMATION);
        return;
    }
    --item.quantity;
    host_.save_shop();
    show();
}

void ClassicShopPage::OnCloseKit() {
    kit_sheet_.request_game_quit();
}

// CBitmapButton's faces: the caption with U (up), D (down), F (focused) or
// X (disabled), from the original.
HBITMAP ClassicShopPage::face(const CString& caption, TCHAR state) const {
    return static_cast<HBITMAP>(::LoadImage(art_.module(), caption + state, IMAGE_BITMAP, 0, 0,
                                            LR_CREATEDIBSECTION));
}

void ClassicShopPage::OnDrawItem(int id, LPDRAWITEMSTRUCT draw) {
    if (id != kShopPrevious && id != kShopNext && id != kShopEarth) {
        CPropertyPage::OnDrawItem(id, draw);
        return;
    }
    CString caption;
    GetDlgItemText(id, caption);
    TCHAR state = _T('U');
    if ((draw->itemState & ODS_SELECTED) != 0) {
        state = _T('D');
    } else if ((draw->itemState & ODS_DISABLED) != 0) {
        state = _T('X');
    } else if ((draw->itemState & ODS_FOCUS) != 0) {
        state = _T('F');
    }
    HBITMAP bitmap = face(caption, state);
    if (bitmap == nullptr && state != _T('U')) bitmap = face(caption, _T('U'));
    CDC* dc = CDC::FromHandle(draw->hDC);
    if (bitmap == nullptr) {
        dc->DrawFrameControl(&draw->rcItem, DFC_BUTTON, DFCS_BUTTONPUSH);
        return;
    }
    CDC source;
    source.CreateCompatibleDC(dc);
    HGDIOBJ previous = ::SelectObject(source.GetSafeHdc(), bitmap);
    BITMAP info = {};
    ::GetObject(bitmap, sizeof(info), &info);
    dc->BitBlt(draw->rcItem.left, draw->rcItem.top, info.bmWidth, info.bmHeight, &source, 0, 0,
               SRCCOPY);
    ::SelectObject(source.GetSafeHdc(), previous);
    ::DeleteObject(bitmap);
}

// The name, count and notes on white, as the original showed them.
HBRUSH ClassicShopPage::OnCtlColor(CDC* dc, CWnd* control, UINT type) {
    const UINT id = control != nullptr ? control->GetDlgCtrlID() : 0;
    if (type == CTLCOLOR_STATIC && (id == kShopTitle || id == kShopCount || id == kShopNotes)) {
        dc->SetBkColor(RGB(255, 255, 255));
        return static_cast<HBRUSH>(white_.GetSafeHandle());
    }
    return CPropertyPage::OnCtlColor(dc, control, type);
}

} // namespace c1kitshell
