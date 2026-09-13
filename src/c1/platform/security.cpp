#include "security.hpp"

namespace creatures1::platform {

bool build_current_user_security_descriptor(NamedPipeSecurityApi& api) {
    return api.ensure_current_user_security_attributes();
}

} // namespace creatures1::platform
