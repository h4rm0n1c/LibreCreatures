#pragma once

// The Lab Kit's control protocol and message names.  Plain C++, no Windows,
// so it is tested on its own (../tests/labkit_protocol_test.cpp).
//
// The lab reaches the kit through the named pipe \\.\pipe\c1-labkit, one
// request per connection, in message mode:
//     request:  <VERB>[ <argument>]     (the argument is the rest, raw bytes)
//     reply:    OK[ <data>] | ERR <reason>
// Verbs:
//     PING                 -> OK pong
//     STATUS               -> OK tool=.. connected=.. messages=.. answer=..
//                             hang=<ms>x<count> close_on_8=.. paused=..
//     ANSWER true|false    the VT_BOOL answer to every later Communicate
//     HANG <ms> [<count>]  the next <count> (default 1) Communicate calls
//                          wait <ms> before answering
//     DIE next|now         end the kit's process: inside the next
//                          Communicate call, or right after replying
//     CLOSEON8 0|1         whether control state 8 closes the kit (1, as
//                          every kit does)
//     CAOS <script>        run on the kit's own SFC.OLE connection as a
//                          query; the reply is the game's, raw
//     SCHEDULE <script>    queue on the game's scheduler through SFC.OLE
//     QUIT                 "app: quit <tool id>", as a kit's Close does

#include <cstdint>
#include <cstdio>
#include <string>

namespace labkit {

constexpr char kPipeName[] = "\\\\.\\pipe\\c1-labkit";
constexpr char kLogName[] = "Lab Kit.log";

struct Request {
    std::string verb;
    std::string argument;
};

inline Request parse_request(const std::string& text) {
    Request request;
    const std::size_t space = text.find(' ');
    request.verb = text.substr(0, space);
    if (space != std::string::npos) {
        request.argument = text.substr(space + 1);
    }
    while (!request.verb.empty() &&
           (request.verb.back() == '\r' || request.verb.back() == '\n')) {
        request.verb.pop_back();
    }
    return request;
}

// "500" or "500 3" -> milliseconds and count; false if malformed.
inline bool parse_hang(const std::string& argument, unsigned& ms,
                       unsigned& count) {
    unsigned long first = 0;
    unsigned long second = 1;
    char extra = 0;
    const int read = std::sscanf(argument.c_str(), "%lu %lu %c", &first,
                                 &second, &extra);
    if (read < 1 || read > 2 || second == 0) {
        return false;
    }
    ms = static_cast<unsigned>(first);
    count = static_cast<unsigned>(second);
    return true;
}

// What a Communicate message means, for the log.
inline std::string describe_message(std::uint8_t kind, std::uint8_t code,
                                    std::uint16_t aux, std::int32_t payload) {
    char text[96];
    if (kind == 1 && code == 3) {
        std::snprintf(text, sizeof(text), "your id is %u", aux);
    } else if (kind == 1 && code == 4) {
        std::snprintf(text, sizeof(text), "integer %ld",
                      static_cast<long>(payload));
    } else if (kind == 2) {
        const char* name = code == 6   ? "selected creature changed"
                           : code == 7 ? "creature named"
                           : code == 8 ? "close"
                           : code == 9 ? "pause toggled"
                                       : "control state";
        std::snprintf(text, sizeof(text), "%s (state %u)", name, code);
    } else {
        std::snprintf(text, sizeof(text), "kind %u code %u", kind, code);
    }
    return text;
}

} // namespace labkit
