#pragma once

#include <string>
#include <string_view>

namespace creatures1::platform {

// The concrete implementation owns CLSID conversion, HKCR/LocalServer32
// access, quoted-command-line parsing, and COM allocator cleanup.
class ComLocalServerApi {
public:
    virtual ~ComLocalServerApi() = default;

    virtual bool resolve_local_server_path(std::string_view prog_id,
                                           std::string& server_path) = 0;
};

bool resolve_com_progid_local_server_path(ComLocalServerApi& api,
                                          std::string_view prog_id,
                                          std::string& server_path);

} // namespace creatures1::platform
