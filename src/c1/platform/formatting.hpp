#pragma once

#include <cstdarg>
#include <cstddef>

namespace creatures1::platform {

// The compatible MSVC CRT supplies the secure formatting implementation.
// C1 code depends only on this narrow boundary, not on __stdio_common_* or
// the CRT's internal option/locale records.
class SecureFormattingApi {
public:
    virtual ~SecureFormattingApi() = default;

    virtual int secure_vformat(char* output,
                               std::size_t output_capacity,
                               const char* format,
                               std::va_list arguments) const = 0;
    virtual int secure_vformat_bounded(char* output,
                                       std::size_t output_capacity,
                                       std::size_t maximum_count,
                                       const char* format,
                                       std::va_list arguments) const = 0;
};

int secure_vsnprintf(const SecureFormattingApi& api,
                     char* output,
                     std::size_t output_capacity,
                     const char* format,
                     std::va_list arguments);

int secure_vsnprintf_bounded(const SecureFormattingApi& api,
                             char* output,
                             std::size_t output_capacity,
                             std::size_t maximum_count,
                             const char* format,
                             std::va_list arguments);

int secure_snprintf(const SecureFormattingApi& api,
                    char* output,
                    std::size_t output_capacity,
                    const char* format,
                    ...);

} // namespace creatures1::platform
