#include "owner.hpp"

#include <new>

namespace creatures1::creatures {

std::unique_ptr<Owner> Owner::create_object(
    const OwnerInitializationHost& host) {
    std::unique_ptr<Owner> owner(new (std::nothrow) Owner);
    if (owner != nullptr) {
        // The native factory stores its creation timestamp in the second of
        // the six serialized owner strings; the other slots start empty.
        owner->serialized_strings[1] = host.current_owner_timestamp();
    }
    return owner;
}

void Owner::serialize(archive::StringArchive& archive) {
    if (archive.is_loading()) {
        for (std::string& value : serialized_strings) {
            value = archive.read_string();
        }
        return;
    }

    for (const std::string& value : serialized_strings) {
        archive.write_string(value);
    }
}

}  // namespace creatures1::creatures
