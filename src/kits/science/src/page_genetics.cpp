// Genetics: the creature's genome, as a list of properties.
//
// The 1996 page (CChromosonePage, dialog 143) showed a few read-only boxes,
// a spinning DNA animation, and a "Genetic Breakdown" stepped through one
// gene type at a time with Next.  Its Species always said NORN (the box's
// template text: control 1161 is neither data-bound nor set), and its
// breakdown came from the game's count of the genes switched on for the
// creature's sex and age, with "nucleotides" the count times eight
// (CChromosonePage::RebuildGeneDisplayMetrics @ 0x0040f8e0).
//
// Here everything is one list, property and value, as a file's Details are:
// the creature, its brain, and every gene type with its count and size,
// read from the genome file itself (<Genetics>\<moniker>.gen), alongside the
// game's count of those switched on now.

#include "science.hpp"
#include "science_ids.hpp"

#include <fstream>
#include <iterator>

namespace science {
namespace {

// The game's `dde: gene` counts, in its order, by gene family and subtype:
// the brain lobe genes, the five biochemistry types, then the seven creature
// types.  -1: not counted.
int gene_count_index(std::uint8_t family, std::uint8_t subtype) {
    if (family == c1kit::kGeneFamilyBrain && subtype == 0) return 0;
    if (family == c1kit::kGeneFamilyBiochemistry && subtype <= 4) return 1 + subtype;
    if (family == c1kit::kGeneFamilyCreature && subtype <= 6) return 6 + subtype;
    return -1;
}

CString number(long value) {
    CString text;
    text.Format(_T("%ld"), value);
    return text;
}

} // namespace

GeneticsPage::GeneticsPage(ScienceSheet& sheet) : SciencePage(sheet, kStringGeneticsTab) {}

void GeneticsPage::create_controls() {
    make(details_, WC_LISTVIEW, _T(""),
         LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP,
         kControlDetails);
    details_.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    details_.InsertColumn(0, _T("Property"), LVCFMT_LEFT, 180);
    details_.InsertColumn(1, _T("Value"), LVCFMT_LEFT, 300);
    LOGFONT log = {};
    sheet_.font().GetLogFont(&log);
    log.lfWeight = FW_BOLD;
    heading_font_.CreateFontIndirect(&log);
    fill();
}

void GeneticsPage::layout(int width, int height) {
    const int margin = 7;
    const int row = text_height() + 8;
    place(details_, margin, margin, width - 2 * margin, height - 3 * margin - row);
    place(close_, width - margin - 84, height - margin - row, 84, row);
    const int property_width = (std::max)(160, (width - 2 * margin) * 2 / 5);
    details_.SetColumnWidth(0, property_width);
    details_.SetColumnWidth(1, width - 2 * margin - property_width - GetSystemMetrics(SM_CXVSCROLL) - 4);
}

void GeneticsPage::subject_changed() {
    if (created_) {
        fill();
    }
}

void GeneticsPage::add_heading(const CString& text) {
    const int row = details_.InsertItem(details_.GetItemCount(), text);
    heading_rows_.resize(static_cast<std::size_t>(row) + 1, false);
    heading_rows_[static_cast<std::size_t>(row)] = true;
}

void GeneticsPage::add_row(const CString& property, const CString& value) {
    const int row = details_.InsertItem(details_.GetItemCount(), _T("    ") + property);
    details_.SetItemText(row, 1, value);
    heading_rows_.resize(static_cast<std::size_t>(row) + 1, false);
}

void GeneticsPage::fill() {
    details_.SetRedraw(FALSE);
    details_.DeleteAllItems();
    heading_rows_.clear();
    const Subject& subject = sheet_.subject();
    if (!subject.present) {
        add_heading(_T("No creature is selected in the game."));
        details_.SetRedraw(TRUE);
        return;
    }

    // The genome file and the game's counts of genes switched on now.
    const std::string genome_name = c1kit::genome_file_name(subject.moniker);
    std::vector<c1kit::Gene> genes;
    std::size_t genome_bytes = 0;
    const bool have_genome = sheet_.read_genome(genes, genome_bytes);
    const c1kit::GenomeSummary summary = c1kit::summarize_genome(genes, genome_bytes);
    std::vector<int> active;
    std::string reply;
    if (sheet_.query("dde: gene,endm", reply)) {
        c1kit::parse_values(reply, 13, active);
    }

    add_heading(_T("Creature"));
    add_row(_T("Name"), CString(subject.name.c_str()));
    add_row(_T("Moniker"), CString(c1kit::moniker_characters(subject.moniker).c_str()) +
                               _T("  (") + CString(subject.moniker.c_str()) + _T(")"));
    add_row(_T("Sex"), c1kitshell::load_string(subject.sex == 1 ? kStringMale : kStringFemale));
    // Fix (bug 4): the species from the genus gene; the original always
    // showed its template's "NORN".
    add_row(_T("Species"), have_genome && summary.has_genus
                               ? CString(c1kit::species_name(summary.genus))
                               : CString(_T("Unknown")));
    add_row(_T("Age"), CString(subject.age.c_str()));
    const auto parent = [this](const std::string& moniker) {
        if (moniker.empty() || moniker == "0") {
            return CString(_T("None (made, not born)"));
        }
        const CString name = sheet_.name_for_moniker(moniker);
        const CString characters(c1kit::moniker_characters(moniker).c_str());
        return name.IsEmpty() ? characters : name + _T("  (") + characters + _T(")");
    };
    if (have_genome && summary.has_genus) {
        add_row(_T("Mother"), parent(summary.mother));
        add_row(_T("Father"), parent(summary.father));
    }

    add_heading(_T("Brain"));
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    long neurons = 0;
    std::vector<int> per_lobe;
    for (const c1kit::LobeLayout& lobe : lobes) {
        neurons += lobe.neurons();
        per_lobe.push_back(lobe.neurons());
    }
    add_row(_T("Lobes"), number(static_cast<long>(lobes.size())));
    add_row(_T("Neurons"), number(neurons));
    if (have_genome) {
        long fewest = 0, most = 0;
        c1kit::dendrite_range(summary.lobes, per_lobe, fewest, most);
        add_row(_T("Dendrites (the genes allow)"),
                fewest == most ? number(fewest) : number(fewest) + _T(" to ") + number(most));
    }
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        CString value;
        value.Format(_T("%d x %d = %d neurons, at %d,%d"), lobes[i].width, lobes[i].height,
                     lobes[i].neurons(), lobes[i].x, lobes[i].y);
        add_row(CString(c1kit::lobe_name(static_cast<int>(i))) + _T(" lobe"), value);
    }

    add_heading(_T("Genome"));
    if (!have_genome) {
        add_row(_T("Genome file"), CString(genome_name.c_str()) + _T(" could not be read"));
        details_.SetRedraw(TRUE);
        return;
    }
    add_row(_T("Genome file"), CString(genome_name.c_str()));
    add_row(_T("Size"), number(summary.total_bytes) + _T(" bytes"));
    long total_active = 0;
    for (const int count : active) {
        total_active += count;
    }
    add_row(_T("Genes"), number(summary.total_genes) +
                             (active.empty() ? CString() : _T(", ") + number(total_active) + _T(" switched on now")));
    add_row(_T("Male-only genes"), number(summary.male_only_genes));
    add_row(_T("Female-only genes"), number(summary.female_only_genes));
    add_row(_T("Genes that can mutate"), number(summary.mutable_genes));
    add_row(_T("Highest generation"), number(summary.highest_generation));

    add_heading(_T("Genes by type"));
    for (const c1kit::GeneTypeCount& type : summary.types) {
        const char* name = c1kit::gene_type_name(type.family, type.subtype);
        CString property = name != nullptr ? CString(name) : CString();
        if (name == nullptr) {
            property.Format(_T("Unknown type %d.%d"), type.family, type.subtype);
        }
        CString value = number(type.genes) + (type.genes == 1 ? _T(" gene, ") : _T(" genes, ")) +
                        number(type.bytes) + _T(" bytes");
        const int index = gene_count_index(type.family, type.subtype);
        if (index >= 0 && index < static_cast<int>(active.size())) {
            value += _T(" (") + number(active[static_cast<std::size_t>(index)]) + _T(" on now)");
        }
        add_row(property, value);
    }
    details_.SetRedraw(TRUE);
}

// Headings in bold on a shaded row.
BOOL GeneticsPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kControlDetails && header->code == NM_CUSTOMDRAW) {
        auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(lparam);
        switch (draw->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            *result = CDRF_NOTIFYITEMDRAW;
            return TRUE;
        case CDDS_ITEMPREPAINT: {
            const std::size_t row = draw->nmcd.dwItemSpec;
            if (row < heading_rows_.size() && heading_rows_[row]) {
                draw->clrTextBk = RGB(228, 232, 240);
                draw->clrText = RGB(20, 40, 90);
                ::SelectObject(draw->nmcd.hdc, heading_font_.GetSafeHandle());
                *result = CDRF_NEWFONT;
            } else {
                *result = CDRF_DODEFAULT;
            }
            return TRUE;
        }
        default:
            break;
        }
    }
    return SciencePage::OnNotify(wparam, lparam, result);
}

} // namespace science
