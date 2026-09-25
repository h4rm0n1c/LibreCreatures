#pragma once

// The Science Kit's data files, in the game's installation folder.
// Header-only and portable, so it can be tested without Windows.
//
//   allchemicals.str  uint16 count (256), then that many CStrings: the name
//                     of every chemical, by number.
//   themes.str        uint16 count, then per theme a CString name and a
//                     uint32 holding four chemical numbers, one per byte.
//   decision.str      uint16 count, then the decision lobe's action names.
//   injections.str    CStrings to the end of the file, no count: the
//                     medicines, which are chemicals 100 onwards.
//
// The 1996 kit could follow four chemicals, so its themes hold four.  This
// build follows more, and keeps its themes in a file of its own,
// "Science Kit Themes": uint16 count, then per theme a CString name, a
// uint8 count and that many chemical numbers.  It reads themes.str when
// that file does not exist yet, and never writes themes.str.

#include "c1kit/owner_files.hpp"
#include "c1kit/protocol.hpp"

#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

namespace c1kit {

constexpr char kAllChemicalsFileName[] = "allchemicals.str";
constexpr char kOriginalThemesFileName[] = "themes.str";
constexpr char kThemesFileName[] = "Science Kit Themes";
constexpr char kDecisionNamesFileName[] = "decision.str";
constexpr char kInjectionsFileName[] = "injections.str";

constexpr int kChemicalCount = 256;
// The medicines in injections.str are chemicals 100, 101, ...
// (CInjectPage::SendChemicalInjectionStep @ 0x0040aaf0 sends "chem
// <index + 100> <amount>").
constexpr int kFirstMedicineChemical = 100;

// A uint16 count, then that many CStrings.
inline bool parse_counted_strings(const std::vector<std::uint8_t>& bytes,
                                  std::vector<std::string>& out) {
    out.clear();
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        std::string text = reader.cstring();
        if (reader.ok()) {
            out.push_back(std::move(text));
        }
    }
    return reader.ok();
}

// CStrings to the end of the file (injections.str, chemicals.str).
inline bool parse_uncounted_strings(const std::vector<std::uint8_t>& bytes,
                                    std::vector<std::string>& out) {
    out.clear();
    ArchiveReader reader(bytes);
    while (!reader.at_end()) {
        std::string text = reader.cstring();
        if (!reader.ok()) {
            return false;
        }
        out.push_back(std::move(text));
    }
    return true;
}

// allchemicals.str; always answers kChemicalCount names (unnamed ones empty).
inline bool parse_chemical_names(const std::vector<std::uint8_t>& bytes,
                                 std::vector<std::string>& out) {
    const bool ok = parse_counted_strings(bytes, out);
    out.resize(kChemicalCount);
    return ok;
}

// Whether a chemical is worth offering: it has a real name.  Unused slots
// are named by their number ("73"), "<NONE>" (chemical 0) or
// "not_allocated<n>" (the three spare drives and their increases and
// decreases).  The 1996 kit hid those too (its test was "two digits or
// underscores"), and also "Waste Water" and anything named "tox"
// (CMonitorPage::BuildFilteredChemicalIndex @ 0x00404030); those two are
// real chemicals, so this build lists them.
inline bool chemical_is_named(const std::string& name) {
    if (name.empty() || name == "<NONE>" || name.rfind("not_allocated", 0) == 0) {
        return false;
    }
    for (const char c : name) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '_') {
            return true;
        }
    }
    return false;
}

// "Chemical 73" for an unnamed one.
inline std::string chemical_label(const std::vector<std::string>& names,
                                  int chemical) {
    if (chemical >= 0 && chemical < static_cast<int>(names.size()) &&
        chemical_is_named(names[static_cast<std::size_t>(chemical)])) {
        return names[static_cast<std::size_t>(chemical)];
    }
    return "Chemical " + std::to_string(chemical);
}

struct ChemicalTheme {
    std::string name;
    std::vector<std::uint8_t> chemicals;
};

// themes.str (CMonitorPage::LoadThemeNames @ 0x00404b00).  Chemical 0 in a
// slot means the slot is empty, and is dropped.
inline bool parse_original_themes(const std::vector<std::uint8_t>& bytes,
                                  std::vector<ChemicalTheme>& out) {
    out.clear();
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        ChemicalTheme theme;
        theme.name = reader.cstring();
        const std::uint32_t packed = reader.u32();
        for (int slot = 0; slot < 4; ++slot) {
            const auto chemical = static_cast<std::uint8_t>(packed >> (8 * slot));
            if (chemical != 0) {
                theme.chemicals.push_back(chemical);
            }
        }
        if (reader.ok()) {
            out.push_back(std::move(theme));
        }
    }
    return reader.ok();
}

inline bool parse_themes(const std::vector<std::uint8_t>& bytes,
                         std::vector<ChemicalTheme>& out) {
    out.clear();
    if (bytes.empty()) {
        return true;
    }
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        ChemicalTheme theme;
        theme.name = reader.cstring();
        const std::uint8_t chemicals = reader.u8();
        for (std::uint8_t c = 0; c < chemicals && reader.ok(); ++c) {
            theme.chemicals.push_back(reader.u8());
        }
        if (reader.ok()) {
            out.push_back(std::move(theme));
        }
    }
    return reader.ok();
}

inline std::vector<std::uint8_t> serialize_themes(
    const std::vector<ChemicalTheme>& themes) {
    ArchiveWriter writer;
    writer.u16(static_cast<std::uint16_t>(themes.size()));
    for (const ChemicalTheme& theme : themes) {
        writer.cstring(theme.name);
        const std::size_t count = theme.chemicals.size() < 255 ? theme.chemicals.size() : 255;
        writer.u8(static_cast<std::uint8_t>(count));
        for (std::size_t c = 0; c < count; ++c) {
            writer.u8(theme.chemicals[c]);
        }
    }
    return writer.bytes();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

// "inst,dde: putv chem 2,dde: putv chem 17,...,endm": each chemical's level,
// 0 to 255 (CMonitorPage::SubmitSelectedChemicalValues @ 0x004057d0 sends
// four; any number works).
inline std::string chemical_levels_query(const std::vector<int>& chemicals) {
    std::string script = "inst,";
    for (const int chemical : chemicals) {
        script += "dde: putv chem " + std::to_string(chemical) + ",";
    }
    script += "endm";
    return script;
}

// One value per chemical asked for; false if any is missing.
inline bool parse_values(std::string reply, std::size_t count,
                         std::vector<int>& out) {
    out.clear();
    for (std::size_t i = 0; i < count; ++i) {
        const std::string field = take_field(reply, '|');
        if (field.empty()) {
            return false;
        }
        out.push_back(std::atoi(field.c_str()));
    }
    return true;
}

// "inst,chem 100 12,endm": add 12 of medicine 0 (Energy) to the creature
// (SendChemicalInjectionStep @ 0x0040aaf0; the 1996 slider ran 0..255 and
// the kit sent a eighth of it).
inline std::string injection_script(int chemical, int amount) {
    return "inst,chem " + std::to_string(chemical) + " " +
           std::to_string(amount) + ",endm";
}

} // namespace c1kit
