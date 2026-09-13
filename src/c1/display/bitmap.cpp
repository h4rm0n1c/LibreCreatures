#include "bitmap.hpp"

#include <cstring>
#include <limits>
#include <new>

namespace creatures1::display {
namespace {

constexpr std::size_t kPackedDibHeaderSize = 0x0c;

int aligned_row_stride(int width) {
    return (width + 3) & ~3;
}

void write_little_endian_u32(std::uint8_t* destination,
                             std::uint32_t value) {
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8);
    destination[2] = static_cast<std::uint8_t>(value >> 16);
    destination[3] = static_cast<std::uint8_t>(value >> 24);
}

} // namespace

bool write_dib_rect_to_file(const IndexedDib& dib,
                            const PixelRect& source_rect,
                            std::string_view output_path,
                            DibFileSystem& files) {
    if (dib.bits_per_pixel != 8) {
        return false;
    }

    const std::uint32_t row_count = static_cast<std::uint32_t>(
        source_rect.bottom - source_rect.top);
    const std::size_t destination_row_stride = static_cast<std::size_t>(
        aligned_row_stride(source_rect.right - source_rect.left));
    const std::size_t pixel_bytes =
        static_cast<std::size_t>(row_count) * destination_row_stride;
    if (pixel_bytes > std::numeric_limits<std::size_t>::max() -
                          kPackedDibHeaderSize) {
        return false;
    }
    const std::size_t file_size = kPackedDibHeaderSize + pixel_bytes;

    std::unique_ptr<std::uint8_t[]> file_buffer(
        new (std::nothrow) std::uint8_t[file_size]);
    if (file_buffer == nullptr) {
        return false;
    }

    const std::uint32_t output_stride =
        static_cast<std::uint32_t>(destination_row_stride);
    write_little_endian_u32(file_buffer.get(), output_stride);
    write_little_endian_u32(file_buffer.get() + 4, row_count);
    write_little_endian_u32(file_buffer.get() + 8, output_stride);

    const std::ptrdiff_t source_row_stride =
        aligned_row_stride(dib.width);
    const std::uint8_t* source_row =
        dib.pixels + static_cast<std::ptrdiff_t>(source_rect.top) *
                         source_row_stride + source_rect.left;
    std::ptrdiff_t row_step = source_row_stride;
    if (dib.height < 0) {
        source_row += static_cast<std::ptrdiff_t>(source_row_stride) *
                      row_count;
        row_step = -source_row_stride;
    }

    std::uint8_t* destination_row = file_buffer.get() +
                                     kPackedDibHeaderSize;
    for (std::uint32_t row = 0; row < row_count; ++row) {
        std::memcpy(destination_row, source_row, destination_row_stride);
        source_row += row_step;
        destination_row += destination_row_stride;
    }

    std::unique_ptr<DibOutputFile> output =
        files.open_for_write(output_path);
    if (output == nullptr) {
        files.report_open_failure(output_path);
        return false;
    }

    output->write(file_buffer.get(), file_size);
    output->close();
    return true;
}

} // namespace creatures1::display
