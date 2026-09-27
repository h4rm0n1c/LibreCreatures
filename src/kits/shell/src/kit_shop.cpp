// The shared shop page.  See kit_shop.hpp.

#include "c1kitshell/kit_shop.hpp"

#include <algorithm>

namespace c1kitshell {
namespace {

enum : UINT {
    kControlShopList = 3900,
    kControlShopPicture,
    kControlShopAdd,
    kControlShopStatus,
};

CString text(const std::string& value) {
    return CString(value.c_str());
}

} // namespace

BEGIN_MESSAGE_MAP(ShopPage, LayoutPage)
    ON_LBN_SELCHANGE(kControlShopList, &ShopPage::OnSelectionChanged)
    ON_BN_CLICKED(kControlShopAdd, &ShopPage::OnAddToWorld)
END_MESSAGE_MAP()

ShopPage::ShopPage(KitSheet& sheet, ShopHost& host, UINT blank_dialog, UINT title_string)
    : LayoutPage(sheet, blank_dialog, title_string), host_(host) {}

void ShopPage::create_controls() {
    make(items_, _T("LISTBOX"), _T(""), LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
         kControlShopList);
    picture_.create(*this, kControlShopPicture,
                    [this](CDC& dc, const CRect& rect) { draw_picture(dc, rect); });
    make(add_, _T("BUTTON"), _T("Put one in the world"), BS_PUSHBUTTON | WS_TABSTOP,
         kControlShopAdd);
    make(status_, _T("STATIC"), _T(""), SS_LEFT, kControlShopStatus);
    fill_list();
    items_.SetCurSel(host_.shop_items().empty() ? -1 : 0);
    show_selected();
}

void ShopPage::layout(int width, int height) {
    const int list_width = (std::min)(200, width / 3);
    const int bottom = height - 2 * kMargin - button_height();
    place(items_, kMargin, kMargin, list_width, bottom - kMargin);
    const int left = 2 * kMargin + list_width;
    place(picture_, left, kMargin, width - left - kMargin,
          bottom - 3 * kMargin - 2 * button_height());
    place(add_, left, bottom - 2 * button_height() - kMargin, 160, button_height());
    place(status_, left, bottom - button_height(), width - left - kMargin, text_height());
    place_close(width, height);
}

void ShopPage::refresh() {
    if (created_) {
        fill_list();
        show_selected();
    }
}

void ShopPage::fill_list() {
    const int selected = items_.GetCurSel();
    items_.ResetContent();
    for (const c1kit::ShopItem& item : host_.shop_items()) {
        CString line;
        line.Format(_T("%s  (%d left)"), text(item.name).GetString(), item.quantity);
        items_.AddString(line);
    }
    if (selected >= 0 && selected < items_.GetCount()) {
        items_.SetCurSel(selected);
    }
}

void ShopPage::OnSelectionChanged() {
    status_.SetWindowText(_T(""));
    show_selected();
}

void ShopPage::show_selected() {
    const int index = items_.GetCurSel();
    const std::vector<c1kit::ShopItem>& items = host_.shop_items();
    const bool valid = index >= 0 && index < static_cast<int>(items.size());
    add_.EnableWindow(valid && items[static_cast<std::size_t>(index)].quantity > 0);
    picture_.redraw();
}

void ShopPage::draw_picture(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(24, 24, 28));
    dc.SetBkMode(TRANSPARENT);
    const std::vector<c1kit::ShopItem>& items = host_.shop_items();
    const int index = items_.GetSafeHwnd() != nullptr ? items_.GetCurSel() : -1;
    if (index < 0 || index >= static_cast<int>(items.size())) {
        dc.SetTextColor(RGB(200, 200, 200));
        dc.TextOut(rect.left + 12, rect.top + 12,
                   items.empty() ? CString(_T("The shop file (")) + CString(host_.shop_file_name()) +
                                       _T(") could not be read.")
                                 : CString(_T("Pick something from the list.")));
        return;
    }
    const c1kit::ShopItem& item = items[static_cast<std::size_t>(index)];
    // The picture, scaled up to fit, above its name and description.
    const int text_space = 60;
    const c1kit::PhotoBitmap& picture = item.picture;
    if (picture.width > 0 && picture.height > 0 && canvas_.create(picture.width, picture.height)) {
        canvas_.fill(0);
        canvas_.draw_indexed(picture.pixels.data(), picture.width, picture.height, picture.stride,
                             true, 0, 0, host_.shop_palette());
        const int scale = (std::max)(1, (std::min)((rect.Width() - 20) / picture.width,
                                                   (rect.Height() - text_space - 20) / picture.height));
        const int w = picture.width * scale;
        const int h = picture.height * scale;
        CDC source;
        source.CreateCompatibleDC(&dc);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(&dc, picture.width, picture.height);
        CBitmap* previous = source.SelectObject(&bitmap);
        canvas_.present(source, 0, 0, picture.width, picture.height);
        dc.SetStretchBltMode(COLORONCOLOR);
        dc.StretchBlt(rect.left + (rect.Width() - w) / 2, rect.top + 10, w, h, &source, 0, 0,
                      picture.width, picture.height, SRCCOPY);
        source.SelectObject(previous);
    }
    const CRect words(rect.left + 10, rect.bottom - text_space, rect.right - 10, rect.bottom - 4);
    dc.SetTextColor(RGB(255, 255, 255));
    CString count;
    count.Format(_T("   (%d left)"), item.quantity);
    dc.DrawText(text(item.name) + count, CRect(words.left, words.top, words.right, words.top + 20),
                DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    dc.SetTextColor(RGB(200, 200, 200));
    dc.DrawText(text(item.description),
                CRect(words.left, words.top + 22, words.right, words.bottom),
                DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
}

// SubmitSelectedHealthValue (Health Kit @ 0x00405c80): run the item's CAOS,
// which makes the object and puts it in the pointer's hand, then take one
// off and save the stock.
void ShopPage::OnAddToWorld() {
    const int index = items_.GetCurSel();
    std::vector<c1kit::ShopItem>& items = host_.shop_items();
    if (index < 0 || index >= static_cast<int>(items.size())) {
        return;
    }
    c1kit::ShopItem& item = items[static_cast<std::size_t>(index)];
    if (item.quantity <= 0) {
        return;
    }
    if (!host_.run_shop_command(item.command)) {
        status_.SetWindowText(_T("The game did not take it."));
        return;
    }
    --item.quantity;
    host_.save_shop();
    fill_list();
    show_selected();
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
