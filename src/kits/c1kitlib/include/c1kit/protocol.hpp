#pragma once

// The C1 kit protocol, as values.  Portable and header-only: nothing here
// touches Windows, and nothing crosses the c1kitlib.dll boundary.
//
// Reference: external/creaturesstructs/Creatures1/kit-protocol/README.md.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace c1kit {

// ---------------------------------------------------------------------------
// Game -> kit: Communicate(header, payload)
// ---------------------------------------------------------------------------

// header = aux:16 | code:8 | kind:8
struct KitMessage {
    std::uint8_t kind = 0;
    std::uint8_t code = 0;
    std::uint16_t aux = 0;
    std::int32_t payload = 0;
};

enum : std::uint8_t {
    kMessageKindIdentity = 1,  // code carries the meaning
    kMessageKindControl = 2,   // code is the broadcast control state
};

enum : std::uint8_t {
    kIdentityCodeYourIdIs = 3,  // tool slot rides in aux
    kIdentityCodeInteger = 4,   // one integer in the payload
};

// Control states seen by the 1996 kits (Observation
// HandleHotkeyCommand @ 0x004037f0): 8 closes the kit, 9 toggles the paused
// display.  The game sends 9 from its Pause handler.
enum : std::uint8_t {
    kControlStateClose = 8,
    kControlStatePause = 9,
};

inline KitMessage decode_communicate(std::int32_t header,
                                     std::int32_t payload) {
    const auto bits = static_cast<std::uint32_t>(header);
    KitMessage message;
    message.kind = static_cast<std::uint8_t>(bits & 0xffu);
    message.code = static_cast<std::uint8_t>((bits >> 8) & 0xffu);
    message.aux = static_cast<std::uint16_t>(bits >> 16);
    message.payload = payload;
    return message;
}

// ---------------------------------------------------------------------------
// Launch: "Kit.exe /Embedding /ToolID=6 /ProgID=Overview.OLE"
// ---------------------------------------------------------------------------

struct LaunchArgs {
    bool embedding = false;   // launched by the game (or COM) as a server
    bool automation = false;  // /Automation
    int tool_id = -1;         // -1 when absent
    std::string prog_id;
};

namespace detail {
inline bool switch_equals(std::string_view token, std::string_view name) {
    if (token.size() != name.size()) {
        return false;
    }
    for (std::size_t i = 0; i < token.size(); ++i) {
        const char a = token[i];
        const char b = name[i];
        const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
        const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
        if (la != lb) {
            return false;
        }
    }
    return true;
}
} // namespace detail

// Named switches, not positions: reading argv[1] positionally serves slot 0
// while the game talks to the real slot.  Both '/' and '-' prefixes are
// accepted, as MFC's CCommandLineInfo does.
inline LaunchArgs parse_launch_args(std::string_view command_line) {
    LaunchArgs args;
    std::size_t pos = 0;
    while (pos < command_line.size()) {
        while (pos < command_line.size() && command_line[pos] == ' ') {
            ++pos;
        }
        std::size_t end = pos;
        bool quoted = false;
        while (end < command_line.size() &&
               (quoted || command_line[end] != ' ')) {
            if (command_line[end] == '"') {
                quoted = !quoted;
            }
            ++end;
        }
        std::string_view token = command_line.substr(pos, end - pos);
        pos = end;
        if (token.empty() || (token[0] != '/' && token[0] != '-')) {
            continue;
        }
        token.remove_prefix(1);
        const std::size_t equals = token.find('=');
        const std::string_view name = token.substr(0, equals);
        const std::string_view value =
            equals == std::string_view::npos ? std::string_view{}
                                             : token.substr(equals + 1);
        if (detail::switch_equals(name, "embedding")) {
            args.embedding = true;
        } else if (detail::switch_equals(name, "automation")) {
            args.automation = true;
        } else if (detail::switch_equals(name, "toolid")) {
            args.tool_id = std::atoi(std::string(value).c_str());
        } else if (detail::switch_equals(name, "progid")) {
            args.prog_id.assign(value);
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// Kit -> game: SFC.OLE macro conversation
// ---------------------------------------------------------------------------

// SFC.OLE dispatch map order; DISPIDs are assigned by position.
enum SfcDispatchId : long {
    kSfcRequestMacro = 1,
    kSfcExecuteMacro = 2,
    kSfcCreateMacro = 3,
    kSfcDestroyMacro = 4,
    kSfcLoadMacro = 5,
};

// CreateMacro execution modes (CMacroHolder callback table).
enum MacroMode : short {
    kMacroModeSchedule = 0,  // queue on the world scheduler; no output
    kMacroModeQuery = 1,     // run to completion now and publish output
    kMacroModeBrainReport = 2,
};

// ---------------------------------------------------------------------------
// Replies
// ---------------------------------------------------------------------------

// Consume one delimiter-terminated field from the front of `text`, as the
// kits' ExtractDelimitedField does (Observation @ 0x00405860): with no
// delimiter left, the whole remainder is the field and `text` becomes empty.
inline std::string take_field(std::string& text, char delimiter) {
    const std::size_t at = text.find(delimiter);
    if (at == std::string::npos) {
        std::string field = std::move(text);
        text.clear();
        return field;
    }
    std::string field = text.substr(0, at);
    text.erase(0, at + 1);
    return field;
}

// `dde: getb ovvd`: one record per creature in the game's selection, records
// joined with '&', fields joined with '|' (Creature::FormatStatusForExternal
// Query @ 0x0040e520 writes ten).
enum OverviewField : std::size_t {
    kOverviewName = 0,
    kOverviewMoniker = 1,
    kOverviewSex = 2,        // "1" male, "2" female
    kOverviewAge = 3,        // "h:mm"
    kOverviewPregnancy = 4,  // "N/A" (male), "No", or gestation stage
    kOverviewLifeForce = 5,  // "NN%" or the "Dead" string
    kOverviewMedical = 6,
    kOverviewRoom = 7,
};

struct OverviewRecord {
    std::string fields[8];
    const std::string& operator[](OverviewField field) const {
        return fields[field];
    }
};

// Parses at most `max_records` records, reading the first eight fields of
// each, exactly as Observation's LoadOverviewData @ 0x004055f0 does: it stops
// at an empty remainder or after max_records (13 in the 1996 kit).
inline std::vector<OverviewRecord> parse_overview(std::string reply,
                                                  std::size_t max_records) {
    std::vector<OverviewRecord> records;
    while (!reply.empty() && records.size() < max_records) {
        std::string record_text = take_field(reply, '&');
        OverviewRecord record;
        for (std::string& field : record.fields) {
            field = take_field(record_text, '|');
        }
        records.push_back(std::move(record));
    }
    return records;
}

// "57%" -> 57, as the kit's atoi of the field minus its last character.
inline int life_force_percent(const std::string& field) {
    if (field.empty()) {
        return 0;
    }
    return std::atoi(field.substr(0, field.size() - 1).c_str());
}

} // namespace c1kit
