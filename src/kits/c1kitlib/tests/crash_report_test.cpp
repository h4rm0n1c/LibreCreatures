// crash_report.hpp: report text, type names and file names.

#include "c1kit/crash_report.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace c1kit;

namespace {

void test_type_names() {
    assert(readable_msvc_type(".?AVout_of_range@std@@") == "std::out_of_range");
    assert(readable_msvc_type(".?AUFoo@@") == "Foo");
    assert(readable_msvc_type(".?AVbad@inner@outer@@") == "outer::inner::bad");
    // Forms it does not decode are kept as they are.
    assert(readable_msvc_type(".?AV?$vector@H@std@@") == ".?AV?$vector@H@std@@");
    assert(readable_msvc_type("int") == "int");
    assert(readable_msvc_type(".?AVnoend") == ".?AVnoend");
}

void test_access_violation() {
    CrashInfo info;
    info.kit = "Hatchery";
    info.kit_file = "C:\\Kits\\Hatchery.exe";
    info.code = 0xC0000005;
    info.address = 0x00401234;
    info.module = "Hatchery.exe";
    info.module_offset = 0x1234;
    info.has_access = true;
    info.access_kind = 1;
    info.access_address = 0x10;
    info.registers = {{"EAX", 1}, {"EBX", 2}, {"ECX", 3}, {"EDX", 4}, {"EIP", 0x401234}};
    info.frames = {{0x401234, "Hatchery.exe", 0x1234, "", 0},
                   {0x10002000, "c1kitlib.dll", 0x2000, "connect_sfc_ole", 0x10}};
    info.modules = {{"Hatchery.exe", 0x400000, 0x30000, 0x5f000000}};
    info.stack_scan = {{0x408312, "Hatchery.exe", 0x8312, "", 0}};
    const std::string text = crash_report_text(info);
    assert(text.rfind("Hatchery stopped because of an error (access violation at "
                      "Hatchery.exe+0x00001234).\n\n", 0) == 0);
    assert(text.find("Where: Hatchery.exe+0x00001234\n") != std::string::npos);
    assert(text.find("Access: writing 0x00000010\n") != std::string::npos);
    assert(text.find("  EAX=0x00000001  EBX=0x00000002  ECX=0x00000003  "
                     "EDX=0x00000004\n  EIP=0x00401234\n") != std::string::npos);
    assert(text.find("c1kitlib.dll+0x00002000  connect_sfc_ole+0x00000010\n") !=
           std::string::npos);
    assert(text.find("\nReturn addresses on the stack (some may be stale):\n"
                     "  0x00408312  Hatchery.exe+0x00008312\n") != std::string::npos);
    assert(text.find("0x00400000-0x00430000  Hatchery.exe  (0x5F000000)") !=
           std::string::npos);

    std::string summary, details;
    split_crash_report("line one\r\n\r\nKit: x\r\nError: y\r\n", summary, details);
    assert(summary == "line one" && details == "Kit: x\nError: y\n");
    split_crash_report(text, summary, details);
    assert(summary == crash_summary(info) && details == crash_details(info));
}

void test_other_kinds() {
    CrashInfo info;
    info.kit = "Science Kit";
    info.kind = CrashKind::cpp_exception;
    info.code = kMsvcCppExceptionCode;
    info.cpp_type = "std::out_of_range";
    info.message = "invalid vector subscript";
    assert(crash_summary(info) ==
           "Science Kit stopped because of an error it did not handle "
           "(std::out_of_range).");
    assert(crash_details(info).find("Message: invalid vector subscript\n") !=
           std::string::npos);
    // Where a C++ exception was raised is RaiseException: left out.
    assert(crash_details(info).find("Where:") == std::string::npos);
    info.kind = CrashKind::pure_call;
    info.code = kCrashCodePureCall;
    assert(crash_summary(info) == "Science Kit stopped because of an internal error "
                                  "(pure virtual function call).");
    info.kind = CrashKind::carried_on;
    info.code = kCrashCodeCarriedOn;
    assert(crash_summary(info).rfind("Science Kit had an error. It carried on", 0) == 0);
}

void test_file_stem() {
    assert(crash_file_stem("Breeder's Kit", 2026, 10, 6, 9, 5, 3, 4242) ==
           "Breeder's Kit 2026-10-06 09.05.03 4242");
    assert(crash_file_stem("a:b/c", 2026, 1, 2, 3, 4, 5, 6) ==
           "a_b_c 2026-01-02 03.04.05 6");
    assert(crash_file_stem("", 2026, 1, 2, 3, 4, 5, 6).rfind("Kit ", 0) == 0);
}

} // namespace

int main() {
    test_type_names();
    test_access_violation();
    test_other_kinds();
    test_file_stem();
    std::puts("crash_report_test: all passed");
    return 0;
}
