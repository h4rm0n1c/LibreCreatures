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
// original.  A 64-bit build cannot fit an address in 32 bits: it gives each
// object a handle from a registry (object_handle.cpp) the first time one is
// needed, and the object's destructor takes it back.
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

// The address is the handle, so there is nothing to take back.
inline void forget_object_handle(const Object*) noexcept {}

#else

// 64-bit: object_handle.cpp.  handle_of gives an object the next free handle
// the first time; object_at_handle gives nullptr for 0 and for a handle that
// names no object (the registry knows which objects are alive, so callers
// still check with is_live_object as the original does, but no handle
// points at freed memory).
ObjectHandle handle_of(const Object* object);
Object* object_at_handle(ObjectHandle handle);
// Called by Object's destructor.
void forget_object_handle(const Object* object) noexcept;

#endif

}  // namespace creatures1::objects
