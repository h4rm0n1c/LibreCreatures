#pragma once

// Not native: the protected state of an Object that the load equivalence
// test's dump reads (the O column of load_dump_objects.inc), and that the
// object sound test (tests/equivalence/object_sound_equivalence.cpp) sets.
// Object names this class as a friend; nothing else uses it.

#include "object.hpp"

#include <cstdint>

namespace creatures1::objects {

class ObjectFieldDump {
public:
    static std::int32_t timer_period(const Object& o) { return o.timer_period(); }
    static void set_continuous_sound_state(Object& o, std::int32_t handle,
                                           std::uint32_t sound_id, bool persist) {
        o.sound_handle_ = handle;
        o.continuous_sound_descriptor_ = sound_id;
        o.continuous_sound_persist_flag_ = persist;
    }
};

} // namespace creatures1::objects
