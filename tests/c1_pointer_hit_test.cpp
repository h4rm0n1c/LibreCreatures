#include "objects/object.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

namespace c1 = creatures1;
using c1::objects::Object;
using c1::world::WorldRect;

namespace {

class TestObject final : public Object {
public:
    TestObject(WorldRect bounds, int plane, Object::BoundsFlags flags)
        : bounds_(bounds), plane_(plane) {
        set_bounds_flags_for_script(flags);
    }
    bool get_bounds(WorldRect* out) const override {
        *out = bounds_;
        return true;
    }
    int render_plane() const override { return plane_; }

private:
    WorldRect bounds_;
    int plane_;
};

class World final : public c1::objects::ObjectOverlapHost {
public:
    std::vector<Object*> objects;
    const Object* pointer = nullptr;
    WorldRect hotspot{};

    std::size_t object_count() const override { return objects.size(); }
    Object* object_at(std::size_t index) const override {
        return objects[index];
    }
    void report_invalid_index() const override { assert(false); }
    bool is_pointer_tool(const Object& object) const override {
        return &object == pointer;
    }
    WorldRect pointer_tool_bounds(const Object&) const override {
        return hotspot;
    }
    WorldRect vehicle_interaction_bounds(const Object&) const override {
        return {};
    }
};

constexpr Object::BoundsFlags kActivatable = Object::kActivatable;

WorldRect at(int x, int y) { return {x, y, x + 1, y + 1}; }

} // namespace

int main() {
    TestObject hand({0, 0, 1, 1}, 9999, 0);
    World world;
    world.pointer = &hand;

    // A norn inside a lift: the lift draws in front, but the hand is aimed
    // at the norn it encloses.
    TestObject lift({100, 100, 200, 300}, 10, kActivatable);
    TestObject norn({120, 200, 170, 290}, 5, kActivatable);
    world.objects = {&hand, &lift, &norn};
    world.hotspot = at(140, 250);
    assert(hand.find_topmost_overlapping_object(kActivatable, kActivatable,
                                                world) == &norn);

    // Pointing at the lift away from the norn still reaches the lift.
    world.hotspot = at(110, 120);
    assert(hand.find_topmost_overlapping_object(kActivatable, kActivatable,
                                                world) == &lift);

    // Partial overlap keeps the native plane rule.
    TestObject back({0, 0, 50, 50}, 3, kActivatable);
    TestObject front({40, 0, 100, 50}, 7, kActivatable);
    world.objects = {&hand, &back, &front};
    world.hotspot = at(45, 10);
    assert(hand.find_topmost_overlapping_object(kActivatable, kActivatable,
                                                world) == &front);

    // Only matching objects compete: an unflagged thing inside the lift
    // does not hide the lift.
    TestObject scenery_bit({120, 200, 170, 290}, 50, 0);
    world.objects = {&hand, &lift, &scenery_bit};
    world.hotspot = at(140, 250);
    assert(hand.find_topmost_overlapping_object(kActivatable, kActivatable,
                                                world) == &lift);

    // Searches by anything other than the hand keep the native rule.
    TestObject searcher({130, 240, 150, 260}, 0, 0);
    world.objects = {&searcher, &lift, &norn};
    assert(searcher.find_topmost_overlapping_object(
               kActivatable, kActivatable, world) == &lift);

    std::printf("pointer_hit_test ok\n");
    return 0;
}
