#pragma once

#include "windows_prelude.hpp"
#include <afxdisp.h>

#include "windows_macro_host.hpp"
#include "../application/sfc_ole.hpp"

#include <memory>
#include <string>

namespace creatures1::platform {

// Recovered CSfcOLE automation server.  External kits find the game through
// ProgID "SFC.OLE" and CLSID {77C733E1-6797-11CF-BBF2-0020AF71E433}, both read
// from RegisterSfcOleObjectFactory @ 00401380.
//
// The recovered class exposes ten automation methods, whose translated policy
// already lives in application/sfc_ole.cpp as creatures1::application::CSfcOLE.
// Binding those bodies needs a CSfcOLEHost, which requires the macro execution
// host - still unbound - so this supplies the framework identity and factory
// registration only.
// Supplies CSfcOLE with its host.  The MacroHolderHost half is the existing
// macro host, inherited rather than forwarded; only the six OLE-specific
// operations are added here.
class WindowsSfcOleHost final : public WindowsMacroHost,
                                public creatures1::application::CSfcOLEHost {
public:
    explicit WindowsSfcOleHost(C1WindowsDocument& document)
        : WindowsMacroHost(document) {}

    std::unique_ptr<creatures1::scripting::MacroHolder> create_macro_holder(
        creatures1::scripting::MacroExecutionMode execution_mode) override;
    bool debug_console_exists() const override;
    void log(std::string_view message) override;
    std::string ansi_text_from_bstr(const char* text) const override;
    void assign_output_bstr(std::string_view text,
                            wchar_t** output_bstr) override;
    void unlock_ole_application() override;
};

class C1SfcOleAutomation : public CCmdTarget {
    DECLARE_DYNCREATE(C1SfcOleAutomation)
    DECLARE_DISPATCH_MAP()
    DECLARE_INTERFACE_MAP()

public:
    C1SfcOleAutomation();
    ~C1SfcOleAutomation() override;

protected:
    void OnFinalRelease() override;

    // Declared in the recovered dispatch map's order: the entries at
    // 0x00458398 all carry lDispID -1, so MFC assigns DISPIDs by position and
    // the order below is what gives FireCommand its DISPID of 10.
    BOOL RequestMacro(VARIANT* request, VARIANT* response);
    BOOL ExecuteMacro(VARIANT* request, VARIANT* response);
    BOOL CreateMacro(VARIANT* request, VARIANT* response);
    BOOL DestroyMacro(VARIANT* request, VARIANT* response);
    BOOL LoadMacro(VARIANT* request, VARIANT* response);
    long CreateCommand(short execution_mode);
    BOOL DestroyCommand(long holder);
    void LoadCommand(long holder, LPCTSTR command_text);
    BOOL RequestCommand(long holder, BSTR* output);
    BOOL FireCommand(short execution_mode, LPCTSTR command_text,
                     BSTR* output);

private:
    creatures1::application::CSfcOLE* server();

    std::unique_ptr<WindowsSfcOleHost> host_;
    std::unique_ptr<creatures1::application::CSfcOLE> server_;
    C1WindowsDocument* bound_document_ = nullptr;
};

// Constructing this registers the SFC.OLE factory for the process lifetime,
// matching the native static COleObjectFactory the CRT initialiser creates.
void register_sfc_ole_object_factory();

} // namespace creatures1::platform
