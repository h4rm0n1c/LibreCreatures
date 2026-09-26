#pragma once

// COB files (Creatures Object Bundles), as the Object Injector reads them,
// and what can be learned from their scripts.  Header-only and portable.
//
// A .cob (and a removal .rcb) is an MFC archive (LoadInjectorCobFile
// @ 0x00408370): a uint16 count, then per record
//
//   uint16  how many are left (255 and over: unlimited)
//   int32   expiry month, day, year (all 0: never)
//   uint16  install scripts, inject scripts
//   uint16  the next inject script to run (per-activation mode)
//   uint16  mode: 0 one inject script per injection, else all of them
//   CString install scripts   (`scrp f g s e,...`: event scripts)
//   CString inject scripts    (`inst,new: simp ...`: make the object)
//   sprite  int32 width, int32 height, uint16 row stride, then pixels
//   CString name
//   CString description
//
// An .rcb's inject scripts remove what its COB added.

#include "c1kit/owner_files.hpp"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace c1kit {

constexpr int kUnlimitedCob = 255;

struct CobSprite {
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<std::uint8_t> pixels;  // palette indices, height rows of stride
};

struct Cob {
    int quantity = 0;
    int expiry_month = 0;
    int expiry_day = 0;
    int expiry_year = 0;
    int next_inject = 0;
    int mode = 0;
    std::vector<std::string> install_scripts;
    std::vector<std::string> inject_scripts;
    CobSprite sprite;
    std::string name;
    std::string description;

    bool unlimited() const { return quantity >= kUnlimitedCob; }
    bool has_expiry() const {
        return expiry_month != 0 || expiry_day != 0 || expiry_year != 0;
    }
};

inline bool parse_cob_file(const std::vector<std::uint8_t>& bytes, std::vector<Cob>& out) {
    out.clear();
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t r = 0; r < count && reader.ok(); ++r) {
        Cob cob;
        cob.quantity = reader.u16();
        cob.expiry_month = static_cast<std::int32_t>(reader.u32());
        cob.expiry_day = static_cast<std::int32_t>(reader.u32());
        cob.expiry_year = static_cast<std::int32_t>(reader.u32());
        const std::uint16_t installs = reader.u16();
        const std::uint16_t injects = reader.u16();
        cob.next_inject = reader.u16();
        cob.mode = reader.u16();
        for (std::uint16_t i = 0; i < installs && reader.ok(); ++i) {
            cob.install_scripts.push_back(reader.cstring());
        }
        for (std::uint16_t i = 0; i < injects && reader.ok(); ++i) {
            cob.inject_scripts.push_back(reader.cstring());
        }
        cob.sprite.width = static_cast<std::int32_t>(reader.u32());
        cob.sprite.height = static_cast<std::int32_t>(reader.u32());
        cob.sprite.stride = reader.u16();
        if (cob.sprite.width < 0 || cob.sprite.height < 0 || cob.sprite.width > 4096 ||
            cob.sprite.height > 4096) {
            return false;
        }
        if (cob.sprite.stride < cob.sprite.width) {
            cob.sprite.stride = cob.sprite.width;
        }
        reader.bytes(static_cast<std::size_t>(cob.sprite.stride) *
                         static_cast<std::size_t>(cob.sprite.height),
                     cob.sprite.pixels);
        cob.name = reader.cstring();
        cob.description = reader.cstring();
        if (reader.ok()) {
            out.push_back(std::move(cob));
        }
    }
    return reader.ok() && !out.empty();
}

// CCobObject::IsExpired @ 0x00408100: never with no date; a year up to 99
// is 19xx; a COB is good through the whole of its expiry day.
inline bool cob_expired(const Cob& cob, int year, int month, int day) {
    if (!cob.has_expiry()) {
        return false;
    }
    int expiry_year = cob.expiry_year <= 99 ? 1900 + cob.expiry_year : cob.expiry_year;
    int expiry_month = cob.expiry_month < 1 ? 1 : cob.expiry_month > 12 ? 12 : cob.expiry_month;
    int expiry_day = cob.expiry_day < 1 ? 1 : cob.expiry_day > 31 ? 31 : cob.expiry_day;
    if (year != expiry_year) return year > expiry_year;
    if (month != expiry_month) return month > expiry_month;
    return day > expiry_day;
}

// ---------------------------------------------------------------------------
// Reading the scripts
// ---------------------------------------------------------------------------

// CAOS words: separated by spaces and commas; [bracketed text] is one word.
inline std::vector<std::string> caos_tokens(const std::string& script) {
    std::vector<std::string> tokens;
    std::string word;
    bool bracket = false;
    for (const char c : script) {
        if (bracket) {
            word.push_back(c);
            if (c == ']') bracket = false;
            continue;
        }
        if (c == '[') {
            bracket = true;
            word.push_back(c);
        } else if (c == ' ' || c == ',' || c == '\t' || c == '\r' || c == '\n') {
            if (!word.empty()) {
                tokens.push_back(word);
                word.clear();
            }
        } else {
            word.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (!word.empty()) tokens.push_back(word);
    return tokens;
}

inline bool caos_integer(const std::string& token, int& out) {
    if (token.empty()) return false;
    std::size_t at = (token[0] == '-' || token[0] == '+') ? 1 : 0;
    if (at == token.size()) return false;
    for (std::size_t i = at; i < token.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    }
    out = std::atoi(token.c_str());
    return true;
}

struct Classifier {
    int family = 0;
    int genus = 0;
    int species = 0;
    bool operator==(const Classifier& other) const {
        return family == other.family && genus == other.genus && species == other.species;
    }
};

// A script an install script puts in the scriptorium: `scrp f g s e`.
struct InstalledScript {
    Classifier classifier;
    int event = 0;
};

inline bool installed_script(const std::string& script, InstalledScript& out) {
    const std::vector<std::string> tokens = caos_tokens(script);
    if (tokens.size() < 5 || tokens[0] != "scrp") return false;
    return caos_integer(tokens[1], out.classifier.family) &&
           caos_integer(tokens[2], out.classifier.genus) &&
           caos_integer(tokens[3], out.classifier.species) && caos_integer(tokens[4], out.event);
}

// The objects an inject script makes: its `setv clas <n>`, where n is
// family << 24 | genus << 16 | species << 8.
inline std::vector<Classifier> created_classifiers(const std::vector<std::string>& scripts) {
    std::vector<Classifier> found;
    for (const std::string& script : scripts) {
        const std::vector<std::string> tokens = caos_tokens(script);
        for (std::size_t i = 0; i + 2 < tokens.size(); ++i) {
            int value = 0;
            if (tokens[i] == "setv" && tokens[i + 1] == "clas" && caos_integer(tokens[i + 2], value)) {
                const Classifier c{(value >> 24) & 0xff, (value >> 16) & 0xff, (value >> 8) & 0xff};
                bool seen = false;
                for (const Classifier& f : found) seen = seen || f == c;
                if (!seen) found.push_back(c);
            }
        }
    }
    return found;
}

// Chemicals the scripts change (CCobObject::CollectAffectedChemicalIds
// @ 0x00409e10): `chem <n> <amount>`, and the chemical/amount pairs of a
// stimulus (`stim writ <target> a b c d  c1 n1 c2 n2 c3 n3 c4 n4`, and
// `stim shou|sign|tact` without the target).  First seen first; 1-255.
inline std::vector<int> affected_chemicals(const Cob& cob) {
    std::vector<int> found;
    const auto add = [&found](int chemical) {
        if (chemical < 1 || chemical > 255) return;
        for (const int f : found) if (f == chemical) return;
        found.push_back(chemical);
    };
    std::vector<std::string> scripts = cob.install_scripts;
    scripts.insert(scripts.end(), cob.inject_scripts.begin(), cob.inject_scripts.end());
    for (const std::string& script : scripts) {
        const std::vector<std::string> t = caos_tokens(script);
        for (std::size_t i = 0; i < t.size(); ++i) {
            int value = 0;
            if (t[i] == "chem" && (i == 0 || t[i - 1] != "putv") && i + 1 < t.size() &&
                caos_integer(t[i + 1], value)) {
                add(value);
            } else if (t[i] == "stim" && i + 1 < t.size()) {
                std::size_t at = i + 2;
                if (t[i + 1] == "writ") ++at;  // the target
                at += 4;                        // four stimulus values
                for (int pair = 0; pair < 4 && at < t.size(); ++pair, at += 2) {
                    if (!caos_integer(t[at], value)) break;
                    add(value);
                }
            }
        }
    }
    return found;
}

// Whether injecting needs a creature selected: an inject script names
// `norn` (InjectSelectedCob @ 0x00405e60 then asks `dde: putv norn`).
inline bool needs_creature(const Cob& cob) {
    for (const std::string& script : cob.inject_scripts) {
        for (const std::string& token : caos_tokens(script)) {
            if (token == "norn") return true;
        }
    }
    return false;
}

// BuildInjectionSafetyWarnings @ 0x0040a080: the executable's strings, and
// one about an inject list that makes nothing.
inline std::vector<std::string> cob_warnings(const Cob& cob, bool expired) {
    std::vector<std::string> warnings;
    bool kills = false;
    bool fires = false;
    std::vector<std::string> scripts = cob.install_scripts;
    scripts.insert(scripts.end(), cob.inject_scripts.begin(), cob.inject_scripts.end());
    for (const std::string& script : scripts) {
        const std::vector<std::string> t = caos_tokens(script);
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (t[i] == "kill" && i + 1 < t.size() && t[i + 1] == "norn") kills = true;
            if (t[i] == "fire" || t[i] == "trig") fires = true;
        }
    }
    if (kills) warnings.push_back("DANGER - kills the selected creature (kill norn)");
    if (fires) warnings.push_back("Directly fires creature brain neurons");
    if (expired) {
        warnings.push_back("This agent is past its use-by date (" + std::to_string(cob.expiry_day) +
                           "/" + std::to_string(cob.expiry_month) + "/" +
                           std::to_string(cob.expiry_year) + ")");
    }
    if (cob.inject_scripts.empty()) {
        warnings.push_back("Makes no object: it only installs scripts");
    }
    return warnings;
}

// A removal for a COB with no .rcb (BuildCobInjectionScript @ 0x0040a4c0):
// kill every object it makes, then take out every script it installs.
// Empty when it makes and installs nothing.
inline std::string generated_removal(const Cob& cob) {
    std::string script;
    for (const Classifier& c : created_classifiers(cob.inject_scripts)) {
        script += "enum " + std::to_string(c.family) + " " + std::to_string(c.genus) + " " +
                  std::to_string(c.species) + ",kill targ,next,";
    }
    for (const std::string& install : cob.install_scripts) {
        InstalledScript s;
        if (installed_script(install, s)) {
            script += "scrx " + std::to_string(s.classifier.family) + " " +
                      std::to_string(s.classifier.genus) + " " +
                      std::to_string(s.classifier.species) + " " + std::to_string(s.event) + ",";
        }
    }
    return script.empty() ? std::string() : "inst," + script + "endm";
}

// InjectorEventCodeToName @ 0x00408b90 (decoded from its switch table).
inline std::string event_name(int event) {
    static const char* const kNames[73] = {
        "Deactivate", "Activate 1", "Activate 2", "Hit", "Pickup", "Drop", "Collision",
        "Enter Scope", "Leave Scope", "Timer", nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, "Watch (Extraspective)", "Activate 1 (Extraspective)",
        "Activate 2 (Extraspective)", "Deactivate (Extraspective)", "Seek (Extraspective)",
        "Avoid (Extraspective)", "Pickup (Extraspective)", "Drop (Extraspective)",
        "Need (Extraspective)", "Rest (Extraspective)", "Walk West (Extraspective)",
        "Walk East (Extraspective)", nullptr, nullptr, nullptr, nullptr,
        "Quiescent (Introspective)", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        "Drop (Introspective)", "Need (Introspective)", "Rest (Introspective)",
        "Walk West (Introspective)", "Walk East (Introspective)", nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, "Pointer Activate 1", "Pointer Activate 2",
        "Pointer Deactivate", "Pointer Pickup", "Pointer Drop", nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Involuntary 0", "Involuntary 1",
        "Involuntary 2", "Involuntary 3", "Involuntary 4", "Involuntary 5", "Involuntary 6",
        "Involuntary 7", "Die"};
    if (event >= 0 && event < 73 && kNames[event] != nullptr) return kNames[event];
    return "Event " + std::to_string(event);
}

inline std::string family_name(int family) {
    switch (family) {
    case 1: return "Scenery";
    case 2: return "Simple Object";
    case 3: return "Compound Object";
    case 4: return "Creature";
    default: return "Family " + std::to_string(family);
    }
}

// ClassifierNames.txt, in the game's folder: "family|genus|species|name|"
// lines (empty fields for a whole family or genus).
struct ClassifierName {
    int family = -1;
    int genus = -1;
    int species = -1;
    std::string name;
};

inline std::vector<ClassifierName> parse_classifier_names(const std::string& text) {
    std::vector<ClassifierName> names;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        std::vector<std::string> fields;
        std::size_t from = 0;
        while (true) {
            const std::size_t bar = line.find('|', from);
            if (bar == std::string::npos) break;
            fields.push_back(line.substr(from, bar - from));
            from = bar + 1;
        }
        if (fields.size() < 4) continue;
        ClassifierName entry;
        entry.family = fields[0].empty() ? -1 : std::atoi(fields[0].c_str());
        entry.genus = fields[1].empty() ? -1 : std::atoi(fields[1].c_str());
        entry.species = fields[2].empty() ? -1 : std::atoi(fields[2].c_str());
        entry.name = fields[3];
        if (entry.family >= 0) names.push_back(entry);
    }
    return names;
}

// The most specific name known for a classifier ("" if none).
inline std::string classifier_name(const std::vector<ClassifierName>& names, const Classifier& c) {
    std::string best;
    int best_depth = -1;
    for (const ClassifierName& n : names) {
        if (n.family != c.family) continue;
        int depth = 1;
        if (n.genus >= 0) {
            if (n.genus != c.genus) continue;
            depth = 2;
            if (n.species >= 0) {
                if (n.species != c.species) continue;
                depth = 3;
            }
        } else if (n.species >= 0) {
            continue;
        }
        if (depth > best_depth) {
            best_depth = depth;
            best = n.name;
        }
    }
    return best;
}

} // namespace c1kit
