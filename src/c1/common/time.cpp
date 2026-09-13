#include "time.hpp"

#include <ctime>

namespace creatures1::common {

Time64* write_current_time(Time64* output_time) {
#if defined(_MSC_VER)
    // This is the imported CRT routine used by the original executable.
    *output_time = static_cast<Time64>(::_time64(nullptr));
#else
    // The non-MSVC syntax harness has no _time64 import; std::time supplies
    // the same current-calendar-time contract for that build.
    *output_time = static_cast<Time64>(std::time(nullptr));
#endif
    return output_time;
}

} // namespace creatures1::common
