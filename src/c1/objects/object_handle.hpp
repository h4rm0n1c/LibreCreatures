#pragma once

#include <cstdint>

namespace creatures1::objects {

class Object;

// An object as a 32-bit value: what CAOS variables, `targ` and its kin, the
// kits and the lab's records see.  Creatures 1 is a 32-bit program and keeps
// the object's address in these values, so a script that prints `targ`
// prints an address, and a world saves whatever addresses its scripts held.
//
// Every conversion between an Object and such a value goes through these two
// functions.  In the 32-bit build the handle is the address, as in the
// original.  A 64-bit build cannot fit an address in 32 bits, and will map
// handles to objects in a registry behind the same two functions.
using ObjectHandle = std::uint32_t;

#if UINTPTR_MAX == 0xffffffffu

inline ObjectHandle handle_of(const Object* object) noexcept {
    return static_cast<ObjectHandle>(reinterpret_cast<std::uintptr_t>(object));
}

// The object a handle names, or nullptr for 0.  A handle that names no live
// object is not detected here: callers check the result with the host's
// is_live_object, as the original checks its pointers.
inline Object* object_at_handle(ObjectHandle handle) noexcept {
    return reinterpret_cast<Object*>(static_cast<std::uintptr_t>(handle));
}

#else
#error "64-bit builds need the object handle registry, which does not exist yet (object_handle.hpp)"
#endif

}  // namespace creatures1::objects
