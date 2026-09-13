#pragma once

#include "windows_prelude.hpp"
#include <afxdisp.h>

#include <cstddef>
#include <cstdint>

#include "../application/embedded_kits.hpp"

namespace creatures1::platform {

class C1MainFrame;

// The one call every embedded-kit message in the image is made of: DISPID 1,
// DISPATCH_METHOD, VT_BOOL return, two VTS_PVARIANT arguments of VT_I4.  Shared
// because the original shares it -- four unrelated senders build this same
// record, and the Wine launch path routes through a dedicated adapter at
// 0042d860 that does nothing else.
bool invoke_kit_communicate(COleDispatchDriver& driver,
                            const creatures1::application::EmbeddedKitMessage&
                                message);

// Concrete embedded-kit launch and teardown.  The launch/shutdown ordering is
// application policy in embedded_kits.cpp; COM activation, process handles and
// the toolbar refresh are this boundary.
class WindowsEmbeddedKitHost final
    : public creatures1::application::EmbeddedKitExecutionApi,
      public creatures1::application::EmbeddedKitShutdownApi,
      public creatures1::application::EmbeddedKitMessageApi {
public:
    explicit WindowsEmbeddedKitHost(C1MainFrame& frame) : frame_(frame) {}

    bool running_under_wine() const override;
    bool launch_via_native_com(std::size_t tool_index) override;
    bool launch_via_wine_proxy(std::size_t tool_index) override;

    bool send_kit_message(
        std::size_t tool_index,
        const creatures1::application::EmbeddedKitMessage& message) override;

    bool has_dispatch(std::size_t tool_index) const override;
    void release_dispatch(std::size_t tool_index) override;
    creatures1::application::EmbeddedKitProcessHandle take_process_handle(
        std::size_t tool_index) override;
    bool query_process_exit_code(
        creatures1::application::EmbeddedKitProcessHandle handle,
        std::uint32_t& exit_code) const override;
    void terminate_process(
        creatures1::application::EmbeddedKitProcessHandle handle) override;
    void close_process_handle(
        creatures1::application::EmbeddedKitProcessHandle handle) override;
    void invalidate_toolbar() override;
    bool diagnostics_enabled() const override;
    void log_dispatch_release(std::size_t tool_index,
                              int active_tool_count) override;
    void log_process_termination(std::size_t tool_index) override;

private:
    C1MainFrame& frame_;
};

} // namespace creatures1::platform
