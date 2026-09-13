#include "formatting.hpp"

namespace creatures1::platform {

int secure_vsnprintf(const SecureFormattingApi& api,
                     char* output,
                     std::size_t output_capacity,
                     const char* format,
                     std::va_list arguments) {
    return api.secure_vformat(output, output_capacity, format, arguments);
}

int secure_vsnprintf_bounded(const SecureFormattingApi& api,
                             char* output,
                             std::size_t output_capacity,
                             std::size_t maximum_count,
                             const char* format,
                             std::va_list arguments) {
    return api.secure_vformat_bounded(output, output_capacity, maximum_count,
                                      format, arguments);
}

int secure_snprintf(const SecureFormattingApi& api,
                    char* output,
                    std::size_t output_capacity,
                    const char* format,
                    ...) {
    std::va_list arguments;
    va_start(arguments, format);
    const int result = api.secure_vformat(output, output_capacity, format,
                                          arguments);
    va_end(arguments);
    return result < 0 ? -1 : result;
}

} // namespace creatures1::platform
