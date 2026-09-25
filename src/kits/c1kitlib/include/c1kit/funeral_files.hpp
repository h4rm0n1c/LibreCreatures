#pragma once

// The Funeral Kit's graves: one record per creature whose death the game
// reported, kept in the world's folder.  Header-only and portable, so it can
// be tested without Windows.
//
// The 1996 kit kept "The Graveyard" (the dead creatures' Register records)
// and "Album" (a uint32 count, that many uint32s, then that many CStrings),
// but only ever wrote them when it shut down, and "The Graveyard" with a
// count of 0.  This build keeps its own file, "Funeral Kit Graves", and
// leaves those two alone: an MFC archive of a uint16 count, then ten
// CStrings per grave, the same layout as "The Register".

#include "c1kit/owner_files.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace c1kit {

enum GraveField : std::size_t {
    kGraveMoniker = 0,
    kGraveCreatureName = 1,
    kGraveFatherMoniker = 2,
    kGraveMotherMoniker = 3,
    kGraveBirthTime = 4,    // "21:16 Sep 15 2026" (%H:%M %b %d %Y)
    kGraveBirthplace = 5,
    kGraveDeathTime = 6,    // when the kit was told, in the same form
    kGraveEpitaph = 7,
    kGraveHeadstone = 8,    // "1" once a headstone has been made
    kGraveReserved = 9,
    kGraveFieldCount = 10,
};

struct Grave {
    std::string fields[kGraveFieldCount];

    std::string& operator[](GraveField field) { return fields[field]; }
    const std::string& operator[](GraveField field) const { return fields[field]; }
    bool has_headstone() const { return fields[kGraveHeadstone] == "1"; }
};

constexpr char kGravesFileName[] = "Funeral Kit Graves";

// The game reports a death as an integer message whose payload is the
// creature's moniker id; its moniker string (as the Register and the album
// file names spell it) is that id in lower-case hex.
inline std::string moniker_from_id(std::uint32_t id) {
    char text[16];
    std::snprintf(text, sizeof(text), "%lx", static_cast<unsigned long>(id));
    return text;
}

// A grave for a registered creature: the first six fields of its Register
// record, and when it died.
inline Grave grave_from_register(const OwnerRecord& record,
                                 const std::string& death_time) {
    Grave grave;
    for (std::size_t i = kOwnerMoniker; i <= kOwnerBirthplace; ++i) {
        grave.fields[i] = record.fields[i];
    }
    grave[kGraveDeathTime] = death_time;
    return grave;
}

inline bool parse_graves(const std::vector<std::uint8_t>& bytes,
                         std::vector<Grave>& out) {
    out.clear();
    if (bytes.empty()) {
        return true;
    }
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        Grave grave;
        for (std::string& field : grave.fields) {
            field = reader.cstring();
        }
        if (reader.ok()) {
            out.push_back(std::move(grave));
        }
    }
    return reader.ok();
}

inline std::vector<std::uint8_t> serialize_graves(const std::vector<Grave>& graves) {
    ArchiveWriter writer;
    writer.u16(static_cast<std::uint16_t>(graves.size()));
    for (const Grave& grave : graves) {
        for (const std::string& field : grave.fields) {
            writer.cstring(field);
        }
    }
    return writer.bytes();
}

inline Grave* find_grave(std::vector<Grave>& graves, const std::string& moniker) {
    for (Grave& grave : graves) {
        if (grave[kGraveMoniker] == moniker) {
            return &grave;
        }
    }
    return nullptr;
}

// "21:16 Sep 15 to 00:56 Sep 26 2026" (FormatPhotoDateRange @ 0x00409e10).
// The 1996 kit always dropped the birth year (the last five characters).
// Fix (Funeral Kit bug 5): it is kept when the creature was born in another
// year.
inline std::string format_life_span(const std::string& born,
                                    const std::string& died) {
    std::string start = born;
    const auto year = [](const std::string& time) {
        return time.size() >= 5 ? time.substr(time.size() - 5) : std::string();
    };
    if (!start.empty() && year(start) == year(died)) {
        start.erase(start.size() - 5);
    }
    while (!start.empty() && start.back() == ' ') {
        start.pop_back();
    }
    if (start.empty()) {
        return died;
    }
    if (died.empty()) {
        return start;
    }
    return start + " to " + died;
}

} // namespace c1kit
