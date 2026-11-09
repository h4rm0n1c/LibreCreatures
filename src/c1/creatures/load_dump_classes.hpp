#pragma once

// Not native: the load equivalence test's dump (load_dump_objects.inc)
// names every class as objects::<class>.  These two live elsewhere.

#include "../brain/blackboard.hpp"
#include "../ui/tools.hpp"

namespace creatures1::objects {

using Blackboard = brain::Blackboard;
using PointerTool = ui::PointerTool;

} // namespace creatures1::objects
