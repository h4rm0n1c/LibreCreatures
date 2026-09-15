#include "../src/c1/creatures/skeleton.hpp"
#include <cassert>
#include <iostream>

using namespace creatures1::creatures;

int main() {
    Skeleton skeleton;
    skeleton.body = std::make_unique<Body>();
    LimbPart head;
    skeleton.limb_chain_heads[0] = &head;
    for (unsigned facing = 0; facing < 4; ++facing) {
        skeleton.facing_direction = static_cast<FacingDirection>(facing);
        for (unsigned pose = 0; pose < 6; ++pose) {
            head.pose_frame_index = pose;
            for (unsigned drive = 0; drive < 3; ++drive) {
                skeleton.drive_threshold_state = drive;
                const unsigned expression =
                    drive && (facing == 1 || pose == 4) ? drive + 1 : 0;
                for (bool open : {false, true}) {
                    skeleton.eyes_open = open;
                    skeleton.recompute_body_part_layout();
                    assert(head.current_image_index() ==
                           head.image_index_base() + head.attachment_frame_index +
                           expression + (open ? 0 : 13));
                }
            }
        }
    }
    skeleton.limb_chain_heads[0] = nullptr;
    std::cout << "native skeleton eye frames: PASS\n";
}
