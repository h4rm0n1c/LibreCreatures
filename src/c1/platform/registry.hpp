#pragma once

#include <cstdint>
#include <string_view>

namespace creatures1::platform {

struct DwordPair {
    std::uint32_t first;
    std::uint32_t second;
};

// The implementation owns HKEY handles, registry paths, and Reg* calls.
// These operations are the narrow platform boundary used by C1 settings and
// embedded-tool discovery.
class RegistryApi {
public:
    virtual ~RegistryApi() = default;

    virtual void close_creatures_key_pair() = 0;
    virtual void read_or_initialize_dword_pair(std::string_view value_name,
                                               DwordPair& value,
                                               std::uint32_t first_default,
                                               std::uint32_t second_default) = 0;
    virtual void open_creatures_keys(std::string_view subkey) = 0;
};

void close_registry_key_pair(RegistryApi& api);

void read_or_initialize_registry_dword_pair(RegistryApi& api,
                                            std::string_view value_name,
                                            DwordPair& value,
                                            std::uint32_t first_default,
                                            std::uint32_t second_default);

void open_creatures_registry_keys(RegistryApi& api, std::string_view subkey);

} // namespace creatures1::platform
