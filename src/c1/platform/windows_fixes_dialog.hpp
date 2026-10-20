#pragma once

// LibreFixes: Options > LibreFixes, a checklist of the switchable fixes
// (src/c1/common/fix_catalog.hpp) the player can turn on or off while the
// game runs, backed by librefixes.ini.  Only in the LC_COMPAT_RUNTIME
// build, which every shipped LibreCreatures exe is.

#include <afxwin.h>

namespace creatures1::platform {

#if defined(LC_COMPAT_RUNTIME)
// Modal; `parent` is the main frame.  Applies every change (and rewrites
// librefixes.ini) on OK; Cancel leaves the running game as it was.
void show_fixes_dialog(CWnd* parent);
#endif

} // namespace creatures1::platform
