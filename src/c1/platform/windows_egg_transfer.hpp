#pragma once

#include "windows_shell.hpp"

namespace creatures1::platform {

// File > Export Held Egg... and Import Egg... (not in the 1996 game).  The
// file is one creatures::Egg record written through the document's archive
// machinery.  Export moves the egg the hand holds out of the world; import
// uses the file up and puts a fresh hatchery egg down at spawn, where the
// Hatchery leaves its eggs.

// The egg the hand is carrying, if it carries one.
creatures1::objects::Object* held_egg(C1WindowsDocument& document);

void export_held_egg(C1WindowsDocument& document);
void import_egg(C1WindowsDocument& document);

} // namespace creatures1::platform
