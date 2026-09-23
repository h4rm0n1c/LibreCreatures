#include "windows_shell.hpp"

namespace {

using namespace creatures1::platform;

class C1Application final : public CWinApp {
    DECLARE_DYNCREATE(C1Application)
    DECLARE_MESSAGE_MAP()
    DECLARE_DISPATCH_MAP()
    DECLARE_INTERFACE_MAP()

public:
    C1Application() : CWinApp(nullptr) {
        creatures1::application::initialise_sfc_app_state(
            app_state_, settings_host_);
    }

    BOOL InitInstance() override {
        startup_host_ = std::make_unique<C1StartupHost>(app_state_);
        // Ghidra's recovered call passes zero MRU entries, not the adapter's
        // former placeholder value of four.  It runs inside the startup
        // sequence, so the host drives it rather than InitInstance calling it
        // ahead of OLE initialisation.
        startup_host_->set_profile_settings_loader(
            [this] { LoadStdProfileSettings(0); });
        // A std::exception escaping InitInstance reaches std::terminate and
        // aborts with CRT exit code 3, losing every diagnostic.  MFC only
        // unwinds its own CException, so the clean source's typed errors are
        // reported here instead of killing the process silently.
        try {
            return creatures1::application::initialise_sfc_app(*startup_host_)
                       ? TRUE
                       : FALSE;
        } catch (const std::exception& error) {
            report_startup_failure(error.what());
            return FALSE;
        } catch (CException* mfc_error) {
            char text[512] = {};
            mfc_error->GetErrorMessage(text, sizeof(text));
            mfc_error->Delete();
            report_startup_failure(text);
            return FALSE;
        }
    }

    BOOL OnIdle(LONG idle_cycle_index) override {
        // SFCApp::OnIdle @ 0x0043f360: a positive index records the current
        // cycle, a non-positive one folds it into the smoothed average, and
        // BOTH paths call the base and return TRUE regardless of what the
        // base reports -- which is what keeps idle processing running.
        creatures1::application::update_idle_cadence(
            app_state_.idle_cadence,
            static_cast<std::int32_t>(idle_cycle_index));
        CWinApp::OnIdle(idle_cycle_index);
        return TRUE;
    }

    int ExitInstance() override {
        if (shutdown_host_ == nullptr) {
            shutdown_host_ = std::make_unique<C1ShutdownHost>(*startup_host_, *this);
        }
        creatures1::application::SFCApp::ExitInstance(*shutdown_host_);
        // The frame outlives its window so that CFrameWnd::OnClose can finish
        // on a live object; the shutdown steps above are the last users of it.
        deferred_main_frame_release().reset();
        // The recovered order ends with CWinApp::ExitInstance, which the
        // shutdown host performs; calling it again here would run it twice.
        const int exit_code = shutdown_host_->exit_code();
        startup_host_.reset();
        shutdown_host_.reset();
        return exit_code;
    }

    // SFCApp::OnFileNew @ 0x0043f550 and OnFileOpen @ 0x0043f4f0 stop world
    // timer 1 around the CWinApp handler, clamp the interval and set it again.
    afx_msg void OnFileNewPausingWorld() { run_file_command(false); }
    afx_msg void OnFileOpenPausingWorld() { run_file_command(true); }

private:
    class FileCommandAdapter final
        : public creatures1::application::FileCommandHost {
    public:
        FileCommandAdapter(C1Application& app, bool rearm)
            : app_(app), rearm_(rearm) {}
        void stop_world_update_timer() override {
            if (C1MainFrame* frame = active_main_frame();
                frame != nullptr && frame->GetSafeHwnd() != nullptr) {
                frame->KillTimer(1);
            }
        }
        void invoke_base_file_open() override { app_.CWinApp::OnFileOpen(); }
        void invoke_base_file_new() override { app_.CWinApp::OnFileNew(); }
        void restart_world_update_timer(std::uint32_t interval_ms) override {
            C1MainFrame* frame = active_main_frame();
            if (rearm_ && frame != nullptr && frame->GetSafeHwnd() != nullptr) {
                frame->SetTimer(1, interval_ms, nullptr);
            }
        }

    private:
        C1Application& app_;
        bool rearm_;
    };

    static C1WindowsDocument* active_document() {
        C1MainFrame* frame = active_main_frame();
        return frame == nullptr
                   ? nullptr
                   : DYNAMIC_DOWNCAST(C1WindowsDocument,
                                      frame->GetActiveDocument());
    }

    void run_file_command(bool open) {
        C1WindowsDocument* document = active_document();
        // Native re-arms unconditionally; LibreCreatures leaves a paused
        // world paused when the dialog is cancelled.  A world that does get
        // opened arms its own timer while loading, as before.
        FileCommandAdapter host(*this, document != nullptr && document->world_timer_is_armed());
        std::uint32_t interval_ms =
            document != nullptr ? document->world_update_timer_interval_ms() : 1;
        if (open) {
            creatures1::application::handle_file_open(host, interval_ms);
        } else {
            creatures1::application::handle_file_new(host, interval_ms);
        }
        if (C1WindowsDocument* current = active_document(); current != nullptr) {
            current->set_world_update_timer_interval_ms(interval_ms);
        }
    }

    creatures1::application::SfcAppState app_state_;
    C1SettingsHost settings_host_;
    std::unique_ptr<C1StartupHost> startup_host_;
    std::unique_ptr<C1ShutdownHost> shutdown_host_;
};

C1Application g_c1_application;

// Recovered application framework metadata.  These maps are generated by the
// MFC macros in the original too; no ordinary function bodies are emitted for
// them.  The dispatch map is empty in the target image (its entry array begins
// with the NULL terminator), and the interface map has exactly one part at
// CCmdTarget's automation offset.
IMPLEMENT_DYNCREATE(C1Application, CWinApp)

BEGIN_MESSAGE_MAP(C1Application, CWinApp)
    ON_COMMAND(ID_FILE_NEW, OnFileNewPausingWorld)
    ON_COMMAND(ID_FILE_OPEN, OnFileOpenPausingWorld)
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(C1Application, CWinApp)
END_DISPATCH_MAP()

// {380459A2-3587-11CF-94B8-444553540000}
static const IID IID_ISfcApplication = {
    0x380459a2,
    0x3587,
    0x11cf,
    {0x94, 0xb8, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

BEGIN_INTERFACE_MAP(C1Application, CWinApp)
    INTERFACE_PART(C1Application, IID_ISfcApplication, Dispatch)
END_INTERFACE_MAP()

} // namespace
 // namespace

// The Windows executable entry point belongs to the MFC/CRT host boundary.
// C1 application policy starts at SFCApp::InitInstance; it must not be
// represented by a decompiler-shaped free-function or by a second startup
// implementation in the clean game sources.
//
// MFC supplies that entry point itself, and defining one here instead kept
// the linker from pulling the object that carries it -- and with it the
// application's module state.  What arrived instead was the DLL-flavoured
// state, m_bDLL set: COleObjectFactory::Register then skips
// CoRegisterClassObject entirely, so a running server never publishes its
// class object, and every CoCreateInstance on SFC.OLE tries to launch a
// second copy of the game rather than reaching this one.
