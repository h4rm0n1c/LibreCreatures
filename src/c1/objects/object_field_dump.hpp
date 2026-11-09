#pragma once

// Not native: the protected state of an Object that the load equivalence
// test's dump reads (the O column of load_dump_objects.inc).  Object names
// this class as a friend; nothing else uses it.

#include "object.hpp"

#include <cstdint>

namespace creatures1::objects {

class ObjectFieldDump {
public:
    static std::int32_t timer_period(const Object& o) { return o.timer_period(); }
};

} // namespace creatures1::objects
