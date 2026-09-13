#include "windows_pipe_dispatch_proxy.hpp"

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

bool WindowsPipeDispatchProxy::send_to_kit(std::uint32_t first_word,
                                           std::uint32_t second_word) const {
    // Three connect attempts with a 50ms pause between them, except that a
    // missing pipe means the kit has closed and is not retried.
    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 3 && pipe == INVALID_HANDLE_VALUE;
         ++attempt) {
        pipe = CreateFileA(pipe_name_.data(), GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                OutputDebugStringA(
                    "CPipeDispatchProxy::SendToKit: Pipe not found - kit has "
                    "closed\n");
                return false;
            }
            if (attempt < 2) {
                Sleep(50);
            }
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
