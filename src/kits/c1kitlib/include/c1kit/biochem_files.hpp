#pragma once

// The Biochemistry Kit's files.  Header-only and portable.
//
//   biochem_saved.txt  beside the kit: one saved set of chemicals per line,
//                      "name=1,2,3" (CMonitorPage::LoadSavedChemicalGroups
//                      @ 0x004045b0 and SaveChemicalGroups @ 0x00405d90).
//                      Other lines are kept as they are.
//   allchemicals.str   the chemical names (science_files.hpp), which its
//                      Chemical Names page can rename and write back
//                      (CChemicalsPage::SaveChemicalNames @ 0x00409580).

#include "c1kit/owner_files.hpp"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace c1kit {

constexpr char kBiochemSavedFileName[] = "biochem_saved.txt";

struct SavedChemicalSet {
    std::string name;
    std::vector<int> chemicals;
};

inline std::string trim_text(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.pop_back();
    }
    std::size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
    return text.substr(start);
}

inline bool same_name(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// The file's lines, as they are.
inline std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        at = end + 1;
    }
    return lines;
}

inline bool parse_saved_line(const std::string& line, SavedChemicalSet& out) {
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) return false;
    out.name = trim_text(line.substr(0, equals));
    if (out.name.empty()) return false;
    out.chemicals.clear();
    std::string values = line.substr(equals + 1);
    std::size_t at = 0;
    while (at <= values.size()) {
        std::size_t comma = values.find(',', at);
        if (comma == std::string::npos) comma = values.size();
        const std::string field = trim_text(values.substr(at, comma - at));
        if (!field.empty()) {
            const int chemical = std::atoi(field.c_str());
            if (chemical >= 0 && chemical <= 255) out.chemicals.push_back(chemical);
        }
        at = comma + 1;
    }
    return true;
}

inline std::vector<SavedChemicalSet> parse_saved_sets(const std::string& text) {
    std::vector<SavedChemicalSet> sets;
    for (const std::string& line : split_lines(text)) {
        SavedChemicalSet set;
        if (parse_saved_line(line, set)) sets.push_back(set);
    }
    return sets;
}

inline std::string format_saved_line(const SavedChemicalSet& set) {
    std::string line = set.name + "=";
    for (std::size_t i = 0; i < set.chemicals.size(); ++i) {
        if (i > 0) line += ",";
        line += std::to_string(set.chemicals[i]);
    }
    return line;
}

// The file with `set` put in place of the line of the same name (any case),
// or added at the end; with `remove`, that line taken out.  Other lines are
// left alone.
inline std::string update_saved_sets(const std::string& text, const SavedChemicalSet& set,
                                     bool remove = false) {
    std::string out;
    bool placed = false;
    for (const std::string& line : split_lines(text)) {
        SavedChemicalSet existing;
        if (parse_saved_line(line, existing) && same_name(existing.name, set.name)) {
            if (!remove && !placed) {
                out += format_saved_line(set) + "\r\n";
                placed = true;
            }
            continue;
        }
        out += line + "\r\n";
    }
    if (!remove && !placed) out += format_saved_line(set) + "\r\n";
    return out;
}

// allchemicals.str: a uint16 count, then the names.
inline std::vector<std::uint8_t> serialize_chemical_names(const std::vector<std::string>& names) {
    ArchiveWriter writer;
    writer.u16(static_cast<std::uint16_t>(names.size()));
    for (const std::string& name : names) writer.cstring(name);
    return writer.bytes();
}

} // namespace c1kit
