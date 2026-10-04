#pragma once

// Not native: one copy of the game per world.  Two copies running the same
// World.sfc each save over the other's world, and one can hold the file so
// the other cannot save at all (a copy left hung by a crash did exactly
// that).  A copy claims its world before loading it; a world another copy
// has claimed is refused, with the reason.

#include <string>

namespace creatures1::platform {

// Claims `world_file` for this process, giving up any world it claimed
// before.  False when another running copy of the game holds it.  A world
// that cannot be checked (no name could be made) is allowed.
bool claim_world(const std::string& world_file);

// The report shown when claim_world refuses a world.
void report_world_in_use(const std::string& world_file);

} // namespace creatures1::platform
