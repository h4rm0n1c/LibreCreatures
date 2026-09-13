#pragma once

#include "windows_prelude.hpp"

#include <string>
#include <string_view>

#include "com.hpp"

namespace creatures1::platform {

// Concrete COM/registry resolution behind ComLocalServerApi: ProgID to CLSID,
// then HKCR\CLSID\{id}\LocalServer32, then the quoted or switch-terminated
// server path that the embedded-kit launcher needs.
class WindowsComLocalServer final : public ComLocalServerApi {
public:
    bool resolve_local_server_path(std::string_view prog_id,
                                   std::string& server_path) override;
};

} // namespace creatures1::platform
