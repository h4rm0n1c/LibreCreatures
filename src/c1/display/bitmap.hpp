#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace creatures1::display {

struct PixelRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

// The bitmap adapter owns the Windows/MFC representation.  C1 code only
// needs the fields that the packed-DIB export actually consumes.
struct IndexedDib {
    std::uint16_t bits_per_pixel = 0;
    int width = 0;
    int height = 0;
    const std::uint8_t* pixels = nullptr;
};

class DibOutputFile {
public:
    virtual ~DibOutputFile() = default;
    virtual void write(const std::uint8_t* bytes, std::size_t byte_count) = 0;
    virtual void close() = 0;
};

class DibFileSystem {
public:
    virtual ~DibFileSystem() = default;
    // A concrete adapter may use MFC CFile internally.  Open/reporting
    // failure stays outside the C1 file-format operation.
    virtual std::unique_ptr<DibOutputFile> open_for_write(
        std::string_view path) = 0;
    virtual void report_open_failure(std::string_view path) = 0;
};

bool write_dib_rect_to_file(const IndexedDib& dib,
                            const PixelRect& source_rect,
                            std::string_view output_path,
                            DibFileSystem& files);

} // namespace creatures1::display
