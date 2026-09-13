#pragma once

#include <array>
#include <memory>
#include <string>

#include "../archive/string_archive.hpp"

namespace creatures1::creatures {

class OwnerInitializationHost {
public:
    virtual ~OwnerInitializationHost() = default;
    // The native factory obtains and formats the current local timestamp
    // through CRT time services. Formatting and locale remain host-owned.
    virtual std::string current_owner_timestamp() const = 0;
};

class Owner {
public:
    static std::unique_ptr<Owner> create_object(
        const OwnerInitializationHost& host);
    void serialize(archive::StringArchive& archive);

    std::array<std::string, 6> serialized_strings;
};

}  // namespace creatures1::creatures
