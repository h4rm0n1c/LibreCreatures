#pragma once

#include "windows_prelude.hpp"

#include <cstdint>
#include <string>

namespace creatures1::platform {

// Concrete Win32 registry access behind the semantic settings boundary.
// The abstract policy lives in registry.hpp; only raw HKEY work belongs here.
extern const char kC1UserRegistryPath[];
extern const char kC1MachineRegistryPath[];

bool read_registry_string(HKEY key, const char* value_name,
                          std::string& value);
void write_empty_registry_string(HKEY key, const char* value_name);
bool open_c1_secondary_registry(HKEY& key, REGSAM access);
bool read_c1_window_rect(HKEY key, RECT& rect);
void write_c1_window_rect(HKEY key, const RECT& rect);
bool open_c1_primary_registry(HKEY& key);
bool read_registry_dword(HKEY key, const char* value_name,
                         std::uint32_t& value);
void write_registry_dword(HKEY key, const char* value_name,
                          std::uint32_t value);

} // namespace creatures1::platform
