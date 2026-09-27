// The Science Kit's classic skin: the 1996 pages on their own templates
// (dialogs 133, 143, 134, 135 and 144), sharing only the sheet's data and
// queries with the kit's own pages.  See science.hpp.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>
#include <cctype>

namespace science {
namespace {

constexpr UINT kClassicClose = 1160;
constexpr UINT_PTR kPollTimer = 31;
constexpr UINT_PTR kAnimationTimer = 32;

std::string main_file(const char* name) {
    return std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))) + name;
}

void load_palette(c1kitshell::GamePalette& palette) {
    const CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    palette.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\")) : palettes)) +
                 "palette.dta");
}

}  // namespace

// ===========================================================================
// ClassicSciencePage
// ===========================================================================

BEGIN_MESSAGE_MAP(ClassicSciencePage, CPropertyPage)
    ON_WM_TIMER()
    ON_BN_CLICKED(kClassicClose, &ClassicSciencePage::OnCloseKit)
END_MESSAGE_MAP()

ClassicSciencePage::ClassicSciencePage(ScienceSheet& sheet, UINT dialog, UINT title_string, UINT poll_ms)
    : CPropertyPage(dialog), sheet_(sheet), poll_ms_(poll_ms) {
    title_ = c1kitshell::load_string(title_string);
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

BOOL ClassicSciencePage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    init();
    return TRUE;
}

// The 1996 pages worked only while they showed.
BOOL ClassicSciencePage::OnSetActive() {
    poll();
    if (poll_ms_ != 0) SetTimer(kPollTimer, poll_ms_, nullptr);
    if (animation_ms_ != 0) SetTimer(kAnimationTimer, animation_ms_, nullptr);
    return CPropertyPage::OnSetActive();
}

BOOL ClassicSciencePage::OnKillActive() {
    KillTimer(kPollTimer);
    KillTimer(kAnimationTimer);
    return CPropertyPage::OnKillActive();
}

void ClassicSciencePage::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kPollTimer) {
        if (!sheet_.paused()) poll();
    } else if (timer_id == kAnimationTimer) {
        tick();
    } else {
        CPropertyPage::OnTimer(timer_id);
    }
}

void ClassicSciencePage::OnCloseKit() {
    sheet_.request_game_quit();
}

void ClassicSciencePage::replace_picture(UINT control, PaintedView& view, PaintedView::Painter painter) {
    CWnd* picture = GetDlgItem(control);
    if (picture == nullptr) return;
    CRect area;
    picture->GetWindowRect(&area);
    ScreenToClient(&area);
    picture->ShowWindow(SW_HIDE);
    view.create(*this, 0x7f00 + control, std::move(painter));
    view.ModifyStyle(0, WS_CLIPSIBLINGS);
    view.SetWindowPos(&wndBottom, area.left, area.top, area.Width(), area.Height(), SWP_NOACTIVATE);
}

void ClassicSciencePage::set_template_bitmaps(std::initializer_list<UINT> bitmaps) {
    const c1kitshell::ClassicArt* art = sheet_.classic_art();
    if (art == nullptr) return;
    auto next = bitmaps.begin();
    for (CWnd* child = GetWindow(GW_CHILD); child != nullptr && next != bitmaps.end();
         child = child->GetNextWindow()) {
        if (child->GetDlgCtrlID() != 0xffff || (child->GetStyle() & SS_TYPEMASK) != SS_BITMAP) continue;
        HBITMAP bitmap = static_cast<HBITMAP>(
            ::LoadImage(art->module(), MAKEINTRESOURCE(*next++), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        if (bitmap != nullptr) child->SendMessage(STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bitmap));
    }
}

// ===========================================================================
// Biochemistry: CMonitorPage, dialog 133
// ===========================================================================
//
// Four chemicals, each in its box beside its colour mark (bitmaps 176-179),
// sampled once a second (SubmitSelectedChemicalValues @ 0x004057d0) and
// drawn as the 1996 graph drew them (DrawObservableGraphFrame @ 0x004053c0):
// white, the axes 28 pixels in from the left and 20 up from the foot with a
// tick every 16, "MAX", "0" and "Time", each sample a four-pixel step of
// every line, scrolling once the lines reach the right.  Themes of four:
// picking one fills the boxes; Add names the four now (the Add Theme box,
// dialog 185) and Remove drops the one picked.

namespace {
constexpr UINT kChemicalBoxes[4] = {1028, 1033, 1034, 1035};
constexpr UINT kGraph = 1019;
constexpr UINT kThemeBox = 1029;
constexpr UINT kRemoveTheme = 1113;
constexpr UINT kAddTheme = 1114;
constexpr COLORREF kLineColours[4] = {RGB(255, 0, 0), RGB(0, 0, 255), RGB(0, 128, 0), RGB(128, 0, 128)};
constexpr int kGraphStep = 4;
constexpr unsigned kStringMax = 104;

// The Add Theme box: a name, Enter to add it.
class AddThemeDialog : public CDialog {
public:
    explicit AddThemeDialog(CWnd* parent) : CDialog(185, parent) {}
    CString name;

protected:
    void OnOK() override {
        GetDlgItemText(1114, name);
        name.Trim();
        CDialog::OnOK();
    }
};
}  // namespace

BEGIN_MESSAGE_MAP(ClassicMonitorPage, ClassicSciencePage)
    ON_CBN_SELCHANGE(1028, &ClassicMonitorPage::OnChemicalChanged)
    ON_CBN_SELCHANGE(1033, &ClassicMonitorPage::OnChemicalChanged)
    ON_CBN_SELCHANGE(1034, &ClassicMonitorPage::OnChemicalChanged)
    ON_CBN_SELCHANGE(1035, &ClassicMonitorPage::OnChemicalChanged)
    ON_CBN_SELCHANGE(kThemeBox, &ClassicMonitorPage::OnThemeChanged)
    ON_BN_CLICKED(kAddTheme, &ClassicMonitorPage::OnAddTheme)
    ON_BN_CLICKED(kRemoveTheme, &ClassicMonitorPage::OnRemoveTheme)
END_MESSAGE_MAP()

ClassicMonitorPage::ClassicMonitorPage(ScienceSheet& sheet)
    : ClassicSciencePage(sheet, 133, kStringBiochemistryTab, 1000) {}

void ClassicMonitorPage::init() {
    set_template_bitmaps({176, 177, 178, 179});
    replace_picture(kGraph, graph_, [this](CDC& dc, const CRect& rect) { draw_graph(dc, rect); });
    std::uint32_t saved = 0;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Classic Chemicals", saved);
    }
    const std::vector<std::string>& names = sheet_.chemical_names();
    for (int slot = 0; slot < 4; ++slot) {
        auto* box = static_cast<CComboBox*>(GetDlgItem(kChemicalBoxes[slot]));
        if (box == nullptr) continue;
        box->SetItemData(box->AddString(_T("<NONE>")), 0);
        for (int chemical = 1; chemical < static_cast<int>(names.size()); ++chemical) {
            if (!c1kit::chemical_is_named(names[static_cast<std::size_t>(chemical)])) continue;
            box->SetItemData(box->AddString(CString(names[static_cast<std::size_t>(chemical)].c_str())),
                             static_cast<DWORD_PTR>(chemical));
        }
        const int wanted = static_cast<int>((saved >> (8 * slot)) & 0xff);
        box->SetCurSel(0);
        for (int i = 0; i < box->GetCount(); ++i) {
            if (static_cast<int>(box->GetItemData(i)) == wanted) box->SetCurSel(i);
        }
    }
    fill_themes();
    // RestoreSelectedTheme @ 0x00403cd0: with nothing chosen yet, the first
    // theme (the stock file's "Custom").
    if (saved == 0 && !sheet_.themes().empty()) {
        if (auto* box = static_cast<CComboBox*>(GetDlgItem(kThemeBox))) box->SetCurSel(0);
        OnThemeChanged();
    }
}

int ClassicMonitorPage::chemical_in(int slot) const {
    const auto* box = static_cast<const CComboBox*>(GetDlgItem(kChemicalBoxes[slot]));
    const int index = box != nullptr ? box->GetCurSel() : -1;
    return index < 0 ? 0 : static_cast<int>(box->GetItemData(index));
}

void ClassicMonitorPage::fill_themes() {
    auto* box = static_cast<CComboBox*>(GetDlgItem(kThemeBox));
    if (box == nullptr) return;
    box->ResetContent();
    for (const c1kit::ChemicalTheme& theme : sheet_.themes()) box->AddString(CString(theme.name.c_str()));
}

void ClassicMonitorPage::OnChemicalChanged() {
    std::uint32_t saved = 0;
    for (int slot = 0; slot < 4; ++slot) saved |= static_cast<std::uint32_t>(chemical_in(slot) & 0xff) << (8 * slot);
    if (c1kit::KitSettings* settings = sheet_.settings()) settings->write_dword("Classic Chemicals", saved);
    samples_.clear();  // RefreshObservableView @ 0x00404de0: a new graph
    graph_.redraw();
    poll();
}

void ClassicMonitorPage::OnThemeChanged() {
    const auto* box = static_cast<CComboBox*>(GetDlgItem(kThemeBox));
    const int index = box != nullptr ? box->GetCurSel() : -1;
    const std::vector<c1kit::ChemicalTheme>& themes = sheet_.themes();
    if (index < 0 || index >= static_cast<int>(themes.size())) return;
    const c1kit::ChemicalTheme& theme = themes[static_cast<std::size_t>(index)];
    for (int slot = 0; slot < 4; ++slot) {
        auto* chemical_box = static_cast<CComboBox*>(GetDlgItem(kChemicalBoxes[slot]));
        if (chemical_box == nullptr) continue;
        const int wanted = slot < static_cast<int>(theme.chemicals.size()) ? theme.chemicals[static_cast<std::size_t>(slot)] : 0;
        chemical_box->SetCurSel(0);
        for (int i = 0; i < chemical_box->GetCount(); ++i) {
            if (static_cast<int>(chemical_box->GetItemData(i)) == wanted) chemical_box->SetCurSel(i);
        }
    }
    OnChemicalChanged();
}

void ClassicMonitorPage::OnAddTheme() {
    AddThemeDialog dialog(this);
    if (dialog.DoModal() != IDOK || dialog.name.IsEmpty()) return;
    c1kit::ChemicalTheme theme;
    theme.name = std::string(CStringA(dialog.name));
    for (int slot = 0; slot < 4; ++slot) {
        if (chemical_in(slot) != 0) theme.chemicals.push_back(static_cast<std::uint8_t>(chemical_in(slot)));
    }
    sheet_.themes().push_back(theme);
    sheet_.save_themes();
    fill_themes();
    if (auto* box = static_cast<CComboBox*>(GetDlgItem(kThemeBox))) box->SetCurSel(box->GetCount() - 1);
}

void ClassicMonitorPage::OnRemoveTheme() {
    const auto* box = static_cast<CComboBox*>(GetDlgItem(kThemeBox));
    const int index = box != nullptr ? box->GetCurSel() : -1;
    std::vector<c1kit::ChemicalTheme>& themes = sheet_.themes();
    if (index < 0 || index >= static_cast<int>(themes.size())) return;
    themes.erase(themes.begin() + index);
    sheet_.save_themes();
    fill_themes();
}

void ClassicMonitorPage::poll() {
    if (!sheet_.subject().present) return;
    std::vector<int> chemicals;
    for (int slot = 0; slot < 4; ++slot) chemicals.push_back(chemical_in(slot));
    std::string reply;
    std::vector<int> values;
    if (!sheet_.query(c1kit::chemical_levels_query(chemicals), reply) || !c1kit::parse_values(reply, 4, values)) {
        return;
    }
    std::array<int, 4> sample = {};
    for (int slot = 0; slot < 4; ++slot) sample[static_cast<std::size_t>(slot)] = (std::max)(0, (std::min)(255, values[static_cast<std::size_t>(slot)]));
    samples_.push_back(sample);
    graph_.redraw();
}

void ClassicMonitorPage::draw_graph(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(255, 255, 255));
    CBrush border(RGB(0, 0, 0));
    dc.FrameRect(rect, &border);
    const CRect plot(rect.left + 0x1c, rect.top + 6, rect.right - 6, rect.bottom - 0x14);
    CPen axis(PS_SOLID, 1, RGB(0, 0, 0));
    CPen* previous_pen = dc.SelectObject(&axis);
    dc.MoveTo(plot.left, plot.bottom);
    dc.LineTo(plot.right, plot.bottom);
    for (int x = plot.left; x < plot.right; x += 16) {
        dc.MoveTo(x, plot.bottom);
        dc.LineTo(x, plot.bottom + 2);
    }
    dc.MoveTo(plot.left, plot.bottom);
    dc.LineTo(plot.left, plot.top);
    for (int y = plot.bottom; y > plot.top; y -= 16) {
        dc.MoveTo(plot.left, y);
        dc.LineTo(plot.left - 2, y);
    }
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(0, 0, 0));
    dc.SetTextAlign(TA_LEFT | TA_TOP);
    dc.TextOut(rect.left + 2, plot.top, c1kitshell::load_string(kStringMax));
    dc.SetTextAlign(TA_LEFT | TA_BOTTOM);
    dc.TextOut(rect.left + 2, plot.bottom + 1, _T("0"));
    dc.SetTextAlign(TA_CENTER | TA_BOTTOM);
    dc.TextOut((rect.left + rect.right) / 2, rect.bottom - 2, c1kitshell::load_string(kStringTime));
    dc.SetTextAlign(TA_LEFT | TA_TOP);
    // The lines: the newest samples that fit, a step apart.
    const CRect lines(plot.left + 1, plot.top + 1, plot.right - 4, plot.bottom - 1);
    const std::size_t room = static_cast<std::size_t>((std::max)(1, lines.Width() / kGraphStep));
    const std::size_t first = samples_.size() > room + 1 ? samples_.size() - room - 1 : 0;
    const auto y_of = [&](int value) { return lines.bottom - value * lines.Height() / 256; };
    for (int slot = 0; slot < 4; ++slot) {
        CPen pen(PS_SOLID, 1, kLineColours[slot]);
        dc.SelectObject(&pen);
        for (std::size_t i = first + 1; i < samples_.size(); ++i) {
            const int x = lines.left + static_cast<int>(i - first - 1) * kGraphStep;
            dc.MoveTo(x, y_of(samples_[i - 1][static_cast<std::size_t>(slot)]));
            dc.LineTo(x + kGraphStep, y_of(samples_[i][static_cast<std::size_t>(slot)]));
        }
        dc.SelectObject(&axis);
    }
    dc.SelectObject(previous_pen);
    if (samples_.size() > room * 2) samples_.erase(samples_.begin(), samples_.end() - room - 1);
}

// ===========================================================================
// Genetics: CChromosonePage, dialog 143
// ===========================================================================
//
// Gender, species, moniker and "fingerprint" (the last of `dde: gene`'s
// fourteen figures); the brain's neurons, lobes and the dendrites its genes
// allow; the DNA spinning in its strip (Gene.spr, a frame every 85 ms:
// InitializeGenePageControls @ 0x0040fc00); and the genetic breakdown, a gene
// type at a time with Next, its count, the count times eight as
// "nucleotides" and the total.  Fixes: the species is the creature's
// (bug 4: always "NORN"), the breakdown skips the three types the original
// labelled "Unused", and the total counts every type (the original left
// some out).

namespace {
constexpr UINT kGender = 1155, kSpecies = 1161, kMoniker = 1126, kFingerprint = 1130;
constexpr UINT kNeurons = 1136, kLobes = 1138, kDendrites = 1137;
constexpr UINT kGeneType = 1146, kGeneCount = 1147, kNucleotides = 1148, kTotalGenes = 1156;
constexpr UINT kNext = 1144, kDna = 1143, kTitle = 1142;
// LoadGeneDetailFields @ 0x0040f600: the label for each of `dde: gene`'s
// counts; 0 where the original had "Unused".
constexpr UINT kGeneLabels[13] = {388, 389, 390, 397, 391, 0, 0, 0, 392, 393, 394, 395, 396};
}  // namespace

BEGIN_MESSAGE_MAP(ClassicGeneticsPage, ClassicSciencePage)
    ON_BN_CLICKED(kNext, &ClassicGeneticsPage::OnNext)
END_MESSAGE_MAP()

ClassicGeneticsPage::ClassicGeneticsPage(ScienceSheet& sheet)
    : ClassicSciencePage(sheet, 143, kStringGeneticsTab, 0) {}

void ClassicGeneticsPage::init() {
    load_palette(palette_);
    std::vector<c1kitshell::KitSprite> set;
    if (c1kitshell::KitSprite::load_set(main_file("Gene.spr"), set) && !set.empty()) gene_ = set[0];
    replace_picture(kDna, dna_, [this](CDC& dc, const CRect& rect) { draw_dna(dc, rect); });
    LOGFONT face = {};
    GetFont()->GetLogFont(&face);
    face.lfWeight = FW_BOLD;
    title_font_.CreateFontIndirect(&face);
    if (CWnd* title = GetDlgItem(kTitle)) title->SetFont(&title_font_);
    start_animation(85);
}

BOOL ClassicGeneticsPage::OnSetActive() {
    fill();  // ActivateGenePage @ 0x0040fdc0
    return ClassicSciencePage::OnSetActive();
}

void ClassicGeneticsPage::tick() {
    if (++frame_ >= gene_.frame_count()) frame_ = 0;
    dna_.redraw();
}

void ClassicGeneticsPage::draw_dna(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    const c1kitshell::KitSprite::Frame* frame = gene_.frame(frame_);
    if (frame == nullptr || !canvas_.create(frame->width, frame->height)) return;
    canvas_.draw_frame(gene_, frame_, 0, 0, palette_);
    canvas_.present(dc, rect.left, rect.top, (std::min)(frame->width, rect.Width()),
                    (std::min)(frame->height, rect.Height()));
}

void ClassicGeneticsPage::fill() {
    const Subject& subject = sheet_.subject();
    const auto set = [this](UINT id, const CString& text) { SetDlgItemText(id, text); };
    const auto number = [](long value) {
        CString text;
        text.Format(_T("%ld"), value);
        return text;
    };
    counts_.clear();
    if (!subject.present) {
        for (UINT id : {kGender, kSpecies, kMoniker, kFingerprint, kNeurons, kLobes, kDendrites, kGeneType,
                        kGeneCount, kNucleotides, kTotalGenes}) {
            set(id, CString());
        }
        return;
    }
    set(kGender, c1kitshell::load_string(subject.sex == 1 ? kStringMale : kStringFemale));
    std::vector<c1kit::Gene> genes;
    std::size_t bytes = 0;
    const bool have_genome = sheet_.read_genome(genes, bytes);
    const c1kit::GenomeSummary summary = c1kit::summarize_genome(genes, bytes);
    CString species = have_genome && summary.has_genus ? CString(c1kit::species_name(summary.genus)) : CString();
    species.MakeUpper();
    set(kSpecies, species);
    set(kMoniker, CString(c1kit::moniker_characters(subject.moniker).c_str()));
    std::string reply;
    if (sheet_.query("dde: gene,endm", reply)) c1kit::parse_values(reply, 14, counts_);
    set(kFingerprint, counts_.size() > 13 ? number(counts_[13]) : CString());
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    long neurons = 0;
    std::vector<int> per_lobe;
    for (const c1kit::LobeLayout& lobe : lobes) {
        neurons += lobe.neurons();
        per_lobe.push_back(lobe.neurons());
    }
    set(kNeurons, number(neurons));
    set(kLobes, number(static_cast<long>(lobes.size())));
    CString dendrites;
    if (have_genome) {
        long fewest = 0, most = 0;
        c1kit::dendrite_range(summary.lobes, per_lobe, fewest, most);
        dendrites = fewest == most ? number(fewest) : number(fewest) + _T(" - ") + number(most);
    }
    set(kDendrites, dendrites);
    group_ = 1;
    show_group();
}

void ClassicGeneticsPage::show_group() {
    long total = 0;
    for (std::size_t i = 0; i < counts_.size() && i < 13; ++i) total += counts_[i];
    const int count = group_ < static_cast<int>(counts_.size()) ? counts_[static_cast<std::size_t>(group_)] : 0;
    CString text;
    SetDlgItemText(kGeneType, c1kitshell::load_string(kGeneLabels[group_]));
    text.Format(_T("%d"), count);
    SetDlgItemText(kGeneCount, text);
    text.Format(_T("%d"), count * 8);
    SetDlgItemText(kNucleotides, text);
    text.Format(_T("%ld"), total);
    SetDlgItemText(kTotalGenes, text);
}

void ClassicGeneticsPage::OnNext() {
    do {
        group_ = (group_ + 1) % 13;
    } while (kGeneLabels[group_] == 0);
    show_group();
}

// ===========================================================================
// Brain scanner: CScannerPage, dialog 134
// ===========================================================================
//
// Scanner.bmp, and on it the brain report once a second: each of the first
// 64 x 48 cells whose level is above 0 a five-pixel cross at (8 + 4x, 5 + 4y)
// in its level's colour (InitializeCellCoordinates @ 0x004015a0 and its
// palette table, RenderCell @ 0x00401700).  It asks for activation, as the
// original did; its game answered firing strength (bug 5).

namespace {
constexpr std::uint8_t kLevelColours[16] = {0x25, 0x29, 0x2b, 0x67, 0x6b, 0x72, 0x80, 0x82,
                                            0x86, 0xa9, 0xac, 0xb5, 0xbc, 0xbd, 0xe4, 0xf2};
}  // namespace

ClassicScannerPage::ClassicScannerPage(ScienceSheet& sheet)
    : ClassicSciencePage(sheet, 134, kStringBrainTab, 1000), activity_(std::make_unique<c1kit::BrainActivity>()) {}

void ClassicScannerPage::init() {
    load_palette(palette_);
    replace_picture(1019, picture_, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
}

void ClassicScannerPage::poll() {
    std::string reply;
    have_report_ = sheet_.subject().present &&
                   sheet_.brain_report(c1kit::kReportActivation, 0, reply);
    activity_->clear();
    if (have_report_) c1kit::parse_activity_report(reply, *activity_);
    picture_.redraw();
}

void ClassicScannerPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    c1kitshell::PaletteBitmap scanner;
    if (!scanner.load_file(CString(main_file("Scanner.bmp").c_str())) ||
        !canvas_.create(scanner.width(), scanner.height())) {
        return;
    }
    canvas_.draw_bitmap_file(main_file("Scanner.bmp"), 0, 0);
    if (have_report_) {
        for (int x = 0; x < 64; ++x) {
            for (int y = 0; y < 48; ++y) {
                const int level = activity_->reported[x][y] ? activity_->level[x][y] : 0;
                if (level <= 0) continue;
                const std::uint32_t colour = palette_.colour(kLevelColours[(std::min)(level, 15)]);
                const int px = 8 + 4 * x, py = 5 + 4 * y;
                canvas_.set_pixel(px, py, colour);
                canvas_.set_pixel(px - 1, py, colour);
                canvas_.set_pixel(px + 1, py, colour);
                canvas_.set_pixel(px, py - 1, colour);
                canvas_.set_pixel(px, py + 1, colour);
            }
        }
    }
    canvas_.present(dc, rect.left, rect.top, (std::min)(scanner.width(), rect.Width()),
                    (std::min)(scanner.height(), rect.Height()));
}

// ===========================================================================
// Decisions: CDecisionPage, dialog 135
// ===========================================================================
//
// RunScienceExperimentCaosScript @ 0x00407840's query once a second: each
// action's output as a short dark-red bar in its template box, named from
// decision.str, and the reward and punishment chemicals under them.

namespace {
constexpr UINT kDecisionBars[12] = {1046, 1051, 1055, 1059, 1063, 1067, 1049, 1053, 1057, 1061, 1065, 1069};
constexpr UINT kDecisionLabels[12] = {1045, 1052, 1056, 1060, 1064, 1068, 1050, 1054, 1058, 1062, 1066, 1070};
constexpr UINT kRewardBar = 1079, kPunishmentBar = 1081;
}  // namespace

BEGIN_MESSAGE_MAP(ClassicDecisionsPage, ClassicSciencePage)
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

ClassicDecisionsPage::ClassicDecisionsPage(ScienceSheet& sheet)
    : ClassicSciencePage(sheet, 135, kStringDecisionsTab, 1000) {}

void ClassicDecisionsPage::init() {
    const std::vector<std::string>& names = sheet_.neuron_names().decisions;
    for (int i = 0; i < 12; ++i) {
        if (i < static_cast<int>(names.size())) SetDlgItemText(kDecisionLabels[i], CString(names[static_cast<std::size_t>(i)].c_str()));
        // The template's names are too narrow for "Deactivate": each is
        // widened leftwards to fit.
        if (CWnd* label = GetDlgItem(kDecisionLabels[i])) {
            CRect area;
            label->GetWindowRect(&area);
            ScreenToClient(&area);
            CRect extra(0, 0, 5, 0);
            MapDialogRect(&extra);
            label->SetWindowPos(nullptr, area.left - extra.Width(), area.top, area.Width() + extra.Width(),
                                area.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (CWnd* bar = GetDlgItem(kDecisionBars[i])) bar->ModifyStyle(SS_TYPEMASK, SS_OWNERDRAW);
    }
    for (UINT id : {kRewardBar, kPunishmentBar}) {
        if (CWnd* bar = GetDlgItem(id)) bar->ModifyStyle(SS_TYPEMASK, SS_OWNERDRAW);
    }
}

void ClassicDecisionsPage::poll() {
    std::string reply;
    std::vector<int> values;
    if (!sheet_.subject().present || !sheet_.query(c1kit::decisions_query(16), reply) ||
        !c1kit::parse_values(reply, 3 + 7 * 16, values)) {
        return;
    }
    reward_ = values[1];
    punishment_ = values[2];
    outputs_.assign(16, 0);
    for (int n = 0; n < 16; ++n) outputs_[static_cast<std::size_t>(n)] = values[static_cast<std::size_t>(3 + n * 7)];
    for (UINT id : kDecisionBars) {
        if (CWnd* bar = GetDlgItem(id)) bar->Invalidate(FALSE);
    }
    for (UINT id : {kRewardBar, kPunishmentBar}) {
        if (CWnd* bar = GetDlgItem(id)) bar->Invalidate(FALSE);
    }
}

void ClassicDecisionsPage::OnDrawItem(int id, LPDRAWITEMSTRUCT draw) {
    int value = -1;
    for (int i = 0; i < 12; ++i) {
        if (static_cast<UINT>(id) == kDecisionBars[i]) value = i < static_cast<int>(outputs_.size()) ? outputs_[static_cast<std::size_t>(i)] : 0;
    }
    if (static_cast<UINT>(id) == kRewardBar) value = reward_;
    if (static_cast<UINT>(id) == kPunishmentBar) value = punishment_;
    if (value < 0) {
        CPropertyPage::OnDrawItem(id, draw);
        return;
    }
    CDC* dc = CDC::FromHandle(draw->hDC);
    CRect area = draw->rcItem;
    dc->FillSolidRect(area, RGB(255, 255, 255));
    dc->DrawEdge(&area, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
    const int length = area.Width() * (std::max)(0, (std::min)(255, value)) / 255;
    dc->FillSolidRect(area.left, area.top, length, area.Height(), RGB(128, 0, 0));
}

// ===========================================================================
// Injections: CInjectPage, dialog 144
// ===========================================================================
//
// The syringe (DOSE.bmp and the medicine's dosage sprite, by the original's
// table), the upright slider (the dose, most at the top), the medicines
// (injections.str, chemicals 100 on) and Go, which injects the dose and
// plays the syringe's injection.  The original's table turned Adrenaline
// blue by falling through into the next case; here it is orange.

namespace {
constexpr UINT kSyringe = 1140, kSlider = 1109, kMedicines = 1110, kGo = 1111;
}  // namespace

BEGIN_MESSAGE_MAP(ClassicInjectionsPage, ClassicSciencePage)
    ON_WM_VSCROLL()
    ON_CBN_SELCHANGE(kMedicines, &ClassicInjectionsPage::OnMedicineChanged)
    ON_BN_CLICKED(kGo, &ClassicInjectionsPage::OnGo)
END_MESSAGE_MAP()

ClassicInjectionsPage::ClassicInjectionsPage(ScienceSheet& sheet)
    : ClassicSciencePage(sheet, 144, kStringInjectionsTab, 0) {}

void ClassicInjectionsPage::init() {
    load_palette(palette_);
    syringe_.load(std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))),
                  kMedicineLiquids[0], palette_);
    liquid_ = 0;
    replace_picture(kSyringe, syringe_view_, [this](CDC& dc, const CRect& rect) { syringe_.draw(dc, rect); });
    if (auto* slider = static_cast<CSliderCtrl*>(GetDlgItem(kSlider))) {
        slider->SetRange(0, 255);
        slider->SetTicFreq(32);
    }
    std::uint32_t selected = 0, dose = 32;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Chemical", selected);
        settings->read_dword(c1kit::SettingsScope::user, "Dose", dose);
    }
    if (auto* medicines = static_cast<CComboBox*>(GetDlgItem(kMedicines))) {
        for (const std::string& name : sheet_.medicines()) medicines->AddString(CString(name.c_str()));
        medicines->SetCurSel(selected < sheet_.medicines().size() ? static_cast<int>(selected) : 0);
    }
    set_dose(dose <= 255 ? static_cast<int>(dose) : 32);
    change_liquid();
}

int ClassicInjectionsPage::dose() const {
    const auto* slider = static_cast<const CSliderCtrl*>(GetDlgItem(kSlider));
    return slider != nullptr ? 255 - slider->GetPos() : 0;
}

void ClassicInjectionsPage::set_dose(int dose) {
    if (auto* slider = static_cast<CSliderCtrl*>(GetDlgItem(kSlider))) slider->SetPos(255 - dose);
    syringe_.set_dose(dose);
    syringe_view_.redraw();
}

void ClassicInjectionsPage::change_liquid() {
    const auto* medicines = static_cast<const CComboBox*>(GetDlgItem(kMedicines));
    const int index = medicines != nullptr ? medicines->GetCurSel() : -1;
    const int count = static_cast<int>(sizeof(kMedicineLiquids) / sizeof(kMedicineLiquids[0]));
    const int liquid = index < 0 ? 0 : (std::min)(index, count);
    if (liquid != liquid_ && syringe_.set_liquid(liquid < count ? kMedicineLiquids[liquid] : kOtherMedicineLiquid)) {
        liquid_ = liquid;
        syringe_view_.redraw();
    }
}

void ClassicInjectionsPage::OnVScroll(UINT code, UINT position, CScrollBar* bar) {
    ClassicSciencePage::OnVScroll(code, position, bar);
    if (syringe_.injecting()) return;
    syringe_.set_dose(dose());
    syringe_view_.redraw();
    if (c1kit::KitSettings* settings = sheet_.settings()) settings->write_dword("Dose", static_cast<std::uint32_t>(dose()));
}

void ClassicInjectionsPage::OnMedicineChanged() {
    const auto* medicines = static_cast<const CComboBox*>(GetDlgItem(kMedicines));
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Chemical", static_cast<std::uint32_t>(medicines->GetCurSel()));
    }
    change_liquid();
}

// SendChemicalInjectionStep @ 0x0040aaf0, and the syringe's injection.
void ClassicInjectionsPage::OnGo() {
    const auto* medicines = static_cast<const CComboBox*>(GetDlgItem(kMedicines));
    const int index = medicines != nullptr ? medicines->GetCurSel() : -1;
    if (index < 0 || dose() <= 0 || !sheet_.subject().present) return;
    sheet_.run(c1kit::injection_script(c1kit::kFirstMedicineChemical + index, dose()));
    if (!syringe_.injecting()) {
        syringe_.start_injection();
        SetTimer(kAnimationTimer, c1kitshell::Syringe::kTickMs, nullptr);
    }
}

void ClassicInjectionsPage::tick() {
    const bool more = syringe_.tick();
    if (auto* slider = static_cast<CSliderCtrl*>(GetDlgItem(kSlider))) {
        slider->SetPos(255 - (more ? syringe_.shown_dose() : syringe_.dose()));
    }
    syringe_view_.redraw();
    if (!more) KillTimer(kAnimationTimer);
}

}  // namespace science
