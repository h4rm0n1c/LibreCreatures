// The kit's OLE local server: a class factory and an IDispatch exposing the
// single method the game calls, "Communicate" (dispid 1, two VARIANT* args,
// VT_BOOL result).  Every 1996 kit declares it in its MFC dispatch map
// (Science Kit's entry at 0x00412df8); the game asks for it by name.

#include "c1kit/c1kit.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

#include <cstring>

namespace c1kit {
namespace {

constexpr DISPID kCommunicateDispid = 1;

bool read_i4(const VARIANT& argument, std::int32_t& value) {
    const VARIANT* resolved = &argument;
    if (resolved->vt == (VT_VARIANT | VT_BYREF) && resolved->pvarVal != nullptr) {
        resolved = resolved->pvarVal;
    }
    switch (resolved->vt) {
    case VT_I4:
        value = resolved->lVal;
        return true;
    case VT_I4 | VT_BYREF:
        if (resolved->plVal == nullptr) {
            return false;
        }
        value = *resolved->plVal;
        return true;
    case VT_I2:
        value = resolved->iVal;
        return true;
    default: {
        VARIANT converted;
        VariantInit(&converted);
        if (SUCCEEDED(VariantChangeType(&converted,
                                        const_cast<VARIANT*>(resolved), 0,
                                        VT_I4))) {
            value = converted.lVal;
            return true;
        }
        return false;
    }
    }
}

class KitDispatch final : public IDispatch {
public:
    explicit KitDispatch(KitEvents& events) : events_(events) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (out == nullptr) {
            return E_POINTER;
        }
        *out = nullptr;
        if (InlineIsEqualGUID(iid, IID_IUnknown) ||
            InlineIsEqualGUID(iid, IID_IDispatch)) {
            *out = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&references_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG remaining = InterlockedDecrement(&references_);
        if (remaining == 0) {
            events_.on_last_client_released();
            delete this;
        }
        return static_cast<ULONG>(remaining);
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {
        if (count != nullptr) {
            *count = 0;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR* names,
                                            UINT count, LCID,
                                            DISPID* ids) override {
        if (names == nullptr || ids == nullptr || count == 0) {
            return E_INVALIDARG;
        }
        HRESULT hr = S_OK;
        for (UINT index = 0; index < count; ++index) {
            ids[index] = DISPID_UNKNOWN;
            if (index == 0 && names[0] != nullptr &&
                lstrcmpiW(names[0], L"Communicate") == 0) {
                ids[0] = kCommunicateDispid;
            } else {
                hr = DISP_E_UNKNOWNNAME;
            }
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags,
                                     DISPPARAMS* params, VARIANT* result,
                                     EXCEPINFO*, UINT* bad_argument) override {
        if (id != kCommunicateDispid) {
            return DISP_E_MEMBERNOTFOUND;
        }
        if ((flags & DISPATCH_METHOD) == 0) {
            return DISP_E_MEMBERNOTFOUND;
        }
        if (params == nullptr || params->cArgs != 2) {
            return DISP_E_BADPARAMCOUNT;
        }
        // DISPPARAMS holds arguments in reverse: rgvarg[1] is the header.
        std::int32_t header = 0;
        std::int32_t payload = 0;
        if (!read_i4(params->rgvarg[1], header)) {
            if (bad_argument != nullptr) {
                *bad_argument = 1;
            }
            return DISP_E_TYPEMISMATCH;
        }
        if (!read_i4(params->rgvarg[0], payload)) {
            if (bad_argument != nullptr) {
                *bad_argument = 0;
            }
            return DISP_E_TYPEMISMATCH;
        }
        const bool accepted = events_.on_communicate(header, payload);
        if (result != nullptr) {
            VariantInit(result);
            result->vt = VT_BOOL;
            result->boolVal = accepted ? VARIANT_TRUE : VARIANT_FALSE;
        }
        return S_OK;
    }

private:
    KitEvents& events_;
    LONG references_ = 1;
};

class KitClassFactory final : public IClassFactory {
public:
    explicit KitClassFactory(KitEvents& events) : events_(events) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (out == nullptr) {
            return E_POINTER;
        }
        *out = nullptr;
        if (InlineIsEqualGUID(iid, IID_IUnknown) ||
            InlineIsEqualGUID(iid, IID_IClassFactory)) {
            *out = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    // The factory lives as long as the KitServer that owns it.
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid,
                                             void** out) override {
        if (out == nullptr) {
            return E_POINTER;
        }
        *out = nullptr;
        if (outer != nullptr) {
            return CLASS_E_NOAGGREGATION;
        }
        auto* dispatch = new KitDispatch(events_);
        const HRESULT hr = dispatch->QueryInterface(iid, out);
        dispatch->Release();
        return hr;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) override { return S_OK; }

private:
    KitEvents& events_;
};

class Win32KitServer final : public KitServer {
public:
    Win32KitServer(const KitIdentity& identity, KitEvents& events)
        : factory_(events) {
        std::memcpy(&clsid_, identity.clsid, sizeof(clsid_));
    }
    ~Win32KitServer() override { revoke_class_object(); }

    bool register_class_object(long* hresult) override {
        const HRESULT hr = CoRegisterClassObject(
            clsid_, static_cast<IClassFactory*>(&factory_),
            CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie_);
        if (hresult != nullptr) {
            *hresult = hr;
        }
        if (FAILED(hr)) {
            cookie_ = 0;
            return false;
        }
        return true;
    }

    void revoke_class_object() override {
        if (cookie_ != 0) {
            CoRevokeClassObject(cookie_);
            cookie_ = 0;
        }
    }

    void release() override { delete this; }

private:
    KitClassFactory factory_;
    CLSID clsid_{};
    DWORD cookie_ = 0;
};

} // namespace

KitServer* create_kit_server(const KitIdentity& identity, KitEvents& events) {
    return new Win32KitServer(identity, events);
}

} // namespace c1kit
