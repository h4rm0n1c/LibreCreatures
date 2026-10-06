#pragma once

// Crash reports: what a kit writes when it fails, as text a player can read
// and pass on.  Header-only and portable; the shell (kit_crash.cpp) gathers
// the facts on Windows and shows the report.
//
// A report file is the summary line, a blank line, then the details.  It
// sits beside a minidump of the same name (.dmp) in the crash folder.

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace c1kit {

// What kind of failure it was.
enum class CrashKind {
    exception,          // a hardware or SEH exception nothing handled
    cpp_exception,      // a C++ exception nothing caught
    terminate,          // std::terminate without an exception in flight
    invalid_parameter,  // a C runtime function was given bad arguments
    pure_call,          // a pure virtual function was called
    abort,              // abort() was called
    carried_on,         // an error the kit reported and then carried on from
};

// Exception codes the kit itself raises for the kinds above, so every report
// carries a code.  0xE0 sets the "customer" bit; "C1K" plus a number.
constexpr std::uint32_t kCrashCodeTerminate = 0xE0C14B01;
constexpr std::uint32_t kCrashCodeInvalidParameter = 0xE0C14B02;
constexpr std::uint32_t kCrashCodePureCall = 0xE0C14B03;
constexpr std::uint32_t kCrashCodeAbort = 0xE0C14B04;
constexpr std::uint32_t kCrashCodeCarriedOn = 0xE0C14B05;
constexpr std::uint32_t kMsvcCppExceptionCode = 0xE06D7363;  // "msc"

struct CrashFrame {
    std::uint32_t address = 0;
    std::string module;          // file name only; empty if unknown
    std::uint32_t offset = 0;    // from the module's base
    std::string symbol;          // empty if unknown
    std::uint32_t symbol_offset = 0;
};

struct CrashModule {
    std::string name;
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::uint32_t timestamp = 0;  // the PE header's link time
};

struct CrashInfo {
    CrashKind kind = CrashKind::exception;
    std::string kit;              // e.g. "Hatchery"
    std::string kit_file;         // the exe's full path
    std::uint32_t kit_timestamp = 0;
    std::string system;           // Windows (and Wine) version
    std::string time;             // when, local time
    std::uint32_t code = 0;
    std::uint32_t address = 0;    // where it happened
    std::string module;           // the module holding `address`
    std::uint32_t module_offset = 0;
    // An access violation: 0 read, 1 write, 8 execute; and the address.
    bool has_access = false;
    std::uint32_t access_kind = 0;
    std::uint32_t access_address = 0;
    std::string cpp_type;         // a C++ exception's type, readable
    std::string message;          // what() or another explanation
    std::uint32_t thread_id = 0;
    std::vector<std::pair<std::string, std::uint32_t>> registers;
    std::vector<CrashFrame> frames;
    // Return addresses found by reading the stack itself: code just after a
    // call.  Kept apart from `frames`, as some may be stale.
    std::vector<CrashFrame> stack_scan;
    std::vector<CrashModule> modules;
    std::string dump_file;        // the minidump beside the report, if any
};

inline std::string hex32(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof text, "0x%08X", static_cast<unsigned>(value));
    return text;
}

inline const char* exception_code_name(std::uint32_t code) {
    switch (code) {
    case 0xC0000005: return "access violation";
    case 0xC0000006: return "in-page error";
    case 0xC000001D: return "illegal instruction";
    case 0xC0000025: return "non-continuable exception";
    case 0xC000008C: return "array bounds exceeded";
    case 0xC000008D: return "floating-point denormal operand";
    case 0xC000008E: return "floating-point divide by zero";
    case 0xC000008F: return "floating-point inexact result";
    case 0xC0000090: return "floating-point invalid operation";
    case 0xC0000091: return "floating-point overflow";
    case 0xC0000092: return "floating-point stack check";
    case 0xC0000093: return "floating-point underflow";
    case 0xC0000094: return "integer divide by zero";
    case 0xC0000095: return "integer overflow";
    case 0xC0000096: return "privileged instruction";
    case 0xC00000FD: return "stack overflow";
    case 0xC0000374: return "heap corruption";
    case 0xC0000409: return "stack buffer overrun";
    case 0xC0000417: return "invalid C runtime parameter";
    case 0x80000003: return "breakpoint";
    case 0x80000002: return "data misalignment";
    case kMsvcCppExceptionCode: return "C++ exception";
    case kCrashCodeTerminate: return "std::terminate";
    case kCrashCodeInvalidParameter: return "invalid C runtime parameter";
    case kCrashCodePureCall: return "pure virtual function call";
    case kCrashCodeAbort: return "abort";
    case kCrashCodeCarriedOn: return "error, carried on";
    default: return "unknown exception";
    }
}

// An MSVC type descriptor's decorated name, readable:
// ".?AVout_of_range@std@@" -> "std::out_of_range", ".?AUFoo@@" -> "Foo".
// Templates and other forms are returned as they are.
inline std::string readable_msvc_type(const std::string& decorated) {
    std::string name = decorated;
    if (name.rfind(".?AV", 0) == 0 || name.rfind(".?AU", 0) == 0 ||
        name.rfind(".?AW", 0) == 0) {
        name = name.substr(4);
    } else {
        return decorated;
    }
    if (name.size() < 2 || name.compare(name.size() - 2, 2, "@@") != 0 ||
        name.find('?') != std::string::npos) {
        return decorated;
    }
    name.resize(name.size() - 2);
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= name.size()) {
        const std::size_t at = name.find('@', start);
        parts.push_back(name.substr(start, at == std::string::npos ? std::string::npos
                                                                   : at - start));
        if (at == std::string::npos) {
            break;
        }
        start = at + 1;
    }
    std::string out;
    for (auto part = parts.rbegin(); part != parts.rend(); ++part) {
        if (part->empty()) {
            return decorated;
        }
        if (!out.empty()) {
            out += "::";
        }
        out += *part;
    }
    return out;
}

inline std::string where_text(const std::string& module, std::uint32_t offset,
                              std::uint32_t address) {
    return module.empty() ? hex32(address) : module + "+" + hex32(offset);
}

// One line for the top of the report and the dialog.
inline std::string crash_summary(const CrashInfo& info) {
    const std::string kit = info.kit.empty() ? "The kit" : info.kit;
    switch (info.kind) {
    case CrashKind::carried_on:
        return kit + " had an error. It carried on, but may not work correctly "
                     "until it is closed and opened again.";
    case CrashKind::cpp_exception:
        return kit + " stopped because of an error it did not handle" +
               (info.cpp_type.empty() ? std::string(".")
                                      : " (" + info.cpp_type + ").");
    case CrashKind::terminate:
    case CrashKind::invalid_parameter:
    case CrashKind::pure_call:
    case CrashKind::abort:
        return kit + " stopped because of an internal error (" +
               exception_code_name(info.code) + ").";
    case CrashKind::exception:
    default:
        return kit + " stopped because of an error (" +
               exception_code_name(info.code) + " at " +
               where_text(info.module, info.module_offset, info.address) + ").";
    }
}

inline std::string crash_details(const CrashInfo& info) {
    std::string out;
    const auto line = [&out](const std::string& label, const std::string& value) {
        out += label;
        out += ": ";
        out += value;
        out += '\n';
    };
    line("Kit", info.kit + (info.kit_file.empty() ? "" : " (" + info.kit_file + ")"));
    if (info.kit_timestamp != 0) {
        line("Build", hex32(info.kit_timestamp));
    }
    if (!info.system.empty()) {
        line("System", info.system);
    }
    if (!info.time.empty()) {
        line("Time", info.time);
    }
    line("Error", std::string(exception_code_name(info.code)) + " (" +
                      hex32(info.code) + ")");
    // Only a hardware fault's own address says where it went wrong; for the
    // rest it is where the kit or the runtime raised it (see the stack).
    if (info.kind == CrashKind::exception) {
        line("Where", where_text(info.module, info.module_offset, info.address));
    }
    if (info.has_access) {
        const char* verb = info.access_kind == 1   ? "writing"
                           : info.access_kind == 8 ? "running code at"
                                                   : "reading";
        line("Access", std::string(verb) + " " + hex32(info.access_address));
    }
    if (!info.cpp_type.empty()) {
        line("Type", info.cpp_type);
    }
    if (!info.message.empty()) {
        line("Message", info.message);
    }
    if (info.thread_id != 0) {
        line("Thread", std::to_string(info.thread_id));
    }
    if (!info.dump_file.empty()) {
        line("Dump", info.dump_file);
    }
    if (!info.registers.empty()) {
        out += "\nRegisters:\n";
        std::size_t column = 0;
        for (const auto& reg : info.registers) {
            out += "  " + reg.first + "=" + hex32(reg.second);
            if (++column % 4 == 0) {
                out += '\n';
            }
        }
        if (column % 4 != 0) {
            out += '\n';
        }
    }
    const auto frame_lines = [&out](const std::vector<CrashFrame>& frames) {
        for (const CrashFrame& frame : frames) {
            out += "  " + hex32(frame.address) + "  " +
                   where_text(frame.module, frame.offset, frame.address);
            if (!frame.symbol.empty()) {
                out += "  " + frame.symbol;
                if (frame.symbol_offset != 0) {
                    out += "+" + hex32(frame.symbol_offset);
                }
            }
            out += '\n';
        }
    };
    if (!info.frames.empty()) {
        out += "\nStack:\n";
        frame_lines(info.frames);
    }
    if (!info.stack_scan.empty()) {
        out += "\nReturn addresses on the stack (some may be stale):\n";
        frame_lines(info.stack_scan);
    }
    if (!info.modules.empty()) {
        out += "\nModules:\n";
        for (const CrashModule& module : info.modules) {
            out += "  " + hex32(module.base) + "-" + hex32(module.base + module.size) +
                   "  " + module.name;
            if (module.timestamp != 0) {
                out += "  (" + hex32(module.timestamp) + ")";
            }
            out += '\n';
        }
    }
    return out;
}

// The whole report file: the summary, a blank line, the details.
inline std::string crash_report_text(const CrashInfo& info) {
    return crash_summary(info) + "\n\n" + crash_details(info);
}

// A report file back as its summary and details.
inline void split_crash_report(const std::string& text, std::string& summary,
                               std::string& details) {
    std::string clean;
    clean.reserve(text.size());
    for (const char character : text) {
        if (character != '\r') {
            clean += character;
        }
    }
    const std::size_t gap = clean.find("\n\n");
    if (gap == std::string::npos) {
        summary = clean;
        details.clear();
        return;
    }
    summary = clean.substr(0, gap);
    details = clean.substr(gap + 2);
}

// The report's file name without extension: the kit and the time, with no
// characters Windows refuses in a file name.
inline std::string crash_file_stem(const std::string& kit, int year, int month,
                                   int day, int hour, int minute, int second,
                                   std::uint32_t process_id) {
    std::string name;
    for (const char character : kit) {
        const bool bad = character == '\\' || character == '/' || character == ':' ||
                         character == '*' || character == '?' || character == '"' ||
                         character == '<' || character == '>' || character == '|' ||
                         static_cast<unsigned char>(character) < 32;
        name += bad ? '_' : character;
    }
    char when[64];
    std::snprintf(when, sizeof when, " %04d-%02d-%02d %02d.%02d.%02d %u", year, month,
                  day, hour, minute, second, static_cast<unsigned>(process_id));
    return (name.empty() ? std::string("Kit") : name) + when;
}

} // namespace c1kit
