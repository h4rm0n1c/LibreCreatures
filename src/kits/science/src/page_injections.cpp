// Injections: give the creature a medicine.
//
// The 1996 page (CInjectPage, dialog 144) had a syringe animation, a
// slider and a list of the medicines in injections.str (chemicals 100
// onwards), and Go sent "chem <chemical> <slider / 8>".  Here the dose is
// the amount the game adds, 1 to 255.  The syringe is the 1996 page's, from
// the game's DOSE.bmp and dosage sprites (the medicine's colour from the
// 1996 kit's table), and plays its injection; beside it, how much of the
// medicine the creature has now and would have after the dose.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>

namespace science {

BEGIN_MESSAGE_MAP(InjectionsPage, SciencePage)
    ON_BN_CLICKED(kControlInject, &InjectionsPage::OnInject)
    ON_LBN_SELCHANGE(kControlMedicineList, &InjectionsPage::OnMedicineChanged)
    ON_WM_HSCROLL()
    ON_WM_TIMER()
END_MESSAGE_MAP()

InjectionsPage::InjectionsPage(ScienceSheet& sheet) : SciencePage(sheet, kStringInjectionsTab) {}

void InjectionsPage::create_controls() {
    make(medicine_label_, _T("STATIC"), _T("Medicine:"), SS_LEFT, kControlHint);
    make(medicines_, _T("LISTBOX"), _T(""), LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
         kControlMedicineList);
    for (const std::string& name : sheet_.medicines()) {
        medicines_.AddString(CString(name.c_str()));
    }
    const CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    palette_.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\")) : palettes)) + "palette.dta");
    syringe_.load(std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))), kMedicineLiquids[0],
                  palette_);
    liquid_ = 0;
    syringe_view_.create(*this, kControlSyringeView, [this](CDC& dc, const CRect& rect) {
        CRect card = rect;
        card.DeflateRect(1, 1);
        CBrush fill(RGB(255, 255, 255));
        CPen edge(PS_SOLID, 1, RGB(208, 204, 196));
        CBrush* old_brush = dc.SelectObject(&fill);
        CPen* old_pen = dc.SelectObject(&edge);
        dc.RoundRect(card, CPoint(10, 10));
        dc.SelectObject(old_pen);
        dc.SelectObject(old_brush);
        if (syringe_.loaded()) {
            syringe_.present(dc, rect);
        } else {
            CRect note = card;
            note.DeflateRect(10, 10);
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(RGB(110, 104, 96));
            CFont* old_font = dc.SelectObject(GetFont());
            dc.DrawText(_T("The syringe needs DOSE.bmp and the dosage sprites in the game's folder."), &note,
                        DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
            dc.SelectObject(old_font);
        }
    });
    make(dose_, TRACKBAR_CLASS, _T(""), TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, kControlDose);
    dose_.SetRange(1, 255);
    dose_.SetTicFreq(32);
    dose_.SetPageSize(16);
    make(dose_label_, _T("STATIC"), _T(""), SS_LEFT, kControlDoseLabel);
    make(inject_, _T("BUTTON"), _T("Inject"), BS_DEFPUSHBUTTON | WS_TABSTOP, kControlInject);
    reading_.create(*this, kControlReading, [this](CDC& dc, const CRect& rect) { draw_reading(dc, rect); });
    std::uint32_t selected = 0;
    std::uint32_t dose = 32;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Chemical", selected);
        settings->read_dword(c1kit::SettingsScope::user, "Dose", dose);
    }
    medicines_.SetCurSel(selected < sheet_.medicines().size() ? static_cast<int>(selected) : 0);
    dose_.SetPos(dose >= 1 && dose <= 255 ? static_cast<int>(dose) : 32);
    change_liquid();
    syringe_.set_dose(dose_.GetPos());
    update_dose_label();
}

// The medicines on the left; the syringe beside them; the dose, Inject and
// the reading on the right; Close at the bottom right.
void InjectionsPage::layout(int width, int height) {
    const int margin = 7;
    const int row = text_height() + 8;
    const int bottom = height - margin - row - margin;
    const CSize picture_size = c1kitshell::Syringe::picture_size();
    const int picture_width = static_cast<int>(picture_size.cx);
    const int picture_height = static_cast<int>(picture_size.cy);
    const int list_width = (std::max)(150, (std::min)(240, width / 3));
    place(medicine_label_, margin, margin, list_width, text_height());
    place(medicines_, margin, margin + text_height() + 4, list_width,
          (std::max)(60, bottom - (margin + text_height() + 4)));
    const int syringe_left = 2 * margin + list_width;
    const int syringe_width = picture_width + 24;
    place(syringe_view_, syringe_left, margin, syringe_width,
          (std::max)(60, (std::min)(picture_height + 24, bottom - margin)));
    const int right = syringe_left + syringe_width + margin;
    const int right_width = (std::max)(170, width - right - margin);
    int y = margin;
    place(dose_label_, right, y, right_width, text_height());
    y += text_height() + 2;
    place(dose_, right, y, right_width, 30);
    y += 36;
    place(inject_, right, y, 90, row);
    y += row + margin;
    place(reading_, right, y, right_width, 2 * text_height() + 52);
    place(close_, width - margin - 84, height - margin - row, 84, row);
}

int InjectionsPage::selected_chemical() const {
    const int index = medicines_.GetCurSel();
    return index < 0 ? -1 : c1kit::kFirstMedicineChemical + index;
}

// The medicine's colour, by the 1996 kit's table (as the classic page).
void InjectionsPage::change_liquid() {
    const int index = medicines_.GetCurSel();
    const int count = static_cast<int>(sizeof(kMedicineLiquids) / sizeof(kMedicineLiquids[0]));
    const int liquid = index < 0 ? 0 : (std::min)(index, count);
    if (liquid != liquid_ &&
        syringe_.set_liquid(liquid < count ? kMedicineLiquids[liquid] : kOtherMedicineLiquid)) {
        liquid_ = liquid;
        syringe_view_.redraw();
    }
}

void InjectionsPage::update_dose_label() {
    CString text;
    text.Format(_T("Dose: %d"), dose_.GetPos());
    dose_label_.SetWindowText(text);
}

void InjectionsPage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    SciencePage::OnHScroll(code, position, bar);
    if (syringe_.injecting()) return;
    update_dose_label();
    syringe_.set_dose(dose_.GetPos());
    syringe_view_.redraw();
    reading_.redraw();
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Dose", static_cast<std::uint32_t>(dose_.GetPos()));
    }
}

void InjectionsPage::OnMedicineChanged() {
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Chemical", static_cast<std::uint32_t>(medicines_.GetCurSel()));
    }
    change_liquid();
    level_now_ = -1;
    poll();
    reading_.redraw();
}

void InjectionsPage::OnInject() {
    const int chemical = selected_chemical();
    if (chemical < 0 || !sheet_.subject().present) {
        return;
    }
    sheet_.run(c1kit::injection_script(chemical, dose_.GetPos()));
    // The syringe squeezes out a drop and drains, then fills to the dose
    // again (the 1996 page's OnDosagePreviewTimer).
    if (syringe_.loaded() && !syringe_.injecting()) {
        syringe_.start_injection();
        SetTimer(kTimerSyringe, c1kitshell::Syringe::kTickMs, nullptr);
    }
    poll();
}

// The syringe's animation; the slider follows the barrel as it drains.
void InjectionsPage::OnTimer(UINT_PTR timer_id) {
    if (timer_id != kTimerSyringe) {
        SciencePage::OnTimer(timer_id);
        return;
    }
    const bool more = syringe_.tick();
    dose_.SetPos(more ? (std::max)(1, syringe_.shown_dose()) : syringe_.dose());
    syringe_view_.redraw();
    if (!more) {
        KillTimer(kTimerSyringe);
        poll();
    }
}

void InjectionsPage::poll() {
    const int chemical = selected_chemical();
    int level = -1;
    std::string reply;
    std::vector<int> values;
    if (chemical >= 0 && sheet_.subject().present &&
        sheet_.query(c1kit::chemical_levels_query({chemical}), reply) &&
        c1kit::parse_values(reply, 1, values)) {
        level = values[0];
    }
    if (level != level_now_) {
        level_now_ = level;
        reading_.redraw();
    }
}

void InjectionsPage::draw_reading(CDC& dc, const CRect& rect) {
    CString name;
    const int index = medicines_.GetCurSel();
    if (index >= 0) {
        medicines_.GetText(index, name);
    } else {
        name = _T("No medicine chosen");
    }
    c1kitshell::draw_dose_reading(dc, rect, GetFont(), name, level_now_, dose_.GetPos(),
                                  syringe_.liquid_colour());
}

} // namespace science
