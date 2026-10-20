#include "windows_sfc_ole_host.hpp"

#include "windows_shell.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace creatures1::platform {

// {77C733E1-6797-11CF-BBF2-0020AF71E433}
static const CLSID kSfcOleClsid = {
    0x77c733e1,
    0x6797,
    0x11cf,
    {0xbb, 0xf2, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};

IMPLEMENT_DYNCREATE(C1SfcOleAutomation, CCmdTarget)

C1SfcOleAutomation::C1SfcOleAutomation() { EnableAutomation(); }

C1SfcOleAutomation::~C1SfcOleAutomation() = default;

void C1SfcOleAutomation::OnFinalRelease() {
    // The recovered server releases the application lock when its last
    // automation reference goes away.
    CCmdTarget::OnFinalRelease();
}

// Entries and order recovered from the dispatch map at 0x004584f8, whose
// entry array at 0x00458398 gives every method lDispID -1: MFC therefore
// assigns DISPIDs by position, so the order below is load bearing.
BEGIN_DISPATCH_MAP(C1SfcOleAutomation, CCmdTarget)
    DISP_FUNCTION(C1SfcOleAutomation, "RequestMacro", C1SfcOleAutomation::RequestMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "ExecuteMacro", C1SfcOleAutomation::ExecuteMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "CreateMacro", C1SfcOleAutomation::CreateMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "DestroyMacro", C1SfcOleAutomation::DestroyMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "LoadMacro", C1SfcOleAutomation::LoadMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "CreateCommand", C1SfcOleAutomation::CreateCommand, VT_I4,
                  VTS_I2)
    DISP_FUNCTION(C1SfcOleAutomation, "DestroyCommand", C1SfcOleAutomation::DestroyCommand,
                  VT_BOOL, VTS_I4)
    DISP_FUNCTION(C1SfcOleAutomation, "LoadCommand", C1SfcOleAutomation::LoadCommand, VT_EMPTY,
                  VTS_I4 VTS_BSTR)
    DISP_FUNCTION(C1SfcOleAutomation, "RequestCommand", C1SfcOleAutomation::RequestCommand,
                  VT_BOOL, VTS_I4 VTS_PBSTR)
    DISP_FUNCTION(C1SfcOleAutomation, "FireCommand", C1SfcOleAutomation::FireCommand, VT_BOOL,
                  VTS_I2 VTS_BSTR VTS_PBSTR)
END_DISPATCH_MAP()

BEGIN_INTERFACE_MAP(C1SfcOleAutomation, CCmdTarget)
    INTERFACE_PART(C1SfcOleAutomation, IID_IDispatch, Dispatch)
END_INTERFACE_MAP()


std::unique_ptr<creatures1::scripting::MacroHolder>
WindowsSfcOleHost::create_macro_holder(
    creatures1::scripting::MacroExecutionMode execution_mode) {
    return std::make_unique<creatures1::scripting::MacroHolder>(execution_mode,
                                                                *this);
}

bool WindowsSfcOleHost::debug_console_exists() const {
    // The recovered guard is a null test on the debug console dialog; this
    // process has no console bound, so the policy's logging stays silent
    // rather than being routed somewhere the native never wrote to.
    return false;
}

void WindowsSfcOleHost::log(std::string_view message) {
    const std::string text(message);
    ::OutputDebugStringA(text.c_str());
}

std::string WindowsSfcOleHost::ansi_text_from_bstr(const char* text) const {
    // The macro family's "BSTR" is an ANSI buffer the caller owns.
    // CSfcOLE::LoadMacro @ 0x0042fb60 reads it straight through as char*, so
    // there is nothing to convert -- and converting it as wide text, which is
    // what the VT_BSTR tag suggests, garbles every script a kit sends.
    return text == nullptr ? std::string() : std::string(text);
}

void WindowsSfcOleHost::assign_output_bstr(std::string_view text,
                                           wchar_t** output_bstr) {
    if (output_bstr == nullptr) {
        return;
    }
    if (*output_bstr != nullptr) {
        ::SysFreeString(*output_bstr);
        *output_bstr = nullptr;
    }
    const std::string source(text);
    const int length = ::MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1,
                                             nullptr, 0);
    if (length <= 0) {
        *output_bstr = ::SysAllocString(L"");
        return;
    }
    std::wstring wide(static_cast<std::size_t>(length - 1), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, wide.data(), length);
    *output_bstr = ::SysAllocString(wide.c_str());
}

void WindowsSfcOleHost::unlock_ole_application() { AfxOleUnlockApp(); }

creatures1::application::CSfcOLE* C1SfcOleAutomation::server() {
    C1MainFrame* frame = active_main_frame();
    C1WindowsDocument* document =
        frame == nullptr
            ? nullptr
            : DYNAMIC_DOWNCAST(C1WindowsDocument, frame->GetActiveDocument());
    if (document == nullptr) {
        return nullptr;
    }
    // The macro chain is document-scoped, so a closed and reopened document
    // gets a fresh server rather than one holding a dangling host.
    if (server_ == nullptr || bound_document_ != document) {
        host_ = std::make_unique<WindowsSfcOleHost>(*document);
        server_ = std::make_unique<creatures1::application::CSfcOLE>(*host_);
        bound_document_ = document;
    }
    return server_.get();
}

namespace {

// The macro family's two arguments are VARIANTs: the type is the first field
// and the value sits at offset eight.  A kit sets the request to VT_I4
// carrying its macro holder and the response to VT_BSTR carrying its own
// ANSI command buffer, which the game also writes the reply back into.
creatures1::scripting::MacroHolder* holder_from(
    const creatures1::application::CSfcOLE& sfc_ole, const VARIANT* request) {
    return request == nullptr
               ? nullptr
               : sfc_ole.holder_at(static_cast<std::uint32_t>(request->lVal));
}

// OleScriptVariant is a layout mirror of VARIANT -- tag at offset 0, value at
// offset 8 -- so the automation VARIANT is reinterpreted in place rather than
// copied.  Copying is what broke RequestMacro: the callback publishes its reply
// by replacing the BSTR in the variant it is given, and a copy sends that reply
// into a stack temporary the caller never sees.
static_assert(sizeof(creatures1::application::OleScriptVariant) ==
                  sizeof(VARIANT),
              "OleScriptVariant must overlay a VARIANT exactly");
static_assert(offsetof(creatures1::application::OleScriptVariant, bstr_value) ==
                  8,
              "the VARIANT value field sits at offset 8");

creatures1::application::OleScriptVariant* script_from(VARIANT* response) {
    return reinterpret_cast<creatures1::application::OleScriptVariant*>(
        response);
}

} // namespace

BOOL C1SfcOleAutomation::CreateMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr || request == nullptr || response == nullptr) {
        return FALSE;
    }
    creatures1::application::MacroCreateRequest create_request;
    create_request.kind =
        static_cast<creatures1::application::MacroRequestKind>(request->vt);
    create_request.execution_mode = static_cast<std::uint16_t>(request->iVal);
    creatures1::application::MacroCreateResponse create_response;
    if (!sfc_ole->CreateMacro(create_request, create_response)) {
        return FALSE;
    }
    // The holder goes back the way it came in: tagged VT_I4, value at eight.
    response->vt = static_cast<VARTYPE>(create_response.kind);
    response->lVal = static_cast<LONG>(sfc_ole->handle_of(create_response.holder));
    return TRUE;
}

BOOL C1SfcOleAutomation::DestroyMacro(VARIANT* request, VARIANT*) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr && sfc_ole->DestroyMacro(holder_from(*sfc_ole, request))
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::LoadMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr &&
                   script_from(response) != nullptr &&
                   sfc_ole->LoadMacro(holder_from(*sfc_ole, request),
                                      *script_from(response))
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::RequestMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr &&
                   sfc_ole->RequestMacro(holder_from(*sfc_ole, request),
                                         script_from(response)) != 0
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::ExecuteMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr &&
                   script_from(response) != nullptr &&
                   sfc_ole->ExecuteMacro(holder_from(*sfc_ole, request),
                                         *script_from(response)) != 0
               ? TRUE
               : FALSE;
}

long C1SfcOleAutomation::CreateCommand(short execution_mode) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return 0;
    }
    return static_cast<long>(sfc_ole->handle_of(
        sfc_ole->CreateCommand(static_cast<std::uint16_t>(execution_mode))));
}

BOOL C1SfcOleAutomation::DestroyCommand(long holder) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return FALSE;
    }
    return sfc_ole->DestroyCommand(
               sfc_ole->holder_at(static_cast<std::uint32_t>(holder)))
               ? TRUE
               : FALSE;
}

void C1SfcOleAutomation::LoadCommand(long holder, LPCTSTR command_text) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr || command_text == nullptr) {
        return;
    }
    sfc_ole->LoadCommand(sfc_ole->holder_at(static_cast<std::uint32_t>(holder)),
                         std::string_view(command_text));
}

BOOL C1SfcOleAutomation::RequestCommand(long holder, BSTR* output) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return FALSE;
    }
    return sfc_ole->RequestCommand(
               sfc_ole->holder_at(static_cast<std::uint32_t>(holder)),
               output) != 0
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::FireCommand(short execution_mode,
                                     LPCTSTR command_text, BSTR* output) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return FALSE;
    }
    return sfc_ole->FireCommand(
               static_cast<std::uint16_t>(execution_mode),
               command_text == nullptr ? std::string_view()
                                       : std::string_view(command_text),
               output)
               ? TRUE
               : FALSE;
}

namespace {

// One IDispatch call as c1kitlib makes it: two VARIANT* arguments by
// reference, in reverse order in DISPPARAMS; true when the method said TRUE.
bool invoke_by_reference(IDispatch& dispatch, DISPID id, VARIANT* request,
                         VARIANT* response) {
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
    const HRESULT hr = dispatch.Invoke(id, IID_NULL, LOCALE_USER_DEFAULT,
                                       DISPATCH_METHOD, &parameters, &result,
                                       nullptr, nullptr);
    const bool yes = SUCCEEDED(hr) && result.vt == VT_BOOL &&
                     result.boolVal != VARIANT_FALSE;
    VariantClear(&result);
    return yes;
}

// The kits' command buffer: a byte-length BSTR holding ANSI text.
void set_script(VARIANT& buffer, const char* script) {
    VariantClear(&buffer);
    const UINT bytes = 0x400;
    buffer.vt = VT_BSTR;
    buffer.bstrVal = SysAllocStringByteLen(nullptr, bytes);
    std::memset(buffer.bstrVal, 0, bytes);
    std::memcpy(buffer.bstrVal, script, std::strlen(script));
}

VARIANT holder_variant(LONG handle) {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = handle;
    return value;
}

}  // namespace

bool run_sfc_ole_self_test(const char* out_path) {
    // DISPIDs by position in the dispatch map (c1kit::SfcDispatchId).
    constexpr DISPID kRequestMacro = 1, kExecuteMacro = 2, kCreateMacro = 3,
                     kDestroyMacro = 4, kLoadMacro = 5;
    std::FILE* out = std::fopen(out_path, "w");
    if (out == nullptr) {
        return false;
    }
    bool all = true;
    const auto step = [&](const char* name, bool ok, const char* detail) {
        std::fprintf(out, "%s %s%s%s\n", ok ? "ok  " : "FAIL", name,
                     detail[0] != '\0' ? ": " : "", detail);
        all = all && ok;
    };
    auto* automation =
        static_cast<C1SfcOleAutomation*>(RUNTIME_CLASS(C1SfcOleAutomation)->CreateObject());
    IDispatch* dispatch =
        automation == nullptr ? nullptr : automation->GetIDispatch(FALSE);
    if (dispatch == nullptr) {
        step("dispatch", false, "no IDispatch");
        std::fclose(out);
        return false;
    }
    char detail[256];

    // A query, as Observation polls: create, load, request, destroy.
    VARIANT request;
    VARIANT response;
    VariantInit(&request);
    VariantInit(&response);
    request.vt = VT_I2;
    request.iVal = 1;  // kMacroModeQuery
    response.vt = VT_I4;
    bool ok = invoke_by_reference(*dispatch, kCreateMacro, &request, &response);
    const LONG query_holder = response.vt == VT_I4 ? response.lVal : 0;
    std::snprintf(detail, sizeof detail, "holder %08lX",
                  static_cast<unsigned long>(query_holder));
    step("CreateMacro (query)", ok && query_holder != 0, detail);

    VARIANT buffer;
    VariantInit(&buffer);
    set_script(buffer, "dde: putv 4242");
    VARIANT holder = holder_variant(query_holder);
    step("LoadMacro", invoke_by_reference(*dispatch, kLoadMacro, &holder, &buffer), "");
    holder = holder_variant(query_holder);
    ok = invoke_by_reference(*dispatch, kRequestMacro, &holder, &buffer);
    const char* reply = ok && buffer.vt == VT_BSTR && buffer.bstrVal != nullptr
                            ? reinterpret_cast<const char*>(buffer.bstrVal)
                            : "";
    std::snprintf(detail, sizeof detail, "reply \"%.200s\"", reply);
    step("RequestMacro", ok && std::strcmp(reply, "4242") == 0, detail);
    holder = holder_variant(query_holder);
    VARIANT unused;
    VariantInit(&unused);
    step("DestroyMacro (query)",
         invoke_by_reference(*dispatch, kDestroyMacro, &holder, &unused), "");
    holder = holder_variant(query_holder);
    step("DestroyMacro again is refused",
         !invoke_by_reference(*dispatch, kDestroyMacro, &holder, &unused), "");

    // A scheduled script, as the Hatchery hands over an egg.
    VariantInit(&request);
    VariantInit(&response);
    request.vt = VT_I2;
    request.iVal = 0;  // kMacroModeSchedule
    response.vt = VT_I4;
    ok = invoke_by_reference(*dispatch, kCreateMacro, &request, &response);
    const LONG schedule_holder = response.vt == VT_I4 ? response.lVal : 0;
    std::snprintf(detail, sizeof detail, "holder %08lX",
                  static_cast<unsigned long>(schedule_holder));
    step("CreateMacro (schedule)", ok && schedule_holder != 0, detail);
    set_script(buffer, "setv norn 0");
    holder = holder_variant(schedule_holder);
    step("ExecuteMacro", invoke_by_reference(*dispatch, kExecuteMacro, &holder, &buffer), "");
    holder = holder_variant(schedule_holder);
    step("DestroyMacro (schedule)",
         invoke_by_reference(*dispatch, kDestroyMacro, &holder, &unused), "");

    // The Command family, which passes the holder as a long.
    const long command = automation->CreateCommand(1);
    std::snprintf(detail, sizeof detail, "holder %08lX",
                  static_cast<unsigned long>(command));
    step("CreateCommand", command != 0, detail);
    automation->LoadCommand(command, _T("dde: putv 77"));
    BSTR output = nullptr;
    ok = automation->RequestCommand(command, &output) != FALSE;
    char narrow[256] = {};
    if (output != nullptr) {
        ::WideCharToMultiByte(CP_ACP, 0, output, -1, narrow, sizeof narrow, nullptr, nullptr);
        ::SysFreeString(output);
    }
    std::snprintf(detail, sizeof detail, "reply \"%.200s\"", narrow);
    step("RequestCommand", ok && std::strcmp(narrow, "77") == 0, detail);
    step("DestroyCommand", automation->DestroyCommand(command) != FALSE, "");

    VariantClear(&buffer);
    dispatch->Release();  // CreateObject's reference: this deletes it
    std::fprintf(out, "%s\n", all ? "PASS" : "FAILED");
    std::fclose(out);
    return all;
}

void register_sfc_ole_object_factory() {
    // The native registration is a CRT static initialiser holding a
    // COleObjectFactory for the process lifetime, torn down through atexit.
    static COleObjectFactory factory(
        kSfcOleClsid, RUNTIME_CLASS(C1SfcOleAutomation), FALSE, "SFC.OLE");
    static_cast<void>(factory);
}

} // namespace creatures1::platform
