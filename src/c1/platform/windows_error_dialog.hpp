#pragma once

// Not native: an error the player may need to pass on.  A warning, one
// summary line, then the details in a read-only box they can select, with a
// Copy button that puts the summary and the details on the clipboard.

#include <afxwin.h>

#include <string>

namespace creatures1::platform {

void show_error_report(CWnd* parent, const std::string& title,
                       const std::string& summary, const std::string& details);

} // namespace creatures1::platform
