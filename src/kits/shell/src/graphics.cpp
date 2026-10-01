// Palette bitmaps and the cover page.

#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_art.hpp"

#include <vector>

namespace c1kitshell {

bool PaletteBitmap::load(UINT resource_id, HMODULE module) {
    return adopt(static_cast<HBITMAP>(
        LoadImage(module != nullptr ? module : AfxGetResourceHandle(),
                  MAKEINTRESOURCE(resource_id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
}

bool PaletteBitmap::load_file(const CString& path) {
    return adopt(static_cast<HBITMAP>(LoadImage(nullptr, path, IMAGE_BITMAP, 0, 0,
                                                LR_LOADFROMFILE | LR_CREATEDIBSECTION)));
}

bool PaletteBitmap::adopt(HBITMAP handle) {
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
    // The original kit, renamed from <name>.exe to <name>.old, beside this
    // kit: whatever this kit's own file is called.
    const char* original = kit_definition().original_file_name;
    TCHAR own[MAX_PATH] = {};
    if (original == nullptr || GetModuleFileName(nullptr, own, MAX_PATH) == 0) {
        note_classic_art_available(false);
        return nullptr;
    }
    CString candidate(own);
    candidate = candidate.Left(candidate.ReverseFind(_T('\\')) + 1) + CString(original) + _T(".old");
    HMODULE module = GetFileAttributes(candidate) == INVALID_FILE_ATTRIBUTES
                         ? nullptr
                         : LoadLibraryEx(candidate, nullptr,
                                         LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (module == nullptr) {
        note_classic_art_available(false);
        return nullptr;
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
    note_classic_art_available(complete);
    if (!complete || skin_preference() == KitSkin::libre) {
        // Not the kit, or there but the player chose the kit's own look.
        FreeLibrary(module);
        return nullptr;
    }
    return std::unique_ptr<ClassicArt>(new ClassicArt(module, candidate));
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

CoverPage::CoverPage(UINT dialog_id, const char* picture_file, UINT tab_icon_id)
    : CPropertyPage(dialog_id), picture_file_(picture_file) {
    use_tab_icon(tab_icon_id);
}

CoverPage::CoverPage(UINT dialog_id, UINT bitmap_id, UINT tab_icon_id, const ClassicArt& art)
    : CPropertyPage(dialog_id), bitmap_id_(bitmap_id), art_(&art) {
    use_tab_icon(tab_icon_id);
}

CoverPage::~CoverPage() {
    if (tab_icon_ != nullptr) {
        ::DestroyIcon(tab_icon_);
    }
}

void CoverPage::use_tab_icon(UINT tab_icon_id) {
    // The tab is the icon alone: an empty title replaces the dialog's
    // "Cover" caption.
    tab_icon_ = static_cast<HICON>(::LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(tab_icon_id),
                                               IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON),
                                               ::GetSystemMetrics(SM_CYSMICON), 0));
    m_psp.dwFlags |= PSP_USETITLE | PSP_USEHICON;
    m_psp.pszTitle = _T("");
    m_psp.hIcon = tab_icon_;
}

BOOL CoverPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    if (art_ != nullptr) {
        bitmap_.load(bitmap_id_, art_->module());
    } else {
        bitmap_.load_file(game_directory_setting("Main Directory") + picture_file_);
    }
    bitmap_.realize(*this);
    return TRUE;
}

void CoverPage::OnPaint() {
    CPaintDC dc(this);
    CWnd* frame = art_ == nullptr ? GetDlgItem(kCoverPictureFrame) : nullptr;
    if (frame == nullptr) {
        bitmap_.draw(dc, 7, 7);
        return;
    }
    CRect area;
    frame->GetWindowRect(&area);
    ScreenToClient(&area);
    bitmap_.draw(dc, area.left + (area.Width() - bitmap_.width()) / 2,
                 area.top + (area.Height() - bitmap_.height()) / 2);
}

} // namespace c1kitshell
