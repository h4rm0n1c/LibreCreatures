// Palette bitmaps and the cover page.

#include "c1kitshell/kit_shell.hpp"

#include <vector>

namespace c1kitshell {

bool PaletteBitmap::load(UINT resource_id) {
    HBITMAP handle = static_cast<HBITMAP>(
        LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(resource_id),
                  IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    if (handle == nullptr) {
        return false;
    }
    bitmap_.Attach(handle);
    BITMAP info = {};
    bitmap_.GetBitmap(&info);
    width_ = info.bmWidth;
    height_ = info.bmHeight;

    // Build a logical palette from the DIB's colour table.
    RGBQUAD colours[256] = {};
    UINT count = 0;
    CDC memory;
    memory.CreateCompatibleDC(nullptr);
    CBitmap* previous = memory.SelectObject(&bitmap_);
    count = GetDIBColorTable(memory.GetSafeHdc(), 0, 256, colours);
    memory.SelectObject(previous);
    if (count == 0) {
        return true;  // true-colour image: nothing to realise
    }
    std::vector<BYTE> storage(sizeof(LOGPALETTE) +
                              sizeof(PALETTEENTRY) * (count - 1));
    auto* logical = reinterpret_cast<LOGPALETTE*>(storage.data());
    logical->palVersion = 0x300;
    logical->palNumEntries = static_cast<WORD>(count);
    for (UINT index = 0; index < count; ++index) {
        logical->palPalEntry[index].peRed = colours[index].rgbRed;
        logical->palPalEntry[index].peGreen = colours[index].rgbGreen;
        logical->palPalEntry[index].peBlue = colours[index].rgbBlue;
        logical->palPalEntry[index].peFlags = 0;
    }
    palette_.CreatePalette(logical);
    return true;
}

void PaletteBitmap::realize(CWnd& window) {
    if (palette_.GetSafeHandle() == nullptr) {
        return;
    }
    CClientDC dc(&window);
    CPalette* previous = dc.SelectPalette(&palette_, TRUE);
    dc.RealizePalette();
    dc.SelectPalette(previous, TRUE);
    dc.RealizePalette();
}

void PaletteBitmap::draw(CDC& dc, int x, int y) {
    if (bitmap_.GetSafeHandle() == nullptr) {
        return;
    }
    CPalette* previous_palette = nullptr;
    if (palette_.GetSafeHandle() != nullptr) {
        previous_palette = dc.SelectPalette(&palette_, TRUE);
        dc.RealizePalette();
    }
    CDC memory;
    memory.CreateCompatibleDC(&dc);
    CBitmap* previous = memory.SelectObject(&bitmap_);
    dc.BitBlt(x, y, width_, height_, &memory, 0, 0, SRCCOPY);
    memory.SelectObject(previous);
    if (previous_palette != nullptr) {
        dc.SelectPalette(previous_palette, TRUE);
        dc.RealizePalette();
    }
}

BEGIN_MESSAGE_MAP(CoverPage, CPropertyPage)
    ON_WM_PAINT()
    ON_WM_SIZE()
END_MESSAGE_MAP()

CoverPage::CoverPage(UINT dialog_id, UINT bitmap_id, UINT tab_icon_id)
    : CPropertyPage(dialog_id), bitmap_id_(bitmap_id) {
    m_psp.dwFlags |= PSP_USEHICON;
    m_psp.hIcon = AfxGetApp()->LoadIcon(tab_icon_id);
}

BOOL CoverPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    bitmap_.load(bitmap_id_);
    bitmap_.realize(*this);
    return TRUE;
}

// The 1996 kits blit at (7, 7).  In a resizable sheet the page can be
// larger than the picture, so it is centred once there is room.
void CoverPage::OnPaint() {
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    const int x = client.Width() > bitmap_.width() + 14
                      ? (client.Width() - bitmap_.width()) / 2
                      : 7;
    const int y = client.Height() > bitmap_.height() + 14
                      ? (client.Height() - bitmap_.height()) / 2
                      : 7;
    bitmap_.draw(dc, x, y);
}

void CoverPage::OnSize(UINT type, int cx, int cy) {
    CPropertyPage::OnSize(type, cx, cy);
    Invalidate();
}

// ---------------------------------------------------------------------------
// ControlAnchors
// ---------------------------------------------------------------------------

void ControlAnchors::capture(CWnd& parent) {
    CRect client;
    parent.GetClientRect(&client);
    reference_ = client.Size();
    items_.clear();
}

void ControlAnchors::add(CWnd& parent, UINT id, unsigned anchors) {
    CWnd* control = parent.GetDlgItem(id);
    if (control == nullptr) {
        return;
    }
    CRect rect;
    control->GetWindowRect(&rect);
    parent.ScreenToClient(&rect);
    items_.push_back({id, rect, anchors});
}

void ControlAnchors::apply(CWnd& parent) const {
    if (items_.empty() || parent.GetSafeHwnd() == nullptr) {
        return;
    }
    CRect client;
    parent.GetClientRect(&client);
    const int dx = client.Width() > reference_.cx
                       ? client.Width() - reference_.cx : 0;
    const int dy = client.Height() > reference_.cy
                       ? client.Height() - reference_.cy : 0;
    HDWP batch = ::BeginDeferWindowPos(static_cast<int>(items_.size()));
    for (const Item& item : items_) {
        CWnd* control = parent.GetDlgItem(item.id);
        if (control == nullptr || batch == nullptr) {
            continue;
        }
        CRect rect = item.rect;
        if (item.anchors & kMoveX) {
            rect.OffsetRect(dx, 0);
        }
        if (item.anchors & kMoveY) {
            rect.OffsetRect(0, dy);
        }
        if (item.anchors & kGrowX) {
            rect.right += dx;
        }
        if (item.anchors & kGrowY) {
            rect.bottom += dy;
        }
        batch = ::DeferWindowPos(batch, control->GetSafeHwnd(), nullptr,
                                 rect.left, rect.top, rect.Width(),
                                 rect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (batch != nullptr) {
        ::EndDeferWindowPos(batch);
    }
    parent.Invalidate();
}

} // namespace c1kitshell
