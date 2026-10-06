// Crash reporting for every kit (not in the 1996 kits).
//
// install_crash_handlers() catches what would otherwise end a kit with no
// word: an exception nothing handled (an access violation, a C++ exception
// thrown out of a window procedure), std::terminate, a bad C runtime
// argument, a pure virtual call and abort().  The crashed kit only writes:
// on a fresh thread (so a stack overflow still has stack) it saves a
// minidump and a text report under %LOCALAPPDATA%\LibreCreatures\Crash
// Reports, starts a new copy of itself with /CrashReport=<report> to show
// it, and ends.  Nothing is drawn by the process that crashed.
//
// An error MFC catches in a message handler is reported in the kit itself
// (report_carried_on_error), which then carries on as MFC does.

#include "c1kitshell/kit_shell.hpp"

#include "c1kit/crash_report.hpp"

#include <dbghelp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace c1kitshell {
namespace {

using c1kit::CrashInfo;
using c1kit::CrashKind;

// Filled once at start-up, so the crash path needs no lookups.
char g_exe_path[MAX_PATH] = {};
char g_kit_name[MAX_PATH] = {};
volatile LONG g_crashing = 0;
volatile DWORD g_writer_thread_id = 0;
bool g_test_writer_crash = false;  // C1KIT_CRASH_TEST=reporter
bool g_carried_on_reported = false;

std::string file_name_of(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::uint32_t pe_timestamp(HMODULE module) {
    const auto* base = reinterpret_cast<const unsigned char*>(module);
    if (base == nullptr) {
        return 0;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt->FileHeader.TimeDateStamp : 0;
}

// The module holding `address`: its file name and base, or false.
bool module_at(std::uint32_t address, std::string& name, std::uint32_t& base) {
    HMODULE module = nullptr;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(static_cast<std::uintptr_t>(address)),
                              &module)) {
        return false;
    }
    char path[MAX_PATH] = {};
    ::GetModuleFileNameA(module, path, MAX_PATH);
    name = file_name_of(path);
    base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    return true;
}

std::string system_text() {
    std::string text = "Windows";
    HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");
    if (ntdll == nullptr) {
        return text;
    }
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (auto get_version = reinterpret_cast<RtlGetVersionFn>(
            ::GetProcAddress(ntdll, "RtlGetVersion"))) {
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof version;
        if (get_version(&version) == 0) {
            text += " " + std::to_string(version.dwMajorVersion) + "." +
                    std::to_string(version.dwMinorVersion) + "." +
                    std::to_string(version.dwBuildNumber);
        }
    }
    using WineVersionFn = const char*(CDECL*)();
    if (auto wine_version = reinterpret_cast<WineVersionFn>(
            ::GetProcAddress(ntdll, "wine_get_version"))) {
        text += " (Wine ";
        text += wine_version();
        text += ")";
    }
    return text;
}

std::string local_time_text() {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    char text[64];
    std::snprintf(text, sizeof text, "%04u-%02u-%02u %02u:%02u:%02u", now.wYear,
                  now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    return text;
}

// The folder the reports go in, made if need be: under %LOCALAPPDATA%, or
// the temporary folder if that cannot be had.
std::string crash_folder() {
    char base[MAX_PATH] = {};
    std::string folder;
    if (SUCCEEDED(::SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base))) {
        folder = std::string(base) + "\\LibreCreatures";
        ::CreateDirectoryA(folder.c_str(), nullptr);
        folder += "\\Crash Reports";
        ::CreateDirectoryA(folder.c_str(), nullptr);
        if (::GetFileAttributesA(folder.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return folder;
        }
    }
    ::GetTempPathA(MAX_PATH, base);
    folder = base;
    if (!folder.empty() && folder.back() == '\\') {
        folder.pop_back();
    }
    return folder;
}

std::string report_stem() {
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    return crash_folder() + "\\" +
           c1kit::crash_file_stem(g_kit_name, now.wYear, now.wMonth, now.wDay,
                                  now.wHour, now.wMinute, now.wSecond,
                                  ::GetCurrentProcessId());
}

// --- C++ exceptions ---------------------------------------------------------
// The MSVC x86 throw information an 0xE06D7363 exception carries
// (ExceptionInformation: magic, the object, its ThrowInfo).  Read with no
// C++ objects in scope, so __try may guard it.

struct MsvcTypeDescriptor {
    const void* vftable;
    void* spare;
    char name[1];
};
struct MsvcPmd {
    int mdisp;
    int pdisp;
    int vdisp;
};
struct MsvcCatchableType {
    unsigned properties;
    MsvcTypeDescriptor* type;
    MsvcPmd this_displacement;
    int size;
    void* copy_function;
};
struct MsvcCatchableTypeArray {
    int count;
    MsvcCatchableType* types[1];
};
struct MsvcThrowInfo {
    unsigned attributes;
    void* unwind;
    void* forward_compat;
    MsvcCatchableTypeArray* catchable;
};

void copy_text(char* out, std::size_t size, const char* text) {
    std::size_t at = 0;
    for (; text != nullptr && text[at] != '\0' && at + 1 < size; ++at) {
        out[at] = text[at];
    }
    out[at] = '\0';
}

// The thrown type's decorated name, and what() if it is a std::exception.
void read_cpp_exception(const EXCEPTION_RECORD* record, char* type, std::size_t type_size,
                        char* what, std::size_t what_size) {
    type[0] = '\0';
    what[0] = '\0';
    if (record->ExceptionCode != c1kit::kMsvcCppExceptionCode ||
        record->NumberParameters < 3) {
        return;
    }
    __try {
        const auto* object = reinterpret_cast<const char*>(record->ExceptionInformation[1]);
        const auto* info =
            reinterpret_cast<const MsvcThrowInfo*>(record->ExceptionInformation[2]);
        if (info == nullptr || info->catchable == nullptr) {
            return;
        }
        const MsvcCatchableTypeArray* types = info->catchable;
        if (types->count > 0 && types->types[0] != nullptr &&
            types->types[0]->type != nullptr) {
            copy_text(type, type_size, types->types[0]->type->name);
        }
        for (int index = 0; index < types->count && object != nullptr; ++index) {
            const MsvcCatchableType* entry = types->types[index];
            if (entry != nullptr && entry->type != nullptr &&
                std::strcmp(entry->type->name, ".?AVexception@std@@") == 0) {
                const auto* exception = reinterpret_cast<const std::exception*>(
                    object + entry->this_displacement.mdisp);
                copy_text(what, what_size, exception->what());
                break;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// --- dbghelp (loaded when needed; not linked) -------------------------------

struct DbgHelp {
    HMODULE module = nullptr;
    decltype(&::SymInitialize) sym_initialize = nullptr;
    decltype(&::SymSetOptions) sym_set_options = nullptr;
    decltype(&::SymCleanup) sym_cleanup = nullptr;
    decltype(&::StackWalk64) stack_walk = nullptr;
    decltype(&::SymFunctionTableAccess64) function_table = nullptr;
    decltype(&::SymGetModuleBase64) module_base = nullptr;
    decltype(&::SymFromAddr) from_address = nullptr;
    decltype(&::MiniDumpWriteDump) write_dump = nullptr;

    DbgHelp() {
        module = ::LoadLibraryA("dbghelp.dll");
        if (module == nullptr) {
            return;
        }
        const auto get = [this](auto& function, const char* name) {
            function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(
                ::GetProcAddress(module, name));
        };
        get(sym_initialize, "SymInitialize");
        get(sym_set_options, "SymSetOptions");
        get(sym_cleanup, "SymCleanup");
        get(stack_walk, "StackWalk64");
        get(function_table, "SymFunctionTableAccess64");
        get(module_base, "SymGetModuleBase64");
        get(from_address, "SymFromAddr");
        get(write_dump, "MiniDumpWriteDump");
    }
};

void walk_stack(DbgHelp& help, const CONTEXT& context, DWORD thread_id, CrashInfo& info) {
    if (help.sym_initialize == nullptr || help.stack_walk == nullptr ||
        help.function_table == nullptr || help.module_base == nullptr) {
        return;
    }
    HANDLE process = ::GetCurrentProcess();
    if (help.sym_set_options != nullptr) {
        help.sym_set_options(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
    }
    const bool symbols = help.sym_initialize(process, nullptr, TRUE) != FALSE;
    HANDLE thread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, thread_id);
    CONTEXT walk = context;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = walk.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = walk.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = walk.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    for (int depth = 0; depth < 64; ++depth) {
        if (!help.stack_walk(IMAGE_FILE_MACHINE_I386, process,
                             thread != nullptr ? thread : ::GetCurrentThread(), &frame,
                             &walk, nullptr, help.function_table, help.module_base,
                             nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        c1kit::CrashFrame out;
        out.address = static_cast<std::uint32_t>(frame.AddrPC.Offset);
        std::uint32_t base = 0;
        if (!module_at(out.address, out.module, base)) {
            break;  // lost the thread: an address in no module
        }
        out.offset = out.address - base;
        if (symbols && help.from_address != nullptr) {
            alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256] = {};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            if (help.from_address(process, frame.AddrPC.Offset, &displacement, symbol)) {
                out.symbol = symbol->Name;
                out.symbol_offset = static_cast<std::uint32_t>(displacement);
            }
        }
        info.frames.push_back(out);
    }
    if (thread != nullptr) {
        ::CloseHandle(thread);
    }
    if (symbols && help.sym_cleanup != nullptr) {
        help.sym_cleanup(process);
    }
}

// Whether the bytes before `address` end in a call: E8 rel32, or FF /2
// (an indirect call) of two, three, six or seven bytes.
bool follows_call(std::uint32_t address) {
    const auto* code = reinterpret_cast<const unsigned char*>(
        static_cast<std::uintptr_t>(address));
    if (::IsBadReadPtr(code - 7, 7)) {
        return false;
    }
    if (code[-5] == 0xE8) {
        return true;
    }
    for (const int length : {2, 3, 6, 7}) {
        if (code[-length] == 0xFF && (code[-length + 1] & 0x38) == 0x10) {
            return true;
        }
    }
    return false;
}

bool is_code(std::uint32_t address) {
    MEMORY_BASIC_INFORMATION memory{};
    if (::VirtualQuery(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), &memory,
                       sizeof memory) == 0 ||
        memory.State != MEM_COMMIT) {
        return false;
    }
    return (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                              PAGE_EXECUTE_WRITECOPY)) != 0;
}

// Return addresses read straight off the stack from `esp` up: what the walk
// cannot follow through code built without frame pointers.
void scan_stack(std::uint32_t esp, CrashInfo& info) {
    MEMORY_BASIC_INFORMATION memory{};
    if (esp == 0 ||
        ::VirtualQuery(reinterpret_cast<void*>(static_cast<std::uintptr_t>(esp)), &memory,
                       sizeof memory) == 0 ||
        memory.State != MEM_COMMIT) {
        return;
    }
    const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) +
                               memory.RegionSize;
    const std::uintptr_t limit = (std::min)(end, static_cast<std::uintptr_t>(esp) + 0x10000);
    for (std::uintptr_t at = esp & ~std::uintptr_t{3}; at + 4 <= limit; at += 4) {
        const std::uint32_t value = *reinterpret_cast<const std::uint32_t*>(at);
        if (value < 0x10000 || !is_code(value) || !follows_call(value)) {
            continue;
        }
        c1kit::CrashFrame frame;
        frame.address = value;
        std::uint32_t base = 0;
        if (!module_at(value, frame.module, base)) {
            continue;
        }
        frame.offset = value - base;
        info.stack_scan.push_back(frame);
        if (info.stack_scan.size() >= 40) {
            break;
        }
    }
}

void list_modules(CrashInfo& info) {
    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ::GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    MODULEENTRY32 entry{};
    entry.dwSize = sizeof entry;
    for (BOOL more = ::Module32First(snapshot, &entry); more;
         more = ::Module32Next(snapshot, &entry)) {
        c1kit::CrashModule module;
        module.name = CStringA(entry.szModule).GetString();
        module.base =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entry.modBaseAddr));
        module.size = entry.modBaseSize;
        module.timestamp = pe_timestamp(entry.hModule);
        info.modules.push_back(module);
    }
    ::CloseHandle(snapshot);
}

// Everything but the stack, the modules and the dump.
CrashInfo describe(CrashKind kind, const EXCEPTION_POINTERS& pointers, DWORD thread_id,
                   const std::string& message) {
    CrashInfo info;
    info.kind = kind;
    info.kit = g_kit_name;
    info.kit_file = g_exe_path;
    info.kit_timestamp = pe_timestamp(::GetModuleHandleA(nullptr));
    info.system = system_text();
    info.time = local_time_text();
    info.thread_id = thread_id;
    info.message = message;
    const EXCEPTION_RECORD* record = pointers.ExceptionRecord;
    if (record != nullptr) {
        info.code = record->ExceptionCode;
        info.address =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(record->ExceptionAddress));
        std::uint32_t base = 0;
        if (info.address != 0 && module_at(info.address, info.module, base)) {
            info.module_offset = info.address - base;
        }
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
            record->NumberParameters >= 2) {
            info.has_access = true;
            info.access_kind = static_cast<std::uint32_t>(record->ExceptionInformation[0]);
            info.access_address = static_cast<std::uint32_t>(record->ExceptionInformation[1]);
        }
        char type[256];
        char what[512];
        read_cpp_exception(record, type, sizeof type, what, sizeof what);
        if (type[0] != '\0') {
            info.kind = CrashKind::cpp_exception;
            info.cpp_type = c1kit::readable_msvc_type(type);
        }
        if (what[0] != '\0' && info.message.empty()) {
            info.message = what;
        }
    }
    if (const CONTEXT* context = pointers.ContextRecord) {
        info.registers = {{"EAX", context->Eax}, {"EBX", context->Ebx},
                          {"ECX", context->Ecx}, {"EDX", context->Edx},
                          {"ESI", context->Esi}, {"EDI", context->Edi},
                          {"EBP", context->Ebp}, {"ESP", context->Esp},
                          {"EIP", context->Eip}, {"EFL", context->EFlags}};
    }
    return info;
}

bool write_text_file(const std::string& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    std::string windows;
    windows.reserve(text.size() + text.size() / 16);
    for (const char character : text) {
        if (character == '\n') {
            windows += '\r';
        }
        windows += character;
    }
    file << windows;
    return static_cast<bool>(file);
}

// --- the crash path ---------------------------------------------------------

struct CrashJob {
    CrashKind kind;
    EXCEPTION_POINTERS* pointers;
    DWORD thread_id;
    const char* message;
    char report_path[MAX_PATH];
};

DWORD WINAPI write_crash_files(void* parameter) {
    auto* job = static_cast<CrashJob*>(parameter);
    g_writer_thread_id = ::GetCurrentThreadId();
    const std::string stem = report_stem();
    const std::string report_path = stem + ".txt";
    // A basic report first, from the exception record alone: if gathering
    // the details below fails (handle_crash ends only this thread then),
    // this is what the report window shows.
    CrashInfo info = describe(job->kind, *job->pointers, job->thread_id,
                              job->message == nullptr ? "" : job->message);
    if (write_text_file(report_path,
                        c1kit::crash_report_text(info) +
                            "\n(The stack, the modules and the dump could not be written.)\n")) {
        copy_text(job->report_path, MAX_PATH, report_path.c_str());
    }
    if (g_test_writer_crash) {
        *static_cast<volatile int*>(nullptr) = 2;
    }
    DbgHelp help;
    const std::string dump_path = stem + ".dmp";
    if (help.write_dump != nullptr) {
        HANDLE file = ::CreateFileA(dump_path.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION exception{};
            exception.ThreadId = job->thread_id;
            exception.ExceptionPointers = job->pointers;
            exception.ClientPointers = FALSE;
            const bool dumped =
                help.write_dump(::GetCurrentProcess(), ::GetCurrentProcessId(), file,
                                static_cast<MINIDUMP_TYPE>(
                                    MiniDumpWithIndirectlyReferencedMemory |
                                    MiniDumpScanMemory),
                                &exception, nullptr, nullptr) != FALSE;
            ::CloseHandle(file);
            if (dumped) {
                info.dump_file = dump_path;
            } else {
                ::DeleteFileA(dump_path.c_str());
            }
        }
    }
    if (job->pointers->ContextRecord != nullptr) {
        walk_stack(help, *job->pointers->ContextRecord, job->thread_id, info);
        scan_stack(job->pointers->ContextRecord->Esp, info);
    }
    list_modules(info);
    if (write_text_file(report_path, c1kit::crash_report_text(info))) {
        copy_text(job->report_path, MAX_PATH, report_path.c_str());
    }
    return 0;
}

// Writes the report, opens a new copy of the kit to show it, and ends this
// one.  Never returns.
[[noreturn]] void handle_crash(CrashKind kind, EXCEPTION_POINTERS* pointers,
                               const char* message) {
    if (::InterlockedExchange(&g_crashing, 1) != 0) {
        // Failed again while reporting.  On the writer thread, end only that
        // thread: the crashed thread is waiting for it, and then shows the
        // basic report it wrote first.  Anywhere else, just end.
        if (::GetCurrentThreadId() == g_writer_thread_id) {
            ::ExitThread(1);
        }
        ::TerminateProcess(::GetCurrentProcess(), 0xC1C1);
    }
    CrashJob job{kind, pointers, ::GetCurrentThreadId(), message, {}};
    HANDLE thread = ::CreateThread(nullptr, 256 * 1024, write_crash_files, &job, 0, nullptr);
    if (thread != nullptr) {
        ::WaitForSingleObject(thread, 60000);
        ::CloseHandle(thread);
    }
    if (job.report_path[0] != '\0') {
        std::string command = std::string("\"") + g_exe_path + "\" /CrashReport=\"" +
                              job.report_path + "\"";
        std::vector<char> line(command.begin(), command.end());
        line.push_back('\0');
        STARTUPINFOA startup{};
        startup.cb = sizeof startup;
        PROCESS_INFORMATION process{};
        if (::CreateProcessA(g_exe_path, line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                             nullptr, &startup, &process)) {
            ::CloseHandle(process.hThread);
            ::CloseHandle(process.hProcess);
        }
    }
    const DWORD code = pointers != nullptr && pointers->ExceptionRecord != nullptr
                           ? pointers->ExceptionRecord->ExceptionCode
                           : 0xC1C1;
    ::TerminateProcess(::GetCurrentProcess(), code);
    for (;;) {
        ::Sleep(INFINITE);
    }
}

// This thread's registers, taken here, and a record for `code`.  Read with
// a few instructions: RtlCaptureContext gave a wrong EIP under Wine when
// called from std::terminate, and catching a raised exception to copy its
// context failed inside the C runtime's frame handler.
__declspec(noinline) void capture_here(std::uint32_t code, CONTEXT* context,
                                       EXCEPTION_RECORD* record) {
    DWORD reg_eax = 0, reg_ebx = 0, reg_ecx = 0, reg_edx = 0, reg_esi = 0, reg_edi = 0,
          reg_ebp = 0, reg_esp = 0, reg_eip = 0, reg_flags = 0;
    __asm {
        mov reg_eax, eax
        mov reg_ebx, ebx
        mov reg_ecx, ecx
        mov reg_edx, edx
        mov reg_esi, esi
        mov reg_edi, edi
        mov reg_ebp, ebp
        mov reg_esp, esp
        pushfd
        pop reg_flags
        call here
    here:
        pop reg_eip
    }
    std::memset(context, 0, sizeof *context);
    context->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    context->Eax = reg_eax;
    context->Ebx = reg_ebx;
    context->Ecx = reg_ecx;
    context->Edx = reg_edx;
    context->Esi = reg_esi;
    context->Edi = reg_edi;
    context->Ebp = reg_ebp;
    context->Esp = reg_esp;
    context->Eip = reg_eip;
    context->EFlags = reg_flags;
    std::memset(record, 0, sizeof *record);
    record->ExceptionCode = code;
    record->ExceptionAddress = reinterpret_cast<void*>(static_cast<std::uintptr_t>(reg_eip));
}

// A failure with no exception record of its own (terminate, abort, ...):
// one made here.
[[noreturn]] void crash_here(CrashKind kind, std::uint32_t code, const char* message) {
    static CONTEXT context;
    static EXCEPTION_RECORD record;
    static EXCEPTION_POINTERS pointers;
    capture_here(code, &context, &record);
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;
    handle_crash(kind, &pointers, message);
}

LONG WINAPI on_unhandled_exception(EXCEPTION_POINTERS* pointers) {
    handle_crash(CrashKind::exception, pointers, nullptr);
}

void on_terminate() {
    static char message[512];
    message[0] = '\0';
    if (std::exception_ptr current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& error) {
            std::snprintf(message, sizeof message, "%s: %s", typeid(error).name(),
                          error.what());
        } catch (CException* error) {
            char text[400] = {};
            error->GetErrorMessage(text, sizeof text);
            std::snprintf(message, sizeof message, "%s: %s",
                          error->GetRuntimeClass()->m_lpszClassName, text);
        } catch (...) {
            copy_text(message, sizeof message, "an exception of unknown type");
        }
    }
    crash_here(CrashKind::terminate, c1kit::kCrashCodeTerminate,
               message[0] != '\0'
                   ? message
                   : "std::terminate was called with no exception in flight: most often "
                     "an exception thrown out of a noexcept function or a destructor.");
}

void on_invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int,
                          std::uintptr_t) {
    crash_here(CrashKind::invalid_parameter, c1kit::kCrashCodeInvalidParameter,
               "A C runtime function was given arguments it cannot use.");
}

void on_pure_call() {
    crash_here(CrashKind::pure_call, c1kit::kCrashCodePureCall,
               "A pure virtual function was called (an object used while it was being "
               "made or destroyed).");
}

void on_abort(int) {
    crash_here(CrashKind::abort, c1kit::kCrashCodeAbort, "abort() was called.");
}

// --- the report dialog ------------------------------------------------------

constexpr UINT kControlIcon = 1001;
constexpr UINT kControlSummary = 1002;
constexpr UINT kControlDetails = 1003;
constexpr UINT kControlCopy = 1004;
constexpr UINT kControlFolder = 1005;
constexpr int kWidth = 340;
constexpr int kHeight = 210;
constexpr int kMargin = 7;
constexpr int kButtonWidth = 60;
constexpr int kButtonHeight = 14;

CString windows_lines(const std::string& text) {
    CString out;
    for (const char character : text) {
        if (character == '\n') {
            out += _T("\r\n");
        } else if (character != '\r') {
            out += static_cast<TCHAR>(static_cast<unsigned char>(character));
        }
    }
    return out;
}

// An empty frame in the shell dialog font; OnInitDialog makes the controls.
std::vector<WORD> dialog_template() {
    std::vector<WORD> words;
    const auto push_dword = [&words](DWORD value) {
        words.push_back(LOWORD(value));
        words.push_back(HIWORD(value));
    };
    push_dword(DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    push_dword(0);
    words.push_back(0);  // controls
    words.push_back(0);
    words.push_back(0);
    words.push_back(kWidth);
    words.push_back(kHeight);
    words.push_back(0);  // no menu
    words.push_back(0);  // default class
    words.push_back(0);  // title, set later
    words.push_back(8);  // point size
    for (const wchar_t* face = L"MS Shell Dlg"; *face != L'\0'; ++face) {
        words.push_back(static_cast<WORD>(*face));
    }
    words.push_back(0);
    return words;
}

class CrashReportDialog : public CDialog {
public:
    CrashReportDialog(const std::string& title, const std::string& summary,
                      const std::string& details, const std::string& report_path)
        : title_(title.c_str()), summary_text_(windows_lines(summary)),
          details_text_(windows_lines(details)), report_path_(report_path) {}

    INT_PTR run() {
        const std::vector<WORD> words = dialog_template();
        if (!InitModalIndirect(reinterpret_cast<LPCDLGTEMPLATE>(words.data()), nullptr)) {
            return -1;
        }
        return DoModal();
    }

protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetWindowText(title_);
        CFont* font = GetFont();

        CRect icon_rect(kMargin, kMargin, kMargin + 20, kMargin + 20);
        MapDialogRect(&icon_rect);
        icon_.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_ICON, icon_rect, this, kControlIcon);
        icon_.SetIcon(::LoadIcon(nullptr, IDI_ERROR));

        CRect summary_rect(kMargin + 26, kMargin, kWidth - kMargin, kMargin + 38);
        MapDialogRect(&summary_rect);
        summary_.Create(summary_text_ + _T("\r\n\r\nPlease pass the details below on with ")
                                        _T("your bug report."),
                        WS_CHILD | WS_VISIBLE | SS_LEFT, summary_rect, this,
                        kControlSummary);
        summary_.SetFont(font);

        const int buttons_top = kHeight - kMargin - kButtonHeight;
        CRect details_rect(kMargin, kMargin + 42, kWidth - kMargin, buttons_top - kMargin);
        MapDialogRect(&details_rect);
        details_.CreateEx(WS_EX_CLIENTEDGE, _T("EDIT"), details_text_,
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
                              ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                          details_rect, this, kControlDetails);
        fixed_font_.CreatePointFont(80, _T("Courier New"));
        details_.SetFont(fixed_font_.GetSafeHandle() != nullptr ? &fixed_font_ : font);

        int left = kMargin;
        const auto button = [&](CButton& control, LPCTSTR text, UINT id, DWORD style) {
            CRect rect(left, buttons_top, left + kButtonWidth, buttons_top + kButtonHeight);
            MapDialogRect(&rect);
            control.Create(text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, rect, this, id);
            control.SetFont(font);
            left += kButtonWidth + 4;
        };
        button(copy_, _T("&Copy"), kControlCopy, BS_PUSHBUTTON);
        if (!report_path_.empty()) {
            button(folder_, _T("&Show file"), kControlFolder, BS_PUSHBUTTON);
        }
        left = kWidth - kMargin - kButtonWidth;
        button(close_, _T("Close"), IDOK, BS_DEFPUSHBUTTON);

        ::MessageBeep(MB_ICONERROR);
        close_.SetFocus();
        return FALSE;
    }

    afx_msg void OnCopy() {
        const CStringA text(summary_text_ + _T("\r\n\r\n") + details_text_);
        if (!OpenClipboard()) {
            return;
        }
        ::EmptyClipboard();
        const SIZE_T bytes = static_cast<SIZE_T>(text.GetLength()) + 1;
        if (HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* destination = ::GlobalLock(memory)) {
                std::memcpy(destination, text.GetString(), bytes);
                ::GlobalUnlock(memory);
                if (::SetClipboardData(CF_TEXT, memory) == nullptr) {
                    ::GlobalFree(memory);
                }
            } else {
                ::GlobalFree(memory);
            }
        }
        ::CloseClipboard();
    }

    afx_msg void OnShowFile() {
        const std::string arguments = "/select,\"" + report_path_ + "\"";
        ::ShellExecuteA(GetSafeHwnd(), "open", "explorer.exe", arguments.c_str(), nullptr,
                        SW_SHOWNORMAL);
    }

    DECLARE_MESSAGE_MAP()

private:
    CString title_;
    CString summary_text_;
    CString details_text_;
    std::string report_path_;
    CFont fixed_font_;
    CStatic icon_;
    CStatic summary_;
    CEdit details_;
    CButton copy_;
    CButton folder_;
    CButton close_;
};

BEGIN_MESSAGE_MAP(CrashReportDialog, CDialog)
    ON_BN_CLICKED(kControlCopy, &CrashReportDialog::OnCopy)
    ON_BN_CLICKED(kControlFolder, &CrashReportDialog::OnShowFile)
END_MESSAGE_MAP()

void show_report(const std::string& summary, const std::string& details,
                 const std::string& report_path) {
    CrashReportDialog dialog(g_kit_name, summary, details, report_path);
    if (dialog.run() == -1) {
        AfxMessageBox(CString(summary.c_str()) + _T("\n\n") + CString(details.c_str()),
                      MB_ICONSTOP);
    }
}

// --- testing ----------------------------------------------------------------
// C1KIT_CRASH_TEST=<kind> makes the kit fail on purpose once it is up, to
// check each path: access, throw, noexcept, invalid, purecall, abort,
// stack, mfc (an MFC exception in a message handler), wndthrow (a C++
// exception out of a message handler), reporter (access, and the report
// writer fails after its basic report).

struct PureBase {
    PureBase() { call(); }
    virtual ~PureBase() = default;
    void call() { run(); }
    virtual void run() = 0;
};
struct PureDerived : PureBase {
    void run() override {}
};

void throw_through_noexcept() noexcept {
    throw std::runtime_error("crash test: thrown through a noexcept function");
}

int recurse(volatile int depth) {
    volatile char pad[4096];
    pad[0] = static_cast<char>(depth);
    return recurse(depth + 1) + pad[0];
}

class CrashTestWindow : public CWnd {
public:
    explicit CrashTestWindow(bool mfc) : mfc_(mfc) {}

protected:
    afx_msg void OnTimer(UINT_PTR id) {
        KillTimer(id);
        if (mfc_) {
            AfxThrowNotSupportedException();
        }
        throw std::runtime_error("crash test: thrown out of a message handler");
    }
    DECLARE_MESSAGE_MAP()

private:
    bool mfc_;
};

BEGIN_MESSAGE_MAP(CrashTestWindow, CWnd)
    ON_WM_TIMER()
END_MESSAGE_MAP()

// The exe's path, and its name without ".exe" as the kit's name.
void note_kit_name() {
    ::GetModuleFileNameA(nullptr, g_exe_path, MAX_PATH);
    std::string name = file_name_of(g_exe_path);
    const std::size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        name.resize(dot);
    }
    copy_text(g_kit_name, sizeof g_kit_name, name.empty() ? "Kit" : name.c_str());
}

} // namespace

void install_crash_handlers() {
    note_kit_name();

    ::SetUnhandledExceptionFilter(on_unhandled_exception);
    std::set_terminate(on_terminate);
    _set_invalid_parameter_handler(on_invalid_parameter);
    _set_purecall_handler(on_pure_call);
    std::signal(SIGABRT, on_abort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    // A 32-bit program on 64-bit Windows can lose an exception thrown out of
    // a window procedure: Windows swallows it at the kernel callback and the
    // kit carries on in a broken state.  Let it through to the filter.
    if (HMODULE kernel = ::GetModuleHandleA("kernel32.dll")) {
        using GetPolicyFn = BOOL(WINAPI*)(LPDWORD);
        using SetPolicyFn = BOOL(WINAPI*)(DWORD);
        auto get_policy = reinterpret_cast<GetPolicyFn>(
            ::GetProcAddress(kernel, "GetProcessUserModeExceptionPolicy"));
        auto set_policy = reinterpret_cast<SetPolicyFn>(
            ::GetProcAddress(kernel, "SetProcessUserModeExceptionPolicy"));
        DWORD flags = 0;
        if (get_policy != nullptr && set_policy != nullptr && get_policy(&flags)) {
            set_policy(flags & ~0x1u);  // PROCESS_CALLBACK_FILTER_ENABLED
        }
    }
}

int show_crash_report_file(const std::string& report_path) {
    note_kit_name();
    std::ifstream file(report_path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string summary;
    std::string details;
    if (text.empty()) {
        summary = std::string(g_kit_name) + " stopped because of an error.";
        details = "The crash report could not be read: " + report_path;
    } else {
        c1kit::split_crash_report(text, summary, details);
        details += "\nReport: " + report_path + "\n";
    }
    show_report(summary, details, text.empty() ? std::string() : report_path);
    return 0;
}

void report_carried_on_error(const std::string& message) {
    // Once per run: an error in a paint or timer handler would otherwise
    // bring a new report each time it repeats.
    if (g_carried_on_reported) {
        return;
    }
    g_carried_on_reported = true;
    CONTEXT context{};
    EXCEPTION_RECORD record{};
    capture_here(c1kit::kCrashCodeCarriedOn, &context, &record);
    EXCEPTION_POINTERS pointers{&record, &context};
    CrashInfo info = describe(CrashKind::carried_on, pointers, ::GetCurrentThreadId(), message);
    info.address = 0;
    info.module.clear();
    info.module_offset = 0;
    DbgHelp help;
    walk_stack(help, context, ::GetCurrentThreadId(), info);
    scan_stack(context.Esp, info);
    const std::string report_path = report_stem() + ".txt";
    const bool written = write_text_file(report_path, c1kit::crash_report_text(info));
    std::string details = c1kit::crash_details(info);
    if (written) {
        details += "\nReport: " + report_path + "\n";
    }
    show_report(c1kit::crash_summary(info), details, written ? report_path : std::string());
}

void run_crash_test_if_asked() {
    char kind[32] = {};
    if (::GetEnvironmentVariableA("C1KIT_CRASH_TEST", kind, sizeof kind) == 0) {
        return;
    }
    const std::string test = kind;
    if (test == "reporter") {
        // An access violation whose report writer then fails as well.
        g_test_writer_crash = true;
        *static_cast<volatile int*>(nullptr) = 1;
    } else if (test == "access") {
        *static_cast<volatile int*>(nullptr) = 1;
    } else if (test == "throw") {
        throw std::out_of_range("crash test: thrown and not caught");
    } else if (test == "noexcept") {
        throw_through_noexcept();
    } else if (test == "invalid") {
        char* nowhere = nullptr;
        strcpy_s(nowhere, 1, "x");
    } else if (test == "purecall") {
        PureDerived object;
        (void)object;
    } else if (test == "abort") {
        std::abort();
    } else if (test == "stack") {
        recurse(0);
    } else if (test == "mfc" || test == "wndthrow") {
        auto* window = new CrashTestWindow(test == "mfc");
        if (window->CreateEx(0, AfxRegisterWndClass(0), _T(""), 0, 0, 0, 0, 0, HWND_MESSAGE,
                             nullptr)) {
            window->SetTimer(1, 500, nullptr);
        }
    }
}

} // namespace c1kitshell
