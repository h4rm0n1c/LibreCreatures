// Injections: give the creature a medicine.
//
// The 1996 page (CInjectPage, dialog 144) had a syringe animation, a
// slider and a list of the medicines in injections.str (chemicals 100
// onwards), and Go sent "chem <chemical> <slider / 8>".  Here the dose is
// the amount the game adds, 1 to 255, and the page shows how much of the
// medicine the creature has now.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>

namespace science {

BEGIN_MESSAGE_MAP(InjectionsPage, SciencePage)
    ON_BN_CLICKED(kControlInject, &InjectionsPage::OnInject)
    ON_LBN_SELCHANGE(kControlMedicineList, &InjectionsPage::OnMedicineChanged)
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_TIMER()
END_MESSAGE_MAP()

InjectionsPage::InjectionsPage(ScienceSheet& sheet) : SciencePage(sheet, kStringInjectionsTab) {}

void InjectionsPage::create_controls() {
    make(medicine_label_, _T("STATIC"), _T("Medicine:"), SS_LEFT, kControlHint);
    make(medicines_, _T("LISTBOX"), _T(""), LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
         kControlMedicineList);
    for (const std::string& name : sheet_.medicines()) {
        medicines_.AddString(CString(name.c_str()));
    }
    const bool classic = sheet_.classic();
    make(dose_, TRACKBAR_CLASS, _T(""),
         (classic ? TBS_VERT | TBS_BOTH | TBS_AUTOTICKS : TBS_HORZ | TBS_AUTOTICKS) | WS_TABSTOP, kControlDose);
    dose_.SetRange(1, 255);
    dose_.SetTicFreq(32);
    dose_.SetPageSize(16);
    make(dose_label_, _T("STATIC"), _T(""), SS_LEFT, kControlDoseLabel);
    make(inject_, _T("BUTTON"), classic ? _T("Go") : _T("Inject"), BS_DEFPUSHBUTTON | WS_TABSTOP,
         kControlInject);
    if (classic) {
        syringe_view_.create(*this, kControlSyringe, [this](CDC& dc, const CRect& rect) { syringe_.draw(dc, rect); });
        const CString palettes = c1kitshell::game_directory_setting("Palette Directory");
        palette_.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\")) : palettes)) +
                      "palette.dta");
        syringe_.load(std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))),
                      kMedicineLiquids[0], palette_);
        liquid_ = 0;
    }
    make(level_, _T("STATIC"), _T(""), SS_LEFT, kControlMedicineLevel);
    std::uint32_t selected = 0;
    std::uint32_t dose = 32;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Chemical", selected);
        settings->read_dword(c1kit::SettingsScope::user, "Dose", dose);
    }
    medicines_.SetCurSel(selected < sheet_.medicines().size() ? static_cast<int>(selected) : 0);
    set_slider_dose(dose >= 1 && dose <= 255 ? static_cast<int>(dose) : 32);
    update_dose_label();
    change_liquid();
}

// The classic look: dialog 144.  The syringe in the picture's place, the
// slider upright beside it, Go, and the medicines where its box was (a
// short list here); the medicine's level along the bottom.
void InjectionsPage::layout_classic() {
    place_dlu(syringe_view_, 12, 12, 106, 152);
    place_dlu(dose_, 147, 9, 26, 97);
    place_dlu(dose_label_, 178, 60, 60, 8);
    place_dlu(inject_, 202, 90, 35, 12);
    place_dlu(medicines_, 148, 117, 90, 50);
    place_dlu(level_, 12, 167, 178, 20);
    place_dlu(close_, 194, 170, 50, 14);
    medicine_label_.ShowWindow(SW_HIDE);
}

int InjectionsPage::slider_dose() const {
    return sheet_.classic() ? 256 - dose_.GetPos() : dose_.GetPos();
}

void InjectionsPage::set_slider_dose(int dose) {
    dose_.SetPos(sheet_.classic() ? 256 - dose : dose);
    if (syringe_.loaded()) {
        syringe_.set_dose(dose);
        syringe_view_.redraw();
    }
}

void InjectionsPage::change_liquid() {
    if (!syringe_.loaded()) return;
    const int index = medicines_.GetCurSel();
    const int count = static_cast<int>(sizeof(kMedicineLiquids) / sizeof(kMedicineLiquids[0]));
    const int liquid = index < 0 ? 0 : index < count ? index : count;
    if (liquid != liquid_ &&
        syringe_.set_liquid(liquid < count ? kMedicineLiquids[liquid] : kOtherMedicineLiquid)) {
        liquid_ = liquid;
        syringe_view_.redraw();
    }
}

void InjectionsPage::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerSyringe) {
        const bool more = syringe_.tick();
        dose_.SetPos(256 - (more ? (std::max)(1, syringe_.shown_dose()) : syringe_.dose()));
        syringe_view_.redraw();
        if (!more) KillTimer(kTimerSyringe);
        return;
    }
    SciencePage::OnTimer(timer_id);
}

void InjectionsPage::OnVScroll(UINT code, UINT position, CScrollBar* bar) {
    SciencePage::OnVScroll(code, position, bar);
    OnHScroll(code, position, bar);  // the classic slider stands upright
}

void InjectionsPage::layout(int width, int height) {
    if (sheet_.classic()) {
        layout_classic();
        return;
    }
    const int margin = 7;
    const int row = text_height() + 8;
    const int column = (std::min)(260, width / 2 - margin);
    place(medicine_label_, margin, margin, column, text_height());
    place(medicines_, margin, margin + text_height() + 2, column,
          height - 4 * margin - row - text_height());
    const int left = margin * 2 + column;
    const int right_width = width - left - margin;
    place(dose_label_, left, margin, right_width, text_height());
    place(dose_, left, margin + text_height() + 2, right_width, 32);
    place(inject_, left, margin + text_height() + 40, 90, row);
    place(level_, left, margin + text_height() + 48 + row, right_width, 3 * text_height());
    place(close_, width - margin - 84, height - margin - row, 84, row);
}

int InjectionsPage::selected_chemical() const {
    const int index = medicines_.GetCurSel();
    return index < 0 ? -1 : c1kit::kFirstMedicineChemical + index;
}

void InjectionsPage::update_dose_label() {
    CString text;
    text.Format(_T("Dose: %d"), slider_dose());
    dose_label_.SetWindowText(text);
}

void InjectionsPage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    SciencePage::OnHScroll(code, position, bar);
    if (syringe_.injecting()) return;
    update_dose_label();
    if (syringe_.loaded()) {
        syringe_.set_dose(slider_dose());
        syringe_view_.redraw();
    }
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Dose", static_cast<std::uint32_t>(slider_dose()));
    }
}

void InjectionsPage::OnMedicineChanged() {
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Chemical", static_cast<std::uint32_t>(medicines_.GetCurSel()));
    }
    change_liquid();
    poll();
}

void InjectionsPage::OnInject() {
    const int chemical = selected_chemical();
    if (chemical < 0 || !sheet_.subject().present) {
        return;
    }
    sheet_.run(c1kit::injection_script(chemical, slider_dose()));
    if (syringe_.loaded() && !syringe_.injecting()) {
        syringe_.start_injection();
        SetTimer(kTimerSyringe, c1kitshell::Syringe::kTickMs, nullptr);
    }
    poll();
}

void InjectionsPage::poll() {
    const int chemical = selected_chemical();
    CString text;
    std::string reply;
    std::vector<int> values;
    if (!sheet_.subject().present) {
        text = _T("Select a creature in the game.");
    } else if (chemical >= 0 &&
               sheet_.query(c1kit::chemical_levels_query({chemical}), reply) &&
               c1kit::parse_values(reply, 1, values)) {
        CString name;
        medicines_.GetText(medicines_.GetCurSel(), name);
        text.Format(_T("%s in the creature now: %d of 255"), name.GetString(), values[0]);
    }
    CString shown;
    level_.GetWindowText(shown);
    if (shown != text) {
        level_.SetWindowText(text);
    }
}

} // namespace science
