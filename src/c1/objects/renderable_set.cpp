#include "renderable_set.hpp"

namespace creatures1::objects {

RenderableObjectSet g_renderable_object_set{};

RenderableObjectSet::~RenderableObjectSet() {
    destroy();
}

void RenderableObjectSet::initialize() {
    if (sentinel == nullptr) {
        sentinel = new RenderableObjectSetNode{};
    }
    sentinel->next = sentinel;
    sentinel->prev = sentinel;
    sentinel->object = nullptr;

    size = 0;
    max_load_factor = 1.0F;
    bucket_mask = 7;
    bucket_count = 8;

    delete[] buckets;
    buckets = new RenderableObjectSetBucket[bucket_count];
    buckets_end = buckets + bucket_count;
    buckets_capacity = buckets_end;
    for (RenderableObjectSetBucket* bucket = buckets;
         bucket != buckets_end;
         ++bucket) {
        bucket->first = sentinel;
        bucket->last = sentinel;
    }
}

void RenderableObjectSet::destroy() {
    delete[] buckets;
    buckets = nullptr;
    buckets_end = nullptr;
    buckets_capacity = nullptr;
    bucket_mask = 0;
    bucket_count = 0;
    size = 0;

    if (sentinel != nullptr) {
        RenderableObjectSetNode* node = sentinel->next;
        while (node != nullptr && node != sentinel) {
            RenderableObjectSetNode* next = node->next;
            delete node;
            node = next;
        }
        delete sentinel;
        sentinel = nullptr;
    }
}

void initialize_renderable_object_set() {
    g_renderable_object_set.initialize();
}

} // namespace creatures1::objects
