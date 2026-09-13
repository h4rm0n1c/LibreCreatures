#include "registry.hpp"

namespace creatures1::platform {

void close_registry_key_pair(RegistryApi& api) {
    api.close_creatures_key_pair();
}

void read_or_initialize_registry_dword_pair(RegistryApi& api,
                                            std::string_view value_name,
                                            DwordPair& value,
                                            std::uint32_t first_default,
                                            std::uint32_t second_default) {
    api.read_or_initialize_dword_pair(value_name, value, first_default,
                                      second_default);
}

void open_creatures_registry_keys(RegistryApi& api, std::string_view subkey) {
    api.open_creatures_keys(subkey);
}

} // namespace creatures1::platform
