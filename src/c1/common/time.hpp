#pragma once

#include <cstdint>

namespace creatures1::common {

using Time64 = std::int64_t;

// C1's source-level adapter for the platform CRT's 64-bit current-time
// routine. The CRT implementation itself is an external build boundary.
Time64* write_current_time(Time64* output_time);

} // namespace creatures1::common
