#include "object_handle.hpp"

// Only 64-bit builds use this file: in a 32-bit build the handle is the
// object's address (object_handle.hpp).
#if UINTPTR_MAX != 0xffffffffu

#include <mutex>
#include <unordered_map>

namespace creatures1::objects {

namespace {

// Handles look like the original's: nonzero, 4-aligned, and above the low
// values that scripts use as plain numbers.  They are never reused, so a
// value a script kept after its object was deleted names no object.
constexpr ObjectHandle kFirstHandle = 0x00400000u;

struct Registry {
    std::mutex lock;
    std::unordered_map<const Object*, ObjectHandle> handles;
    std::unordered_map<ObjectHandle, Object*> objects;
    ObjectHandle next = kFirstHandle;
};

Registry& registry() {
    static Registry instance;
    return instance;
}

}  // namespace

ObjectHandle handle_of(const Object* object) {
    if (object == nullptr) {
        return 0;
    }
    Registry& r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    const auto found = r.handles.find(object);
    if (found != r.handles.end()) {
        return found->second;
    }
    const ObjectHandle handle = r.next;
    r.next += 4;
    r.handles.emplace(object, handle);
    r.objects.emplace(handle, const_cast<Object*>(object));
    return handle;
}

Object* object_at_handle(ObjectHandle handle) {
    if (handle == 0) {
        return nullptr;
    }
    Registry& r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    const auto found = r.objects.find(handle);
    return found == r.objects.end() ? nullptr : found->second;
}

void forget_object_handle(const Object* object) noexcept {
    Registry& r = registry();
    std::lock_guard<std::mutex> guard(r.lock);
    const auto found = r.handles.find(object);
    if (found == r.handles.end()) {
        return;
    }
    r.objects.erase(found->second);
    r.handles.erase(found);
}

}  // namespace creatures1::objects

#endif
