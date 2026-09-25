// The Owner's Kit's pages: register the birth, the photo album and the birth
// certificate.  The original's behaviour is in ../ORIGINAL.md; deviations are
// marked "Fix (bug N)".

#include "owner.hpp"
#include "owner_ids.hpp"

namespace owner {
namespace {

CString text(const std::string& value) {
    return CString(value.c_str());
}

std::string control_text(CWnd& page, unsigned id) {
    CString value;
    page.GetDlgItemText(id, value);
    return std::string(CStringA(value));
}

// The pages are only usable with a subject; without one they say why.
void enable_controls(CWnd& page, const unsigned* ids, std::size_t count,
                     bool enable) {
    for (std::size_t i = 0; i < count; ++i) {
        if (CWnd* control = page.GetDlgItem(ids[i])) {
            control->EnableWindow(enable ? TRUE : FALSE);
        }
    }
}

} // namespace

// ===========================================================================
// Register the birth (dialog 136)
// ===========================================================================

BEGIN_MESSAGE_MAP(RegisterPage, CPropertyPage)
    ON_BN_CLICKED(kControlRegisterBirth, &RegisterPage::OnRegisterBirth)
    ON_BN_CLICKED(kControlRegisterClose, &RegisterPage::OnCloseKit)
END_MESSAGE_MAP()

RegisterPage::RegisterPage(OwnerSheet& sheet)
    : CPropertyPage(kDialogRegister, kStringRegisterTab), sheet_(sheet) {}

BOOL RegisterPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    static_cast<CEdit*>(GetDlgItem(kControlCreatureName))->LimitText(kMaxCreatureName);
    initialized_ = true;
    show();
    return TRUE;
}

BOOL RegisterPage::OnSetActive() {
    show();
    return CPropertyPage::OnSetActive();
}

// The Norn Information box shows the game's record; the owner fields show
// what the register holds for this creature.
void RegisterPage::show() {
    if (!initialized_) {
        return;
    }
    const Subject& subject = sheet_.subject();
    static const unsigned kEditable[] = {
        kControlCreatureName, kControlOwnerName, kControlOwnerAddress,
        kControlOwnerPhone,   kControlOwnerEmail, kControlRegisterBirth};
    enable_controls(*this, kEditable, std::size(kEditable), subject.present);
    if (!subject.present) {
        SetDlgItemText(kControlCreatureName, _T(""));
        SetDlgItemText(kControlSex, _T(""));
        SetDlgItemText(kControlAge, _T(""));
        return;
    }
    SetDlgItemText(kControlCreatureName, text(subject.name));
    SetDlgItemText(kControlSex, c1kitshell::load_string(
                                    subject.sex == 1   ? kStringMale
                                    : subject.sex == 2 ? kStringFemale
                                                       : kStringUnknown));
    SetDlgItemText(kControlAge, text(subject.age));
    const c1kit::OwnerRecord* registered = sheet_.registered_record();
    const c1kit::OwnerRecord& source =
        registered != nullptr ? *registered : subject.history;
    SetDlgItemText(kControlOwnerName, text(source[c1kit::kOwnerName]));
    SetDlgItemText(kControlOwnerAddress, text(source[c1kit::kOwnerAddress]));
    SetDlgItemText(kControlOwnerPhone, text(source[c1kit::kOwnerPhone]));
    SetDlgItemText(kControlOwnerEmail, text(source[c1kit::kOwnerEmail]));
}

// The Register Birth button (TransferOwnerDataFields @ 0x00409ac0): the
// game's record for the creature with the name and owner details from the
// page.
void RegisterPage::OnRegisterBirth() {
    const Subject& subject = sheet_.subject();
    if (!subject.present) {
        return;
    }
    const std::string name = control_text(*this, kControlCreatureName);
    if (name.empty() || name.size() > static_cast<std::size_t>(kMaxCreatureName)) {
        AfxMessageBox(kStringNameTooLong);
        return;
    }
    c1kit::OwnerRecord record = subject.history;
    record[c1kit::kOwnerMoniker] = subject.moniker;
    record[c1kit::kOwnerCreatureName] = name;
    record[c1kit::kOwnerName] = control_text(*this, kControlOwnerName);
    record[c1kit::kOwnerAddress] = control_text(*this, kControlOwnerAddress);
    record[c1kit::kOwnerPhone] = control_text(*this, kControlOwnerPhone);
    record[c1kit::kOwnerEmail] = control_text(*this, kControlOwnerEmail);
    sheet_.register_birth(record);
}

void RegisterPage::OnCloseKit() {
    sheet_.request_game_quit();
}

// ===========================================================================
// Photo Album (dialog 137)
// ===========================================================================

BEGIN_MESSAGE_MAP(AlbumPage, CPropertyPage)
    ON_WM_DRAWITEM()
    ON_BN_CLICKED(kControlSaveAs, &AlbumPage::OnSaveAs)
    ON_BN_CLICKED(kControlDeletePhoto, &AlbumPage::OnDeletePhoto)
    ON_BN_CLICKED(kControlTakePhoto, &AlbumPage::OnTakePhoto)
    ON_BN_CLICKED(kControlPreviousPhoto, &AlbumPage::OnPreviousPhoto)
    ON_BN_CLICKED(kControlNextPhoto, &AlbumPage::OnNextPhoto)
    ON_EN_KILLFOCUS(kControlCaption, &AlbumPage::OnCaptionKillFocus)
    ON_BN_CLICKED(kControlAlbumClose, &AlbumPage::OnCloseKit)
END_MESSAGE_MAP()

AlbumPage::AlbumPage(OwnerSheet& sheet)
    : CPropertyPage(kDialogAlbum, kStringAlbumTab), sheet_(sheet) {}

// The buttons carry their caption's bitmaps (SAVEASU/D/F and so on), as
// CBitmapButton::AutoLoad finds them.
BOOL AlbumPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    // The picture is first in the template, so first in the Z-order: on top
    // of the buttons and fields drawn over it.  Put it underneath, and clip
    // it to them, so repainting it never covers them.
    if (CWnd* picture = GetDlgItem(kControlAlbumPicture)) {
        picture->ModifyStyle(0, WS_CLIPSIBLINGS);
        picture->SetWindowPos(&CWnd::wndBottom, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    save_as_.AutoLoad(kControlSaveAs, this);
    delete_.AutoLoad(kControlDeletePhoto, this);
    camera_.AutoLoad(kControlTakePhoto, this);
    previous_.AutoLoad(kControlPreviousPhoto, this);
    next_.AutoLoad(kControlNextPhoto, this);
    initialized_ = true;
    show();
    return TRUE;
}

BOOL AlbumPage::OnSetActive() {
    show();
    return CPropertyPage::OnSetActive();
}

BOOL AlbumPage::OnKillActive() {
    commit_caption();
    return CPropertyPage::OnKillActive();
}

void AlbumPage::show() {
    if (!initialized_) {
        return;
    }
    const std::vector<c1kit::Photo>& photos = sheet_.photos();
    const bool present = sheet_.subject().present;
    const int index = sheet_.selected_photo();
    const bool any = present && !photos.empty();
    const c1kit::Photo* photo = any ? &photos[static_cast<std::size_t>(index)] : nullptr;
    SetDlgItemText(kControlTaken, photo != nullptr ? text(photo->taken) : CString());
    SetDlgItemText(kControlCaption, photo != nullptr ? text(photo->caption) : CString());
    GetDlgItem(kControlCaption)->EnableWindow(photo != nullptr);
    GetDlgItem(kControlTakePhoto)->EnableWindow(present);
    GetDlgItem(kControlSaveAs)->EnableWindow(photo != nullptr);
    GetDlgItem(kControlDeletePhoto)->EnableWindow(photo != nullptr);
    GetDlgItem(kControlPreviousPhoto)->EnableWindow(any && index > 0);
    GetDlgItem(kControlNextPhoto)->EnableWindow(
        any && index + 1 < static_cast<int>(photos.size()));
    draw_picture();
}

// The page picture: photograph.bmp, with the selected photograph (or
// Blank.bmp when there is none) in its frame.
void AlbumPage::draw_picture() {
    CWnd* control = GetDlgItem(kControlAlbumPicture);
    if (control == nullptr) {
        return;
    }
    CRect rect;
    control->GetClientRect(&rect);
    if (!picture_.create(rect.Width(), rect.Height())) {
        return;
    }
    picture_.fill(GetSysColor(COLOR_BTNFACE) & 0xffffff);
    picture_.tile_bitmap_file(kAlbumBackdrop);
    const std::vector<c1kit::Photo>& photos = sheet_.photos();
    if (sheet_.subject().present && !photos.empty()) {
        const c1kit::PhotoBitmap& bitmap =
            photos[static_cast<std::size_t>(sheet_.selected_photo())].bitmap;
        picture_.draw_indexed(bitmap.pixels.data(), bitmap.width, bitmap.height,
                              bitmap.stride, true, kPhotoLeft, kPhotoTop,
                              sheet_.palette());
    } else {
        // Blank.bmp is 125 x 145, centred where a 120 x 140 photo goes.
        picture_.draw_bitmap_file(kBlankPhoto, kPhotoLeft - 3, kPhotoTop - 3);
    }
    control->Invalidate(FALSE);
}

void AlbumPage::OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw) {
    if (control_id != static_cast<int>(kControlAlbumPicture)) {
        CPropertyPage::OnDrawItem(control_id, draw);
        return;
    }
    CDC* dc = CDC::FromHandle(draw->hDC);
    const CRect rect(draw->rcItem);
    picture_.present(*dc, rect.left, rect.top, rect.Width(), rect.Height());
}

void AlbumPage::commit_caption() {
    if (!initialized_ || GetSafeHwnd() == nullptr) {
        return;
    }
    std::vector<c1kit::Photo>& photos = sheet_.photos();
    if (!sheet_.subject().present || photos.empty()) {
        return;
    }
    c1kit::Photo& photo = photos[static_cast<std::size_t>(sheet_.selected_photo())];
    const std::string caption = control_text(*this, kControlCaption);
    if (caption != photo.caption) {
        photo.caption = caption;
        sheet_.save_album();
    }
}

void AlbumPage::OnCaptionKillFocus() {
    commit_caption();
}

void AlbumPage::OnTakePhoto() {
    sheet_.take_photo();
}

void AlbumPage::OnPreviousPhoto() {
    commit_caption();
    sheet_.select_photo(sheet_.selected_photo() - 1);
    show();
}

void AlbumPage::OnNextPhoto() {
    commit_caption();
    sheet_.select_photo(sheet_.selected_photo() + 1);
    show();
}

void AlbumPage::OnDeletePhoto() {
    if (sheet_.photos().empty()) {
        return;
    }
    if (AfxMessageBox(c1kitshell::load_string(kStringDeletePhoto) + _T("?"),
                      MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    sheet_.delete_photo(sheet_.selected_photo());
}

// Save as: the photograph as a 256-colour bitmap with the game palette.
void AlbumPage::OnSaveAs() {
    const std::vector<c1kit::Photo>& photos = sheet_.photos();
    if (photos.empty()) {
        return;
    }
    commit_caption();
    CFileDialog dialog(FALSE, _T("bmp"), text(sheet_.subject().name) + _T(".bmp"),
                       OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
                       _T("Bitmaps (*.bmp)|*.bmp|All files (*.*)|*.*||"), this);
    if (dialog.DoModal() != IDOK) {
        return;
    }
    const c1kit::PhotoBitmap& bitmap =
        photos[static_cast<std::size_t>(sheet_.selected_photo())].bitmap;
    if (!c1kitshell::save_indexed_bmp(std::string(CStringA(dialog.GetPathName())),
                                      bitmap.pixels.data(), bitmap.width,
                                      bitmap.height, bitmap.stride, true,
                                      sheet_.palette())) {
        AfxMessageBox(CString(_T("Could not write ")) + dialog.GetPathName() + _T("."));
    }
}

void AlbumPage::OnCloseKit() {
    commit_caption();
    sheet_.request_game_quit();
}

// ===========================================================================
// Certificate (dialog 141)
// ===========================================================================

BEGIN_MESSAGE_MAP(CertificatePage, CPropertyPage)
    ON_WM_DRAWITEM()
    ON_BN_CLICKED(kControlCertificateClose, &CertificatePage::OnCloseKit)
END_MESSAGE_MAP()

CertificatePage::CertificatePage(OwnerSheet& sheet)
    : CPropertyPage(kDialogCertificate, kStringCertificateTab), sheet_(sheet) {}

BOOL CertificatePage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    font_.CreateFont(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                     VARIABLE_PITCH | FF_ROMAN, _T("Times New Roman"));
    initialized_ = true;
    show();
    return TRUE;
}

BOOL CertificatePage::OnSetActive() {
    show();
    return CPropertyPage::OnSetActive();
}

void CertificatePage::show() {
    if (!initialized_) {
        return;
    }
    CWnd* control = GetDlgItem(kControlCertificatePicture);
    CRect rect;
    control->GetClientRect(&rect);
    if (picture_.create(rect.Width(), rect.Height())) {
        picture_.fill(GetSysColor(COLOR_BTNFACE) & 0xffffff);
        picture_.tile_bitmap_file(kCertificateBackdrop);
    }
    control->Invalidate(FALSE);
}

// RenderOwnerSelectionDisplay @ 0x0040ca90: name, mother, father, and the
// birth date and time split from "21:16 Sep 15 2026".  Unregistered
// creatures show "Unregistered" (string 109).
void CertificatePage::OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw) {
    if (control_id != static_cast<int>(kControlCertificatePicture)) {
        CPropertyPage::OnDrawItem(control_id, draw);
        return;
    }
    CDC* dc = CDC::FromHandle(draw->hDC);
    const CRect rect(draw->rcItem);
    picture_.present(*dc, rect.left, rect.top, rect.Width(), rect.Height());
    CFont* previous_font = dc->SelectObject(&font_);
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(RGB(0, 0, 0));
    const auto line = [&](int top, const CString& value) {
        dc->TextOut(rect.left + kCertificateTextLeft, rect.top + top, value);
    };
    const c1kit::OwnerRecord* record = sheet_.registered_record();
    if (record == nullptr) {
        line(kCertificateNameTop, sheet_.subject().present
                                      ? c1kitshell::load_string(kStringUnregistered)
                                      : CString(_T("No creature selected")));
    } else {
        line(kCertificateNameTop, text((*record)[c1kit::kOwnerCreatureName]));
        line(kCertificateMotherTop,
             sheet_.name_for_moniker((*record)[c1kit::kOwnerMotherMoniker]));
        line(kCertificateFatherTop,
             sheet_.name_for_moniker((*record)[c1kit::kOwnerFatherMoniker]));
        const std::string& born = (*record)[c1kit::kOwnerBirthTime];
        const std::size_t space = born.find(' ');
        line(kCertificateDateTop,
             text(space == std::string::npos ? born : born.substr(space + 1)));
        line(kCertificateTimeTop, text(born.substr(0, space)));
    }
    dc->SelectObject(previous_font);
}

void CertificatePage::OnCloseKit() {
    sheet_.request_game_quit();
}

} // namespace owner
