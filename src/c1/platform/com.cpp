#include "com.hpp"

namespace creatures1::platform {

bool resolve_com_progid_local_server_path(ComLocalServerApi& api,
                                          std::string_view prog_id,
                                          std::string& server_path) {
    server_path.clear();
    return api.resolve_local_server_path(prog_id, server_path);
}

} // namespace creatures1::platform
