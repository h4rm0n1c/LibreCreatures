// SFC.OLE client: the kit side of the macro conversation.
//
// Every method is InvokeHelper(dispid, DISPATCH_METHOD, VT_BOOL, &ok,
// "\x4c\x4c", &request, &response): two VARIANT* arguments, passed by
// reference and therefore in reverse order in DISPPARAMS.  DISPIDs are the
// positions in the game's dispatch map (c1kit::SfcDispatchId); Observation's
// wrappers at 0x004065e0..0x004066a0 push exactly 1..5.

#include "c1kit/c1kit.hpp"
#include "c1kit/protocol.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <ole2.h>
#include <oleauto.h>

#include <cstdio>
#include <cstring>

namespace c1kit {
namespace {

class SfcOleTransport final : public MacroTransport {
public:
    SfcOleTransport(IDispatch* dispatch, std::size_t buffer_bytes)
        : dispatch_(dispatch), buffer_bytes_(buffer_bytes) {
        VariantInit(&buffer_);
    }

    ~SfcOleTransport() override {
        VariantClear(&buffer_);
        if (dispatch_ != nullptr) {
            dispatch_->Release();
        }
    }

    // The kits allocate the command buffer once, as a byte-length BSTR: the
    // length prefix is what lets the automation marshaller carry it
    // (CScienceSheet::ConnectToCreatures @ 0x0040b870; Observation
    // InitializeSheetPages @ 0x00403660 uses 0x400 bytes).
    bool allocate_buffer() {
        BSTR storage = SysAllocStringByteLen(nullptr,
                                             static_cast<UINT>(buffer_bytes_));
        if (storage == nullptr) {
            return false;
        }
        std::memset(storage, 0, buffer_bytes_);
        buffer_.vt = VT_BSTR;
        buffer_.bstrVal = storage;
        return true;
    }

    bool create_macro(short mode, long& handle) override {
        VARIANT request;
        VARIANT response;
        VariantInit(&request);
        VariantInit(&response);
        request.vt = VT_I2;
        request.iVal = mode;
        response.vt = VT_I4;
        response.lVal = 0;
        const bool ok = invoke(kSfcCreateMacro, &request, &response);
        handle = response.vt == VT_I4 ? response.lVal : 0;
        VariantClear(&response);
        return ok;
    }

    bool destroy_macro(long handle) override {
        VARIANT request = handle_variant(handle);
        VARIANT unused;
        VariantInit(&unused);
        const bool ok = invoke(kSfcDestroyMacro, &request, &unused);
        VariantClear(&unused);
        return ok;
    }

    bool load_macro(long handle, const char* script) override {
        if (!copy_script(script)) {
            return false;
        }
        VARIANT request = handle_variant(handle);
        return invoke(kSfcLoadMacro, &request, &buffer_);
    }

    bool execute_macro(long handle, const char* script) override {
        if (!copy_script(script)) {
            return false;
        }
        VARIANT request = handle_variant(handle);
        return invoke(kSfcExecuteMacro, &request, &buffer_);
    }

    // CMacroHolder::ExecuteAndPublishMacroOutput @ 0x00419340 releases the
    // BSTR we passed and stores a fresh one when there is output: the reply is
    // whatever bstrVal holds afterwards.  The kits keep that new BSTR as their
    // buffer from then on, and so does this transport.
    bool request_macro(long handle) override {
        VARIANT request = handle_variant(handle);
        const bool ok = invoke(kSfcRequestMacro, &request, &buffer_);
        reply_length_ = 0;
        reply_ = "";
        if (ok && buffer_.vt == VT_BSTR && buffer_.bstrVal != nullptr) {
            reply_ = reinterpret_cast<const char*>(buffer_.bstrVal);
            reply_length_ = std::strlen(reply_);
        }
        return ok;
    }

    const char* reply() const override { return reply_; }
    std::size_t reply_length() const override { return reply_length_; }
    void release() override { delete this; }

private:
    static VARIANT handle_variant(long handle) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.lVal = handle;
        return value;
    }

    // The script is written as plain ANSI into the byte-length BSTR, as the
    // kits do.  A reply BSTR may be shorter than the original buffer; the
    // kits then overrun it, so grow back to buffer_bytes_ instead.
    bool copy_script(const char* script) {
        if (script == nullptr) {
            return false;
        }
        const std::size_t length = std::strlen(script);
        if (buffer_.vt != VT_BSTR || buffer_.bstrVal == nullptr ||
            SysStringByteLen(buffer_.bstrVal) < buffer_bytes_) {
            VariantClear(&buffer_);
            if (!allocate_buffer()) {
                return false;
            }
        }
        const std::size_t copied =
            length < buffer_bytes_ - 1 ? length : buffer_bytes_ - 1;
        char* bytes = reinterpret_cast<char*>(buffer_.bstrVal);
        std::memcpy(bytes, script, copied);
        bytes[copied] = '\0';
        return true;
    }

    bool invoke(SfcDispatchId id, VARIANT* request, VARIANT* response) {
        VARIANT arguments[2];
        VariantInit(&arguments[0]);
        VariantInit(&arguments[1]);
        arguments[1].vt = VT_VARIANT | VT_BYREF;
        arguments[1].pvarVal = request;
        arguments[0].vt = VT_VARIANT | VT_BYREF;
        arguments[0].pvarVal = response;
        DISPPARAMS parameters = {arguments, nullptr, 2, 0};
        VARIANT result;
        VariantInit(&result);
        EXCEPINFO exception_info;
        std::memset(&exception_info, 0, sizeof(exception_info));
        UINT bad_argument = 0;
        const HRESULT hr = dispatch_->Invoke(
            static_cast<DISPID>(id), IID_NULL, LOCALE_USER_DEFAULT,
            DISPATCH_METHOD, &parameters, &result, &exception_info,
            &bad_argument);
        const bool answered_true =
            result.vt == VT_BOOL && result.boolVal != VARIANT_FALSE;
        VariantClear(&result);
        SysFreeString(exception_info.bstrSource);
        SysFreeString(exception_info.bstrDescription);
        SysFreeString(exception_info.bstrHelpFile);
        return SUCCEEDED(hr) && answered_true;
    }

    IDispatch* dispatch_ = nullptr;
    std::size_t buffer_bytes_ = 0;
    VARIANT buffer_;
    const char* reply_ = "";
    std::size_t reply_length_ = 0;
};

} // namespace

MacroTransport* connect_sfc_ole(std::size_t buffer_bytes,
                                ConnectResult* result, long* hresult) {
    if (result != nullptr) {
        *result = ConnectResult::not_registered;
    }
    if (hresult != nullptr) {
        *hresult = 0;
    }
    CLSID clsid;
    const HRESULT lookup = CLSIDFromProgID(L"SFC.OLE", &clsid);
    if (FAILED(lookup)) {
        if (hresult != nullptr) {
            *hresult = lookup;
        }
        return nullptr;
    }
    // Asking CoCreateInstance for IDispatch directly failed on Windows with
    // E_NOINTERFACE where the 1996 kits, which go through IUnknown and
    // OleRun first, connect; so take their route.
    const auto fail = [&](ConnectResult why, HRESULT hr) -> MacroTransport* {
        if (result != nullptr) {
            *result = why;
        }
        if (hresult != nullptr) {
            *hresult = hr;
        }
        return nullptr;
    };
    IUnknown* unknown = nullptr;
    const HRESULT created =
        CoCreateInstance(clsid, nullptr, CLSCTX_ALL, IID_IUnknown,
                         reinterpret_cast<void**>(&unknown));
    if (FAILED(created) || unknown == nullptr) {
        return fail(ConnectResult::create_failed, created);
    }
    const HRESULT running = OleRun(unknown);
    if (FAILED(running)) {
        unknown->Release();
        return fail(ConnectResult::run_failed, running);
    }
    IDispatch* dispatch = nullptr;
    const HRESULT queried = unknown->QueryInterface(
        IID_IDispatch, reinterpret_cast<void**>(&dispatch));
    unknown->Release();
    if (FAILED(queried) || dispatch == nullptr) {
        return fail(ConnectResult::no_dispatch, queried);
    }
    auto* transport = new SfcOleTransport(dispatch, buffer_bytes < 2 ? 2 : buffer_bytes);
    if (!transport->allocate_buffer()) {
        transport->release();
        if (result != nullptr) {
            *result = ConnectResult::create_failed;
        }
        return nullptr;
    }
    if (result != nullptr) {
        *result = ConnectResult::connected;
    }
    return transport;
}

void describe_connect_failure(ConnectResult result, long hresult,
                              char* buffer, std::size_t size) {
    if (buffer == nullptr || size == 0) {
        return;
    }
    char system_text[256] = {};
    DWORD length = 0;
    if (hresult != 0) {
        length = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
            static_cast<DWORD>(hresult), 0, system_text, sizeof(system_text),
            nullptr);
    }
    while (length > 0 && (system_text[length - 1] == '\r' ||
                          system_text[length - 1] == '\n' ||
                          system_text[length - 1] == ' ')) {
        system_text[--length] = '\0';
    }
    // Not in the originals: which step failed and its code, so a report
    // from a machine we cannot test on says where the connection broke.
    const char* step = result == ConnectResult::not_registered
                           ? "SFC.OLE is not registered"
                       : result == ConnectResult::create_failed
                           ? "creating SFC.OLE"
                       : result == ConnectResult::run_failed
                           ? "starting SFC.OLE"
                       : result == ConnectResult::no_dispatch
                           ? "asking SFC.OLE for IDispatch"
                           : "connecting";
    std::snprintf(buffer, size, "%s\n\n(%s: 0x%08lX)",
                  length > 0 ? system_text
                             : "Can not communicate with application",
                  step, static_cast<unsigned long>(hresult));
}

} // namespace c1kit
