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

} // namespace c1kitshell
