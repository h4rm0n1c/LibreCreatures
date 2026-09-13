#pragma once

#include <string>
#include <string_view>

namespace creatures1::platform {

// Concrete Win32 process-environment queries.  The Wine probe boundary lives
// in environment.hpp; these are the plain process/command-line facts.
std::string current_directory_with_separator();
bool command_line_contains(std::string_view command_line,
                           std::string_view option);

} // namespace creatures1::platform
