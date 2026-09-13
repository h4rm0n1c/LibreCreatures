#include "magic_profiler.hpp"

#include "../display/profiler.hpp"

#include <fstream>
#include <sstream>

namespace creatures1::ui {
namespace {

constexpr std::string_view kReportTitle = "Magic Profiler";
constexpr std::string_view kReportFailure = "Failed to create report file.";

std::string report_path_for_save(std::string path) {
    const std::size_t separator = path.find_last_of("/\\");
    const std::size_t extension = path.find_last_of('.');
    if (extension != std::string::npos &&
        (separator == std::string::npos || extension > separator)) {
        path.erase(extension);
    }
    path += "_report.html";
    return path;
}

// GenerateMagicProfilerReport's own row helper: label, then value, with a
// bare "<tr><td>...</td><td>...</td></tr>\n" shape -- confirmed from the
// real string literals at 0x459fdc/0x459fd0/0x459fec.
void write_summary_row(std::ostream& output,
                       std::string_view label,
                       std::uint32_t value) {
    output << "<tr><td>" << label << "</td><td>" << value
           << "</td></tr>\n";
}

void write_summary_row(std::ostream& output,
                       std::string_view label,
                       std::uint32_t value,
                       std::uint32_t capacity) {
    output << "<tr><td>" << label << "</td><td>" << value << " / "
           << capacity << "</td></tr>\n";
}

// Native's real header/CSS, read back byte-for-byte from the image via
// get_referenced_strings against GenerateMagicProfilerReport
// (0x0045a220..0x0045a084) -- no doctype, no charset meta; this is the
// literal document native writes.
void write_html_header(std::ostream& output,
                       const MagicProfilerSnapshot& snapshot) {
    output << "<html><head><title>Creatures 1 World Report</title>\n"
              "<style>\n"
              "body { font-family: 'Segoe UI', Tahoma, sans-serif; margin: 20px; color: #333; }\n"
              "h2 { color: #555; }\n"
              "table { border-collapse: collapse; margin-bottom: 20px; }\n"
              "td, th { border: 1px solid #ccc; padding: 5px 10px; }\n"
              "th { background: #e8e8e8; text-align: left; }\n"
              "tr:nth-child(even) { background: #f4f4f4; }\n"
              "tfoot td { font-weight: bold; background: #e8e8e8; }\n"
              "</style></head><body>\n"
              "<h1>Creatures 1 World Report</h1>\n"
              "<p>"
           << display::EscapeXmlForProfilerReport(snapshot.world_name)
           << " &mdash; tick "
           << display::EscapeXmlForProfilerReport(std::to_string(snapshot.tick_count))
           << "</p>\n";
}

void write_summary(std::ostream& output,
                   const MagicProfilerSnapshot& snapshot) {
    output << "<h2>Summary</h2>\n<table>\n";
    write_summary_row(output, "Creatures", snapshot.creature_count);
    write_summary_row(output, "Objects", snapshot.object_count);
    write_summary_row(output, "Scenery", snapshot.scenery_count);
    write_summary_row(output, "Entities", snapshot.entity_count);
    write_summary_row(output, "Scripts", snapshot.total_script_count);
    write_summary_row(output, "Active Scripts", snapshot.active_script_count);
    write_summary_row(output, "Messages", snapshot.message_queue_count, 200);
    write_summary_row(output, "Delayed Messages",
                      snapshot.delayed_message_count, 200);
    write_summary_row(output, "Stimuli", snapshot.stimulus_count, 200);
    write_summary_row(output, "Death Row", snapshot.death_row_count);
    write_summary_row(output, "Stuffed Norns",
                      snapshot.stuffed_norn_word_count);
    write_summary_row(output, "Idle Time",
                      static_cast<std::uint32_t>(snapshot.smoothed_idle_cycle_time));
    output << "</table>\n";
}

// The classifier column is ONE cell, "family, genus, species" -- confirmed
// via the real snprintf format string read directly out of the image at
// 0x459f88: "%d, %d, %d\0", called from ResolveClassifierDisplayName
// (0x00439310) to build the numeric prefix before the resolved name.
std::string format_classifier_cell(const brain::ClassifierId& classifier) {
    std::ostringstream text;
    text << classifier.family << ", " << classifier.genus << ", "
         << classifier.species;
    return text.str();
}

void write_classifier_rows(std::ostream& output,
                           const MagicProfilerSnapshot& snapshot) {
    output << "<h2>Objects by Classifier</h2>\n"
              "<table>\n<thead><tr>"
              "<th>Classifier</th><th>Name</th><th>Count</th><th>Active Scripts</th>"
              "</tr></thead>\n<tbody>\n";

    std::uint32_t total_population = 0;
    std::uint32_t total_active_scripts = 0;
    for (const MagicProfilerSnapshot::ClassifierRow& row :
         snapshot.classifier_rows) {
        std::string name = row.display_name;
        if (name.empty()) {
            name = brain::ResolveClassifierDisplayName(snapshot.classifier_names,
                                                        row.classifier);
        }
        output << "<tr><td>" << format_classifier_cell(row.classifier)
               << "</td><td>";
        if (name.empty()) {
            output << "<i>Unknown</i>";
        } else {
            output << display::EscapeXmlForProfilerReport(name);
        }
        output << "</td><td>" << row.population << "</td><td>"
               << row.active_script_count << "</td></tr>\n";
        total_population += row.population;
        total_active_scripts += row.active_script_count;
    }

    output << "</tbody>\n<tfoot><tr><td></td><td>Total</td><td>"
           << total_population << "</td><td>" << total_active_scripts
           << "</td></tr></tfoot>\n"
              "</table>\n";
}

}  // namespace

std::string generate_magic_profiler_report(
    MagicProfilerWindowApi& window,
    const MagicProfilerSnapshot& snapshot) {
    const std::string report_path = report_path_for_save(snapshot.world_save_path);
    if (report_path.empty()) {
        window.show_message(kReportTitle, kReportFailure);
        return {};
    }

    std::ofstream report(report_path, std::ios::out | std::ios::trunc);
    if (!report.is_open()) {
        window.show_message(kReportTitle, kReportFailure);
        return {};
    }

    write_html_header(report, snapshot);
    write_summary(report, snapshot);
    write_classifier_rows(report, snapshot);
    report << "</body></html>\n";
    report.flush();
    if (!report.good()) {
        window.show_message(kReportTitle, kReportFailure);
        return {};
    }

    // Native: "Report saved to:\n%s" (0x0045a438).
    window.show_message(kReportTitle, "Report saved to:\n" + report_path);
    return report_path;
}

}  // namespace creatures1::ui
