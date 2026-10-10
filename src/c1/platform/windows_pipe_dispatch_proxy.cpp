#include "windows_pipe_dispatch_proxy.hpp"

#include "../common/lab_trace.hpp"
#include "windows_shell.hpp"

namespace creatures1::platform {

WindowsPipeDispatchProxy::WindowsPipeDispatchProxy(std::size_t tool_index)
    : tool_index_(tool_index) {
    wsprintfA(pipe_name_.data(), "\\\\.\\pipe\\Creatures1_Kit_Tool%d",
              static_cast<int>(tool_index));
}

HRESULT STDMETHODCALLTYPE
WindowsPipeDispatchProxy::QueryInterface(REFIID riid, void** object_out) {
    if (object_out == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IDispatch) {
        *object_out = static_cast<IDispatch*>(this);
        AddRef();
        return S_OK;
    }
    *object_out = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WindowsPipeDispatchProxy::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&reference_count_));
}

ULONG STDMETHODCALLTYPE WindowsPipeDispatchProxy::Release() {
    const LONG remaining = InterlockedDecrement(&reference_count_);
    if (remaining == 0) {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(remaining);
}

HRESULT STDMETHODCALLTYPE
WindowsPipeDispatchProxy::GetTypeInfoCount(UINT* count_out) {
    // The recovered proxy reports zero type-info objects.
    if (count_out != nullptr) {
        *count_out = 0;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsPipeDispatchProxy::GetTypeInfo(
    UINT /*index*/, LCID /*lcid*/, ITypeInfo** type_info_out) {
    if (type_info_out != nullptr) {
        *type_info_out = nullptr;
    }
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WindowsPipeDispatchProxy::GetIDsOfNames(
    REFIID /*riid*/, LPOLESTR* names, UINT name_count, LCID /*lcid*/,
    DISPID* dispids_out) {
    // Only "Communicate" is recognised, case-insensitively, and it is DISPID 1.
    for (UINT index = 0; index < name_count; ++index) {
        if (_wcsicmp(names[index], L"Communicate") != 0) {
            dispids_out[index] = DISPID_UNKNOWN;
            return DISP_E_UNKNOWNNAME;
        }
        dispids_out[index] = 1;
    }
    return S_OK;
}

bool WindowsPipeDispatchProxy::kit_process_has_exited() const {
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr) {
        return false;
    }
    const auto process =
        reinterpret_cast<HANDLE>(frame->embedded_kit_process_handle(tool_index_));
    DWORD exit_code = 0;
    return process != nullptr && GetExitCodeProcess(process, &exit_code) != FALSE &&
           exit_code != STILL_ACTIVE;
}

// LibreCreatures deviation.  A kit that ends normally tells the game (its
// "app: quit" or the proxy in the kit posts message 0x401) and the slot is
// shut down.  A kit that crashed or was killed never does: native keeps the
// slot as running, so every later message to it fails, and the first click
// on its Tools item only shuts the slot down instead of opening the kit.
// Ask for the same shutdown here, once, when the kit's process is gone.
void WindowsPipeDispatchProxy::request_shutdown_of_exited_kit() const {
    if (shutdown_requested_ || !kit_process_has_exited()) {
        return;
    }
    C1MainFrame* frame = active_main_frame();
    if (frame == nullptr || frame->GetSafeHwnd() == nullptr) {
        return;
    }
    shutdown_requested_ = true;
    creatures1::common::lab_trace(creatures1::common::LabTrace::kits,
                                  "kit process gone slot=%u: shutting the slot down",
                                  static_cast<unsigned>(tool_index_));
    frame->PostMessage(0x401, static_cast<WPARAM>(tool_index_), 0);
}

bool WindowsPipeDispatchProxy::send_to_kit(std::uint32_t first_word,
                                           std::uint32_t second_word) const {
    // Keep trying for up to a second: straight after one message (a new
    // kit's YOUR_ID_IS, say) the kit's pipe can be missing or busy for a
    // moment -- in the lab the call right after YOUR_ID_IS failed to find it.
    // The earlier rule -- three tries,
    // and a missing pipe taken to mean the kit had closed -- dropped the
    // message sent right after a kit started: the Funeral Kit, launched by
    // clicking a dead creature, never heard of the death.
    HANDLE pipe = INVALID_HANDLE_VALUE;
    const DWORD started = GetTickCount();
    while (pipe == INVALID_HANDLE_VALUE) {
        pipe = CreateFileA(pipe_name_.data(), GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }
        const DWORD error = GetLastError();
        // Native gives up at once on a missing pipe.  The wait above is for
        // a kit still starting; one whose process has ended never will.
        if (error == ERROR_FILE_NOT_FOUND && kit_process_has_exited()) {
            request_shutdown_of_exited_kit();
            return false;
        }
        if (GetTickCount() - started >= 1000) {
            if (error == ERROR_FILE_NOT_FOUND) {
                OutputDebugStringA(
                    "CPipeDispatchProxy::SendToKit: Pipe not found - kit has "
                    "closed\n");
                return false;
            }
            break;
        }
        if (error == ERROR_PIPE_BUSY) {
            WaitNamedPipeA(pipe_name_.data(), 100);
        } else {
            Sleep(20);
        }
    }
    if (pipe == INVALID_HANDLE_VALUE) {
        OutputDebugStringA(
            "CPipeDispatchProxy::SendToKit: Failed to connect to pipe\n");
        return false;
    }

    // The wire payload is the two arguments in source order.  DISPPARAMS
    // stores arguments in reverse, so rgvarg[1] is the first parameter.
    const std::uint32_t payload[2] = {first_word, second_word};
    DWORD written = 0;
    const BOOL ok =
        WriteFile(pipe, payload, sizeof(payload), &written, nullptr);
    if (ok != FALSE && written == sizeof(payload)) {
        FlushFileBuffers(pipe);
        CloseHandle(pipe);
        return true;
    }
    OutputDebugStringA("CPipeDispatchProxy::SendToKit: WriteFile failed\n");
    CloseHandle(pipe);
    return false;
}

HRESULT STDMETHODCALLTYPE WindowsPipeDispatchProxy::Invoke(
    DISPID dispatch_id, REFIID /*riid*/, LCID /*lcid*/, WORD invoke_flags,
    DISPPARAMS* params, VARIANT* result_out, EXCEPINFO* exception_info,
    UINT* /*arg_error*/) {
    if (dispatch_id != 1 || (invoke_flags & DISPATCH_METHOD) == 0) {
        return DISP_E_MEMBERNOTFOUND;
    }
    if (params == nullptr || params->cArgs != 2) {
        return DISP_E_BADPARAMCOUNT;
    }

    VARIANT* first = &params->rgvarg[0];
    VARIANT* second = &params->rgvarg[1];
    if (second->vt == (VT_VARIANT | VT_BYREF)) {
        if (first->vt != (VT_VARIANT | VT_BYREF)) {
            return DISP_E_TYPEMISMATCH;
        }
        first = first->pvarVal;
        second = second->pvarVal;
    }
    if (second->vt != VT_I4 || first->vt != VT_I4) {
        return DISP_E_TYPEMISMATCH;
    }

    const bool sent = send_to_kit(static_cast<std::uint32_t>(second->lVal),
                                  static_cast<std::uint32_t>(first->lVal));
    if (!sent) {
        request_shutdown_of_exited_kit();
    }

    if (result_out != nullptr) {
        result_out->vt = VT_BOOL;
        result_out->boolVal = sent ? VARIANT_TRUE : VARIANT_FALSE;
    }
    if (sent) {
        return S_OK;
    }
    if (exception_info != nullptr) {
        *exception_info = EXCEPINFO{};
        exception_info->wCode = 0x3e9;
        exception_info->bstrSource = SysAllocString(L"CPipeDispatchProxy");
        exception_info->bstrDescription =
            SysAllocString(L"Failed to communicate with kit via pipe");
        exception_info->scode = static_cast<SCODE>(0x80004005);
    }
    return DISP_E_EXCEPTION;
}

} // namespace creatures1::platform
