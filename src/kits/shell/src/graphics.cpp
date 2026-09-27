// Palette bitmaps and the cover page.

#include "c1kitshell/kit_shell.hpp"

#include <vector>

namespace c1kitshell {

bool PaletteBitmap::load(UINT resource_id, HMODULE module) {
    HBITMAP handle = static_cast<HBITMAP>(
        LoadImage(module != nullptr ? module : AfxGetResourceHandle(),
                  MAKEINTRESOURCE(resource_id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
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

// ---------------------------------------------------------------------------
// ClassicArt
// ---------------------------------------------------------------------------

std::unique_ptr<ClassicArt> ClassicArt::find(std::initializer_list<UINT> bitmaps,
                                             std::initializer_list<const TCHAR*> named_bitmaps,
                                             std::initializer_list<UINT> icons) {
    TCHAR own[MAX_PATH] = {};
    if (GetModuleFileName(nullptr, own, MAX_PATH) == 0) {
        return nullptr;
    }
    CString path(own);
    CString stem = path;
    if (stem.GetLength() > 4 && stem.Right(4).CompareNoCase(_T(".exe")) == 0) {
        stem = stem.Left(stem.GetLength() - 4);
    }
    for (const CString& candidate : {stem + _T(".old"), path + _T(".old")}) {
        if (GetFileAttributes(candidate) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        HMODULE module = LoadLibraryEx(candidate, nullptr,
                                       LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (module == nullptr) {
            continue;
        }
        // A loose check that it is the kit it claims to be: the art the
        // classic pages use is all there.
        bool complete = true;
        for (const UINT id : bitmaps) {
            complete = complete && FindResource(module, MAKEINTRESOURCE(id), RT_BITMAP) != nullptr;
        }
        for (const TCHAR* name : named_bitmaps) {
            complete = complete && FindResource(module, name, RT_BITMAP) != nullptr;
        }
        for (const UINT id : icons) {
            complete = complete && FindResource(module, MAKEINTRESOURCE(id), RT_GROUP_ICON) != nullptr;
        }
        if (complete) {
            return std::unique_ptr<ClassicArt>(new ClassicArt(module, candidate));
        }
        FreeLibrary(module);
    }
    return nullptr;
}

ClassicArt::~ClassicArt() {
    if (module_ != nullptr) {
        FreeLibrary(module_);
    }
}

// ---------------------------------------------------------------------------
// CoverPage
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CoverPage, CPropertyPage)
    ON_WM_PAINT()
END_MESSAGE_MAP()

CoverPage::CoverPage(UINT dialog_id, UINT bitmap_id, UINT tab_icon_id, const ClassicArt& art)
    : CPropertyPage(dialog_id), bitmap_id_(bitmap_id), art_(art) {
    m_psp.dwFlags |= PSP_USEHICON;
    m_psp.hIcon = AfxGetApp()->LoadIcon(tab_icon_id);
}

BOOL CoverPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    bitmap_.load(bitmap_id_, art_.module());
    bitmap_.realize(*this);
    return TRUE;
}

void CoverPage::OnPaint() {
    CPaintDC dc(this);
    bitmap_.draw(dc, 7, 7);
}

// ---------------------------------------------------------------------------
// Button faces and the kit icon
// ---------------------------------------------------------------------------

namespace {

// The faces the 1996 kits' bitmap buttons had, by caption.
CSize button_face_size(const CString& name) {
    if (name == _T("NEXTA") || name == _T("PREVA")) return CSize(45, 21);
    if (name == _T("NEXT") || name == _T("PREV")) return CSize(37, 21);
    return CSize(75, 21);
}

void draw_glyph(CDC& dc, const CString& name, CPoint c) {
    const COLORREF ink = GetSysColor(COLOR_BTNTEXT);
    CPen pen(PS_SOLID, 1, ink);
    CBrush brush(ink);
    CPen* old_pen = dc.SelectObject(&pen);
    CBrush* old_brush = dc.SelectObject(&brush);
    if (name.Left(4) == _T("NEXT") || name.Left(4) == _T("PREV")) {
        const int dir = name.Left(4) == _T("NEXT") ? 1 : -1;
        POINT tri[3] = {{c.x - 3 * dir, c.y - 5}, {c.x - 3 * dir, c.y + 5}, {c.x + 3 * dir, c.y}};
        dc.Polygon(tri, 3);
    } else if (name == _T("CAMERA")) {
        dc.Rectangle(c.x - 8, c.y - 4, c.x + 8, c.y + 6);
        dc.Rectangle(c.x - 3, c.y - 6, c.x + 3, c.y - 3);
        CBrush face(GetSysColor(COLOR_BTNFACE));
        dc.SelectObject(&face);
        dc.Ellipse(c.x - 3, c.y - 2, c.x + 4, c.y + 5);
        dc.SelectObject(&brush);
    } else if (name == _T("DELETE")) {
        dc.Rectangle(c.x - 7, c.y - 5, c.x + 8, c.y - 3);  // lid
        dc.Rectangle(c.x - 2, c.y - 7, c.x + 3, c.y - 4);  // handle
        dc.Rectangle(c.x - 5, c.y - 3, c.x + 6, c.y + 7);  // bin
        CPen gap(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));
        dc.SelectObject(&gap);
        for (int x = c.x - 2; x <= c.x + 3; x += 3) {
            dc.MoveTo(x, c.y - 1);
            dc.LineTo(x, c.y + 5);
        }
        dc.SelectObject(&pen);
    } else if (name == _T("SAVEAS")) {
        dc.Rectangle(c.x - 7, c.y - 7, c.x + 8, c.y + 8);  // disk
        CBrush face(GetSysColor(COLOR_BTNFACE));
        dc.SelectObject(&face);
        dc.Rectangle(c.x - 4, c.y - 7, c.x + 5, c.y - 2);  // shutter
        dc.Rectangle(c.x - 5, c.y + 1, c.x + 6, c.y + 8);  // label
        dc.SelectObject(&brush);
    }
    dc.SelectObject(old_pen);
    dc.SelectObject(old_brush);
}

// One face: raised, or sunk and shifted when pressed, with a focus rect.
void draw_face(CBitmap& bitmap, CDC& screen, CSize size, const CString& name, bool pressed,
               bool focused) {
    bitmap.CreateCompatibleBitmap(&screen, size.cx, size.cy);
    CDC dc;
    dc.CreateCompatibleDC(&screen);
    CBitmap* old = dc.SelectObject(&bitmap);
    CRect r(0, 0, size.cx, size.cy);
    dc.FillSolidRect(r, GetSysColor(COLOR_BTNFACE));
    dc.DrawEdge(r, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
    const int shift = pressed ? 1 : 0;
    draw_glyph(dc, name, CPoint(size.cx / 2 + shift, size.cy / 2 + shift));
    if (focused) {
        CRect f = r;
        f.DeflateRect(3, 3);
        dc.DrawFocusRect(f);
    }
    dc.SelectObject(old);
}

} // namespace

bool FaceButton::load_faces(UINT control, CWnd& parent, const ClassicArt* art) {
    if (!SubclassDlgItem(control, &parent)) {
        return false;
    }
    CString name;
    GetWindowText(name);
    name.MakeUpper();
    if (art != nullptr) {
        const auto face = [&](const TCHAR* suffix) {
            return static_cast<HBITMAP>(LoadImage(art->module(), name + suffix, IMAGE_BITMAP, 0, 0, 0));
        };
        HBITMAP up = face(_T("U"));
        if (up != nullptr) {
            m_bitmap.Attach(up);
            if (HBITMAP down = face(_T("D"))) m_bitmapSel.Attach(down);
            if (HBITMAP focus = face(_T("F"))) m_bitmapFocus.Attach(focus);
            SizeToContent();
            return true;
        }
    }
    CClientDC screen(&parent);
    const CSize size = button_face_size(name);
    draw_face(m_bitmap, screen, size, name, false, false);
    draw_face(m_bitmapSel, screen, size, name, true, false);
    draw_face(m_bitmapFocus, screen, size, name, false, true);
    SizeToContent();
    return true;
}

HICON kit_icon(UINT icon_id, const ClassicArt* art) {
    if (art != nullptr) {
        if (HICON icon = static_cast<HICON>(
                LoadImage(art->module(), MAKEINTRESOURCE(icon_id), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE))) {
            return icon;
        }
    }
    return AfxGetApp()->LoadIcon(icon_id);
}

} // namespace c1kitshell
