// Injections: give the creature a medicine.
//
// The 1996 page (CInjectPage, dialog 144) had a syringe animation, a
// slider and a list of the medicines in injections.str (chemicals 100
// onwards), and Go sent "chem <chemical> <slider / 8>".  Here the dose is
// the amount the game adds, 1 to 255, and the page shows how much of the
// medicine the creature has now.

#include "science.hpp"
#include "science_ids.hpp"

namespace science {

BEGIN_MESSAGE_MAP(InjectionsPage, SciencePage)
    ON_BN_CLICKED(kControlInject, &InjectionsPage::OnInject)
    ON_LBN_SELCHANGE(kControlMedicineList, &InjectionsPage::OnMedicineChanged)
    ON_WM_HSCROLL()
END_MESSAGE_MAP()

InjectionsPage::InjectionsPage(ScienceSheet& sheet) : SciencePage(sheet, kStringInjectionsTab) {}

void InjectionsPage::create_controls() {
    make(medicine_label_, _T("STATIC"), _T("Medicine:"), SS_LEFT, kControlHint);
    make(medicines_, _T("LISTBOX"), _T(""), LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
         kControlMedicineList);
    for (const std::string& name : sheet_.medicines()) {
        medicines_.AddString(CString(name.c_str()));
    }
    make(dose_, TRACKBAR_CLASS, _T(""), TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, kControlDose);
    dose_.SetRange(1, 255);
    dose_.SetTicFreq(32);
    dose_.SetPageSize(16);
    make(dose_label_, _T("STATIC"), _T(""), SS_LEFT, kControlDoseLabel);
    make(inject_, _T("BUTTON"), _T("Inject"), BS_DEFPUSHBUTTON | WS_TABSTOP, kControlInject);
    make(level_, _T("STATIC"), _T(""), SS_LEFT, kControlMedicineLevel);
    std::uint32_t selected = 0;
    std::uint32_t dose = 32;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Chemical", selected);
        settings->read_dword(c1kit::SettingsScope::user, "Dose", dose);
    }
    medicines_.SetCurSel(selected < sheet_.medicines().size() ? static_cast<int>(selected) : 0);
    dose_.SetPos(dose >= 1 && dose <= 255 ? static_cast<int>(dose) : 32);
    update_dose_label();
}

void InjectionsPage::layout(int width, int height) {
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
    text.Format(_T("Dose: %d"), dose_.GetPos());
    dose_label_.SetWindowText(text);
}

void InjectionsPage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    SciencePage::OnHScroll(code, position, bar);
    update_dose_label();
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Dose", static_cast<std::uint32_t>(dose_.GetPos()));
    }
}

void InjectionsPage::OnMedicineChanged() {
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Chemical", static_cast<std::uint32_t>(medicines_.GetCurSel()));
    }
    poll();
}

void InjectionsPage::OnInject() {
    const int chemical = selected_chemical();
    if (chemical < 0 || !sheet_.subject().present) {
        return;
    }
    sheet_.run(c1kit::injection_script(chemical, dose_.GetPos()));
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
