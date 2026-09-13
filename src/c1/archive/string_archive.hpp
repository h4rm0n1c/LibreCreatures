#pragma once

#include <string>
#include <string_view>

namespace creatures1::archive {

// C1's persistence code depends on the string-record operations of the
// archive stream.  The MFC CArchive buffer, encoding, and exception policy
// are supplied by the platform integration layer.
class StringArchive {
public:
    virtual ~StringArchive() = default;

    virtual bool is_loading() const = 0;
    virtual std::string read_string() = 0;
    virtual void write_string(std::string_view value) = 0;
};

} // namespace creatures1::archive
