#include "windows_embedded_kit_host.hpp"
#include "windows_pipe_server_boundary.hpp"

#include <cstdio>

#include <array>
#include <string>

#include "../common/logging.hpp"
#include "environment.hpp"
#include "windows_com_host.hpp"
#include "windows_pipe_dispatch_proxy.hpp"
#include "windows_shell.hpp"

namespace creatures1::platform {

bool WindowsEmbeddedKitHost::running_under_wine() const {
    // The recovered launcher branches on the same Wine probe the pipe server
    // uses: a wine_get_version export in ntdll.
    class Probe final : public WineVersionExportProbe {
    public:
        bool wine_get_version_is_available() const override {
            const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
            return ntdll != nullptr &&
                   GetProcAddress(ntdll, "wine_get_version") != nullptr;
        }
    } probe;
    return WineEnvironment(probe).is_running_under_wine();
}

bool invoke_kit_communicate(
    COleDispatchDriver& driver,
    const creatures1::application::EmbeddedKitMessage& message) {
    // Parameter info "\x4c\x4c" is two VTS_PVARIANT, so both arguments travel
    // as VARIANT* and MFC marshals them as VT_VARIANT|VT_BYREF.  Passing the
    // two values as plain VTS_I4 looks equivalent but is not: a kit's dispatch
    // map declares "\x4c\x4c" too, and MFC will not coerce an I4 into it.
    VARIANT header;
    VARIANT payload;
    VariantInit(&header);
    VariantInit(&payload);
    header.vt = VT_I4;
    header.lVal = static_cast<long>(message.header);
    payload.vt = VT_I4;
    payload.lVal = static_cast<long>(message.payload);

    char line[96];
    std::snprintf(line, sizeof(line), "COMMUNICATE %08lx %08lx",
                  static_cast<unsigned long>(message.header),
                  static_cast<unsigned long>(message.payload));
    try {
        BOOL accepted = FALSE;
        driver.InvokeHelper(
            1, DISPATCH_METHOD, VT_BOOL, &accepted,
            reinterpret_cast<const BYTE*>(VTS_PVARIANT VTS_PVARIANT), &header,
            &payload);
        log_kit_traffic_line(std::string(line) +
                             (accepted != FALSE ? "  ->  TRUE" : "  ->  FALSE"));
        return accepted != FALSE;
    } catch (CException* error) {
        char detail[512] = {};
        if (error != nullptr) {
            error->GetErrorMessage(detail, sizeof(detail));
            if (COleException* ole = dynamic_cast<COleException*>(error)) {
                char code[32];
                std::snprintf(code, sizeof(code), " (sc %08lx)",
                              static_cast<unsigned long>(ole->m_sc));
                strncat_s(detail, code, _TRUNCATE);
            }
            error->Delete();
        }
        log_kit_traffic_line(std::string(line) + "  ->  failed: " + detail);
        C1DebugConsoleDialog* console = active_debug_console();
        if (console != nullptr) {
            creatures1::common::debug_log(
                *console, 0x2000,
                "Embedded kit communication failed: %s\n",
                detail[0] == '\0' ? "MFC OLE exception" : detail);
        }
        return false;
    } catch (const std::exception& error) {
        C1DebugConsoleDialog* console = active_debug_console();
        if (console != nullptr) {
            creatures1::common::debug_log(
                *console, 0x2000,
                "Embedded kit communication failed: %s\n", error.what());
        }
        return false;
    } catch (...) {
        C1DebugConsoleDialog* console = active_debug_console();
        if (console != nullptr) {
            creatures1::common::debug_log(
                *console, 0x2000,
                "Embedded kit communication failed: unknown exception\n");
        }
        return false;
    }
}

bool WindowsEmbeddedKitHost::launch_via_native_com(std::size_t tool_index) {
    // Recovered ExecuteEmbeddedKitTool @ 004444e0, native branch: create the
    // dispatch from the registry ProgID, report the OLE exception on failure,
    // then send YOUR_ID_IS and treat a false result as a failed launch.  The
    // native branch builds that message inline rather than through 0042d860,
    // but builds it identically, so both branches send it the same way here.
    COleException exception;
    COleDispatchDriver& driver = frame_.embedded_kit_dispatch(tool_index);
    const std::string prog_id(
        frame_.embedded_kit_definitions()[tool_index].prog_id);

    if (!driver.CreateDispatch(prog_id.c_str(), &exception)) {
        if (exception.ReportError(0, 0) == 0) {
            AfxMessageBox("Can not communicate with application", MB_OK, 0);
        }
        return false;
    }

    ++frame_.active_embedded_tool_count();
    if (!creatures1::application::send_your_id_is_message(*this, tool_index)) {
        creatures1::application::shutdown_embedded_kit_tool(
            *this, tool_index, frame_.active_embedded_tool_count());
        return false;
    }
    invalidate_toolbar();
    return true;
}

bool WindowsEmbeddedKitHost::launch_via_wine_proxy(std::size_t tool_index) {
    // Recovered ExecuteEmbeddedKitTool @ 004444e0, Wine branch.  OLEKitProxy.dll
    // sits beside the executable and is not part of this port; when it is
    // absent the recovered code reports exactly this and gives up.
    std::array<char, MAX_PATH> module_path{};
    GetModuleFileNameA(nullptr, module_path.data(),
                       static_cast<DWORD>(module_path.size()));
    char* last_separator = strrchr(module_path.data(), '\\');
    char* write_at =
        last_separator == nullptr ? module_path.data() : last_separator + 1;
    strcpy_s(write_at,
             module_path.size() - (write_at - module_path.data()),
             "OLEKitProxy.dll");

    const HMODULE proxy_dll = LoadLibraryA(module_path.data());
    if (proxy_dll == nullptr) {
        AfxMessageBox("OLEKitProxy.dll not found. Kit communication is not "
                      "available under Wine.", MB_OK, 0);
        return false;
    }

    // Confirmed against the shipped OLEKitProxy.dll: __stdcall (ret 0xc) with
    // szExePath, toolID and progID.  The native call passed the address of the
    // embedded record, whose first member is the registry tool name, so the
    // third argument is that ProgID string.
    using LaunchKitWithInjectionFn = HANDLE(__stdcall*)(const char*, int,
                                                        const char*);
    const auto launch = reinterpret_cast<LaunchKitWithInjectionFn>(
        GetProcAddress(proxy_dll, "LaunchKitWithInjection"));
    if (launch == nullptr) {
        FreeLibrary(proxy_dll);
        AfxMessageBox("OLEKitProxy.dll is invalid. Kit communication is not "
                      "available under Wine.", MB_OK, 0);
        return false;
    }

    WindowsComLocalServer com_resolver;
    std::string kit_executable_path;
    if (!creatures1::platform::resolve_com_progid_local_server_path(
            com_resolver,
            frame_.embedded_kit_definitions()[tool_index].prog_id,
            kit_executable_path)) {
        FreeLibrary(proxy_dll);
        AfxMessageBox("Cannot find executable path for kit.", MB_OK, 0);
        return false;
    }

    const std::string kit_prog_id(
        frame_.embedded_kit_definitions()[tool_index].prog_id);
    const HANDLE kit_process = launch(kit_executable_path.c_str(),
                                      static_cast<int>(tool_index),
                                      kit_prog_id.c_str());
    if (kit_process == nullptr) {
        FreeLibrary(proxy_dll);
        AfxMessageBox("Failed to launch kit. Please check the Wine debug log "
                      "for details.", MB_OK, 0);
        return false;
    }

    // The proxy is handed to MFC with ownership, so its initial reference is
    // the one AttachDispatch takes.
    auto* proxy = new WindowsPipeDispatchProxy(tool_index);
    frame_.embedded_kit_dispatch(tool_index).AttachDispatch(proxy, TRUE);
    frame_.embedded_kit_process_handle(tool_index) =
        reinterpret_cast<std::uintptr_t>(kit_process);
    ++frame_.active_embedded_tool_count();

    if (!creatures1::application::send_your_id_is_message(*this, tool_index)) {
        creatures1::application::shutdown_embedded_kit_tool(
            *this, tool_index, frame_.active_embedded_tool_count());
        AfxMessageBox("Failed to communicate with kit after launch.", MB_OK, 0);
        return false;
    }

    invalidate_toolbar();
    return true;
}

bool WindowsEmbeddedKitHost::send_kit_message(
    std::size_t tool_index,
    const creatures1::application::EmbeddedKitMessage& message) {
    return invoke_kit_communicate(frame_.embedded_kit_dispatch(tool_index),
                                  message);
}

bool WindowsEmbeddedKitHost::has_dispatch(std::size_t tool_index) const {
    return const_cast<C1MainFrame&>(frame_)
               .embedded_kit_dispatch(tool_index)
               .m_lpDispatch != nullptr;
}

void WindowsEmbeddedKitHost::release_dispatch(std::size_t tool_index) {
    frame_.embedded_kit_dispatch(tool_index).ReleaseDispatch();
}

creatures1::application::EmbeddedKitProcessHandle
WindowsEmbeddedKitHost::take_process_handle(std::size_t tool_index) {
    std::uintptr_t& slot = frame_.embedded_kit_process_handle(tool_index);
    const std::uintptr_t handle = slot;
    slot = 0;
    return handle;
}

bool WindowsEmbeddedKitHost::query_process_exit_code(
    creatures1::application::EmbeddedKitProcessHandle handle,
    std::uint32_t& exit_code) const {
    DWORD code = 0;
    if (handle == 0 ||
        GetExitCodeProcess(reinterpret_cast<HANDLE>(handle), &code) == FALSE) {
        return false;
    }
    exit_code = static_cast<std::uint32_t>(code);
    return true;
}

void WindowsEmbeddedKitHost::terminate_process(
    creatures1::application::EmbeddedKitProcessHandle handle) {
    if (handle != 0) {
        TerminateProcess(reinterpret_cast<HANDLE>(handle), 0);
    }
}

void WindowsEmbeddedKitHost::close_process_handle(
    creatures1::application::EmbeddedKitProcessHandle handle) {
    if (handle != 0) {
        CloseHandle(reinterpret_cast<HANDLE>(handle));
    }
}

void WindowsEmbeddedKitHost::invalidate_toolbar() {
    frame_.invalidate_main_toolbar();
}

bool WindowsEmbeddedKitHost::diagnostics_enabled() const { return false; }

void WindowsEmbeddedKitHost::log_dispatch_release(std::size_t tool_index,
                                                  int active_tool_count) {
    char buffer[128]{};
    wsprintfA(buffer, "ToolQuit: Released dispatch for tool %d (ActiveTools=%d)\n",
              static_cast<int>(tool_index), active_tool_count);
    OutputDebugStringA(buffer);
}

void WindowsEmbeddedKitHost::log_process_termination(std::size_t tool_index) {
    char buffer[128]{};
    wsprintfA(buffer, "ToolQuit: Terminated process for tool %d\n",
              static_cast<int>(tool_index));
    OutputDebugStringA(buffer);
}

} // namespace creatures1::platform
