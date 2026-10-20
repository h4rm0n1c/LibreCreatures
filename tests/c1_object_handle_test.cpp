// The object handle registry of 64-bit builds (objects/object_handle.cpp).
// Build it as a 64-bit program:
//
//   g++ -std=c++17 -Isrc/c1 -o object_handle_test
//       tests/c1_object_handle_test.cpp src/c1/objects/object_handle.cpp
//
// In a 32-bit build a handle is the object's address, which needs no test.

#include "objects/object_handle.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

namespace objects = creatures1::objects;

static_assert(sizeof(void*) == 8, "build this test as a 64-bit program");

int main() {
    // The registry never reads an object, so plain storage stands in for one.
    std::vector<char> storage(64);
    auto* first = reinterpret_cast<objects::Object*>(&storage[0]);
    auto* second = reinterpret_cast<objects::Object*>(&storage[32]);

    assert(objects::handle_of(nullptr) == 0);
    assert(objects::object_at_handle(0) == nullptr);

    const objects::ObjectHandle a = objects::handle_of(first);
    const objects::ObjectHandle b = objects::handle_of(second);
    assert(a != 0 && b != 0 && a != b);
    assert(a % 4 == 0 && b % 4 == 0);
    assert(objects::handle_of(first) == a);  // stable
    assert(objects::object_at_handle(a) == first);
    assert(objects::object_at_handle(b) == second);
    assert(objects::object_at_handle(a + 4 * 1000) == nullptr);  // never given

    // A deleted object's handle names nothing, and is not given again.
    objects::forget_object_handle(first);
    assert(objects::object_at_handle(a) == nullptr);
    const objects::ObjectHandle again = objects::handle_of(first);
    assert(again != a);
    assert(objects::object_at_handle(again) == first);
    objects::forget_object_handle(nullptr);  // no effect

    std::printf("object handles: ok (%08x %08x %08x)\n", a, b, again);
    return 0;
}
