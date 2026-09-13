#include "magic_profiler.hpp"

#include "../display/profiler.hpp"

#include <fstream>
#include <iomanip>
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

void write_summary_row(std::ostream& output,
                       std::string_view label,
                       std::uint32_t value,
                       std::string_view limit = {}) {
    output << "<tr><th>" << label << "</th><td>" << value;
    if (!limit.empty()) {
        output << " / " << limit;
    }
    output << "</td></tr>\n";
}

void write_html_header(std::ostream& output,
                       const MagicProfilerSnapshot& snapshot) {
    output << "<!doctype html>\n<html><head><meta charset=\"windows-1252\">\n"
              "<title>Creatures 1 World Report</title>\n"
              "<style>body{font-family:sans-serif}table{border-collapse:collapse}"
              "th,td{border:1px solid #888;padding:3px 6px;text-align:left}"
              "</style></head><body>\n"
              "<h1>Creatures 1 World Report</h1>\n"
              "<p>World: "
           << display::EscapeXmlForProfilerReport(snapshot.world_name)
           << "<br>Tick: "
           << display::EscapeXmlForProfilerReport(std::to_string(snapshot.tick_count))
           << "</p>\n";
}

void write_summary(std::ostream& output,
                   const MagicProfilerSnapshot& snapshot) {
    output << "<h2>Summary</h2>\n<table>\n";
    write_summary_row(output, "Creatures", snapshot.creature_count, "2000");
    write_summary_row(output, "Objects", snapshot.object_count);
    write_summary_row(output, "Scenery", snapshot.scenery_count);
    write_summary_row(output, "Entities", snapshot.entity_count);
    write_summary_row(output, "Active scripts", snapshot.active_script_count,
                      "1000");
    write_summary_row(output, "Message queue", snapshot.message_queue_count,
                      "200");
    write_summary_row(output, "Delayed messages",
                      snapshot.delayed_message_count, "200");
    write_summary_row(output, "Stimuli", snapshot.stimulus_count, "200");
    write_summary_row(output, "Death row", snapshot.death_row_count);
    write_summary_row(output, "Stuffed Norns",
                      snapshot.stuffed_norn_word_count);
    output << "<tr><th>Smoothed idle-cycle time</th><td>"
           << std::fixed << std::setprecision(3)
           << snapshot.smoothed_idle_cycle_time << "</td></tr>\n</table>\n";
}

void write_classifier_rows(std::ostream& output,
                           const MagicProfilerSnapshot& snapshot) {
    output << "<h2>Classifiers</h2>\n<table>\n"
              "<tr><th>Family</th><th>Genus</th><th>Species</th>"
              "<th>Name</th><th>Population</th><th>Active scripts</th></tr>\n";
    for (const MagicProfilerSnapshot::ClassifierRow& row :
         snapshot.classifier_rows) {
        std::string name = row.display_name;
        if (name.empty()) {
            name = brain::ResolveClassifierDisplayName(snapshot.classifier_names,
                                                        row.classifier);
        }
        output << "<tr><td>" << row.classifier.family << "</td><td>"
               << row.classifier.genus << "</td><td>"
               << row.classifier.species << "</td><td>"
               << display::EscapeXmlForProfilerReport(name)
               << "</td><td>" << row.population << "</td><td>"
               << row.active_script_count << "</td></tr>\n";
    }
    output << "</table>\n";
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

    window.show_message(kReportTitle, "Magic profiler report written.");
    return report_path;
}

}  // namespace creatures1::ui
