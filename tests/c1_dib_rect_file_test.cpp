// The snapshot file (temp.spr) that the Owner's Kit reads: a top-down back
// buffer is written bottom-up, with exactly the rectangle's rows.
// Build: c++ -std=c++17 -I src/c1 tests/c1_dib_rect_file_test.cpp
//            src/c1/display/bitmap.cpp
#include "display/bitmap.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

using creatures1::display::DibFileSystem;
using creatures1::display::DibOutputFile;

class MemoryFile final : public DibOutputFile {
public:
    explicit MemoryFile(std::vector<std::uint8_t>& out) : out_(out) {}
    void write(const std::uint8_t* bytes, std::size_t count) override {
        out_.insert(out_.end(), bytes, bytes + count);
    }
    void close() override {}

private:
    std::vector<std::uint8_t>& out_;
};

class MemoryFiles final : public DibFileSystem {
public:
    std::unique_ptr<DibOutputFile> open_for_write(std::string_view) override {
        return std::make_unique<MemoryFile>(bytes);
    }
    void report_open_failure(std::string_view) override {}
    std::vector<std::uint8_t> bytes;
};

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

} // namespace

int main() {
    // An 8 x 6 top-down DIB whose row r holds the value 10 + r.
    constexpr int kWidth = 8;
    constexpr int kHeight = 6;
    std::vector<std::uint8_t> pixels(kWidth * kHeight);
    for (int row = 0; row < kHeight; ++row) {
        for (int x = 0; x < kWidth; ++x) {
            pixels[row * kWidth + x] = static_cast<std::uint8_t>(10 + row);
        }
    }
    const creatures1::display::IndexedDib top_down{8, kWidth, -kHeight, pixels.data()};

    // Rows 2..5 (the rectangle touches the bottom of the buffer).
    MemoryFiles files;
    check(creatures1::display::write_dib_rect_to_file(
              top_down, {0, 2, 4, kHeight}, "temp.spr", files),
          "write");
    check(files.bytes.size() == 12 + 4 * 4, "file size");
    // Header: stride, rows, stride; then the rows bottom-up: 15, 14, 13, 12.
    check(files.bytes[0] == 4 && files.bytes[4] == 4 && files.bytes[8] == 4, "header");
    const int expected[] = {15, 14, 13, 12};
    for (int row = 0; row < 4; ++row) {
        check(files.bytes[12 + row * 4] == expected[row], "row order bottom-up");
    }

    // A bottom-up DIB is copied as it is.
    const creatures1::display::IndexedDib bottom_up{8, kWidth, kHeight, pixels.data()};
    MemoryFiles plain;
    creatures1::display::write_dib_rect_to_file(bottom_up, {0, 1, 4, 3}, "temp.spr", plain);
    check(plain.bytes[12] == 11 && plain.bytes[16] == 12, "bottom-up DIB rows kept");

    std::printf(failures == 0 ? "ok\n" : "%d failures\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
