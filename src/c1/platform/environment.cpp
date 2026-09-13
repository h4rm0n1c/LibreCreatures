#include "environment.hpp"

namespace creatures1::platform {

bool WineEnvironment::is_running_under_wine() const {
    if (cached_result_ < 0) {
        cached_result_ = probe_.wine_get_version_is_available() ? 1 : 0;
    }
    return cached_result_ == 1;
}

} // namespace creatures1::platform
