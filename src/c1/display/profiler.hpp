#pragma once

#include <string>

namespace creatures1::display {

// Report-local XML escaping.  The profiler report escapes the four markup
// characters observed in the executable and leaves all other input bytes
// unchanged, including apostrophes and non-ASCII bytes.
std::string EscapeXmlForProfilerReport(const std::string& text);

}  // namespace creatures1::display
