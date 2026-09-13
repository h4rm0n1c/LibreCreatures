#pragma once

#include <cstddef>
#include <cstdint>

namespace creatures1::objects {

class Object;

struct RenderableObjectSetNode {
    RenderableObjectSetNode* next = nullptr;
    RenderableObjectSetNode* prev = nullptr;
    Object* object = nullptr;
};

struct RenderableObjectSetBucket {
    RenderableObjectSetNode* first = nullptr;
    RenderableObjectSetNode* last = nullptr;
};

// The node/bucket representation is retained because later renderable-set
// operations use the sentinel list and bucket ranges directly.  Process
// startup and teardown call the explicit owner methods; CRT atexit and
// allocator mechanics are supplied by the host/toolchain boundary.
class RenderableObjectSet {
public:
    ~RenderableObjectSet();

    void initialize();
    void destroy();

    float max_load_factor = 1.0F;
    RenderableObjectSetNode* sentinel = nullptr;
    std::uint32_t size = 0;
    RenderableObjectSetBucket* buckets = nullptr;
    RenderableObjectSetBucket* buckets_end = nullptr;
    RenderableObjectSetBucket* buckets_capacity = nullptr;
    std::uint32_t bucket_mask = 0;
    std::uint32_t bucket_count = 0;
};

extern RenderableObjectSet g_renderable_object_set;

void initialize_renderable_object_set();

} // namespace creatures1::objects
