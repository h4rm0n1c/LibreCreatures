#pragma once

#include "windows_prelude.hpp"
#include <oaidl.h>

#include <array>
#include <cstddef>

namespace creatures1::platform {

// Recovered CPipeDispatchProxy: the IDispatch an embedded kit is given under
// Wine, where COM activation is unavailable.  Calls to "Communicate" are
// marshalled as two 32-bit words down a per-tool named pipe.  The native
// object is 0x4c bytes: vtable, reference count, tool index, 64-byte pipe name.
class WindowsPipeDispatchProxy final : public IDispatch {
public:
    explicit WindowsPipeDispatchProxy(std::size_t tool_index);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                             void** object_out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDispatch
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count_out) override;
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT index, LCID lcid,
                                          ITypeInfo** type_info_out) override;
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR* names,
                                            UINT name_count, LCID lcid,
                                            DISPID* dispids_out) override;
    HRESULT STDMETHODCALLTYPE Invoke(DISPID dispatch_id, REFIID riid, LCID lcid,
                                     WORD invoke_flags, DISPPARAMS* params,
                                     VARIANT* result_out,
                                     EXCEPINFO* exception_info,
                                     UINT* arg_error) override;

private:
    bool send_to_kit(std::uint32_t first_word, std::uint32_t second_word) const;
    // Not native: whether the kit's process (LaunchKitWithInjection's
    // handle) has ended, and the one shutdown request posted for it.
    bool kit_process_has_exited() const;
    void request_shutdown_of_exited_kit() const;

    LONG reference_count_ = 1;
    std::size_t tool_index_ = 0;
    std::array<char, 0x40> pipe_name_{};
    mutable bool shutdown_requested_ = false;
};

} // namespace creatures1::platform
