#include "windows_sfc_ole_host.hpp"

#include "windows_shell.hpp"

#include <cstdint>
#include <cstdio>

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
    DISP_FUNCTION(C1SfcOleAutomation, "RequestMacro", RequestMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "ExecuteMacro", ExecuteMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "CreateMacro", CreateMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "DestroyMacro", DestroyMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "LoadMacro", LoadMacro, VT_BOOL,
                  VTS_PVARIANT VTS_PVARIANT)
    DISP_FUNCTION(C1SfcOleAutomation, "CreateCommand", CreateCommand, VT_I4,
                  VTS_I2)
    DISP_FUNCTION(C1SfcOleAutomation, "DestroyCommand", DestroyCommand,
                  VT_BOOL, VTS_I4)
    DISP_FUNCTION(C1SfcOleAutomation, "LoadCommand", LoadCommand, VT_EMPTY,
                  VTS_I4 VTS_BSTR)
    DISP_FUNCTION(C1SfcOleAutomation, "RequestCommand", RequestCommand,
                  VT_BOOL, VTS_I4 VTS_PBSTR)
    DISP_FUNCTION(C1SfcOleAutomation, "FireCommand", FireCommand, VT_BOOL,
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
creatures1::scripting::MacroHolder* holder_from(const VARIANT* request) {
    return request == nullptr
               ? nullptr
               : reinterpret_cast<creatures1::scripting::MacroHolder*>(
                     static_cast<std::intptr_t>(request->lVal));
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
    response->lVal = static_cast<LONG>(
        reinterpret_cast<std::intptr_t>(create_response.holder));
    return TRUE;
}

BOOL C1SfcOleAutomation::DestroyMacro(VARIANT* request, VARIANT*) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr && sfc_ole->DestroyMacro(holder_from(request))
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::LoadMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr &&
                   script_from(response) != nullptr &&
                   sfc_ole->LoadMacro(holder_from(request),
                                      *script_from(response))
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::RequestMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr &&
                   sfc_ole->RequestMacro(holder_from(request),
                                         script_from(response)) != 0
               ? TRUE
               : FALSE;
}

BOOL C1SfcOleAutomation::ExecuteMacro(VARIANT* request, VARIANT* response) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    return sfc_ole != nullptr &&
                   script_from(response) != nullptr &&
                   sfc_ole->ExecuteMacro(holder_from(request),
                                         *script_from(response)) != 0
               ? TRUE
               : FALSE;
}

long C1SfcOleAutomation::CreateCommand(short execution_mode) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return 0;
    }
    return static_cast<long>(reinterpret_cast<std::intptr_t>(
        sfc_ole->CreateCommand(static_cast<std::uint16_t>(execution_mode))));
}

BOOL C1SfcOleAutomation::DestroyCommand(long holder) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return FALSE;
    }
    return sfc_ole->DestroyCommand(
               reinterpret_cast<creatures1::scripting::MacroHolder*>(
                   static_cast<std::intptr_t>(holder)))
               ? TRUE
               : FALSE;
}

void C1SfcOleAutomation::LoadCommand(long holder, LPCTSTR command_text) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr || command_text == nullptr) {
        return;
    }
    sfc_ole->LoadCommand(reinterpret_cast<creatures1::scripting::MacroHolder*>(
                             static_cast<std::intptr_t>(holder)),
                         std::string_view(command_text));
}

BOOL C1SfcOleAutomation::RequestCommand(long holder, BSTR* output) {
    creatures1::application::CSfcOLE* sfc_ole = server();
    if (sfc_ole == nullptr) {
        return FALSE;
    }
    return sfc_ole->RequestCommand(
               reinterpret_cast<creatures1::scripting::MacroHolder*>(
                   static_cast<std::intptr_t>(holder)),
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

void register_sfc_ole_object_factory() {
    // The native registration is a CRT static initialiser holding a
    // COleObjectFactory for the process lifetime, torn down through atexit.
    static COleObjectFactory factory(
        kSfcOleClsid, RUNTIME_CLASS(C1SfcOleAutomation), FALSE, "SFC.OLE");
    static_cast<void>(factory);
}

} // namespace creatures1::platform
