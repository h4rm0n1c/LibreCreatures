#include "compat.hpp"

#include <bitset>
#include <cstdlib>
#include <cstring>
#include <string>

namespace creatures1::common {

namespace {

constexpr int kRowLimit = 128;

struct NativeRows {
    std::bitset<kRowLimit> rows;
    bool all = false;
};

// C1_NATIVE_ROWS: "all" or rows, comma separated; "-<row>" keeps a row
// fixed after "all".
NativeRows read_native_rows() {
    NativeRows result;
    const char* setting = std::getenv("C1_NATIVE_ROWS");
    if (setting == nullptr) {
        return result;
    }
    const std::string text(setting);
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::string item = text.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start);
        if (item == "all") {
            result.all = true;
            result.rows.set();
        } else if (!item.empty()) {
            const bool keep = item[0] == '-';
            const int row = std::atoi(item.c_str() + (keep ? 1 : 0));
            if (row > 0 && row < kRowLimit) {
                result.rows.set(static_cast<std::size_t>(row), !keep);
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return result;
}

NativeRows& native_rows() {
    static NativeRows value = read_native_rows();
    return value;
}

} // namespace

bool compat_fixed(int row) {
    return row <= 0 || row >= kRowLimit ||
           !native_rows().rows.test(static_cast<std::size_t>(row));
}

#if defined(LC_COMPAT_RUNTIME)
void set_compat_fixed(int row, bool fixed) {
    if (row <= 0 || row >= kRowLimit) {
        return;
    }
    native_rows().rows.set(static_cast<std::size_t>(row), !fixed);
}
#endif

std::string compat_summary() {
#if defined(LC_COMPAT_RUNTIME)
    const NativeRows& native = native_rows();
    if (native.rows.none()) {
        return "none";
    }
    std::string text = native.all ? "all" : "";
    for (int row = 1; row < kRowLimit; ++row) {
        const bool is_native = native.rows.test(static_cast<std::size_t>(row));
        if (native.all ? is_native : !is_native) {
            continue;
        }
        if (!text.empty()) {
            text += ",";
        }
        text += (native.all ? "-" : "") + std::to_string(row);
    }
    return text;
#elif defined(LC_NATIVE_MODE)
    return "all (built native)";
#else
    return "none";
#endif
}

} // namespace creatures1::common
