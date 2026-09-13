#include "profiler.hpp"

namespace creatures1::display {

std::string EscapeXmlForProfilerReport(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text) {
        switch (character) {
        case '"':
            escaped.append("&quot;");
            break;
        case '&':
            escaped.append("&amp;");
            break;
        case '<':
            escaped.append("&lt;");
            break;
        case '>':
            escaped.append("&gt;");
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

}  // namespace creatures1::display
