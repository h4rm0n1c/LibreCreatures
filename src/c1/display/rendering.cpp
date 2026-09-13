#include "rendering.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <vector>

namespace creatures1::display {

namespace {

bool render_plane_precedes(const VisibleSpriteRecord& left,
                           const VisibleSpriteRecord& right) {
    return left.render_plane < right.render_plane;
}

int clamp_vertical_origin(int requested_top, int viewport_height) {
    if (requested_top < 0) {
        return 0;
    }
    if (requested_top + viewport_height > world::kWorldHeight) {
        return std::max(0, world::kWorldHeight - viewport_height);
    }
    return requested_top;
}

} // namespace

void merge_visible_sprite_record_runs_to_buffer(
    const VisibleSpriteRecord* source_begin,
    const VisibleSpriteRecord* source_end,
    VisibleSpriteRecord* destination_begin,
    int left_run_length,
    int record_count) {
    const VisibleSpriteRecord* left = source_begin;
    const VisibleSpriteRecord* right = source_begin + left_run_length;
    const VisibleSpriteRecord* right_end = right +
        std::min(left_run_length, record_count - left_run_length);
    VisibleSpriteRecord* destination = destination_begin;

    while (left != right && right != right_end) {
        if (left->render_plane <= right->render_plane) {
            *destination++ = *left++;
        } else {
            *destination++ = *right++;
        }
    }
    while (left != right) {
        *destination++ = *left++;
    }
    while (right != right_end) {
        *destination++ = *right++;
    }

    const std::ptrdiff_t copied = destination - destination_begin;
    const std::ptrdiff_t available = source_end - source_begin;
    if (copied < available) {
        std::memmove(destination, source_begin + copied,
                     static_cast<std::size_t>(available - copied) *
                         sizeof(VisibleSpriteRecord));
    }
}

void reverse_visible_sprite_record_range(VisibleSpriteRecord* first,
                                         VisibleSpriteRecord* last) {
    std::reverse(first, last);
}

VisibleSpriteRecord* insertion_sort_visible_sprite_records_by_plane(
    VisibleSpriteRecord* begin,
    VisibleSpriteRecord* end,
    int record_count) {
    (void)record_count;
    if (begin == end) {
        return end;
    }
    for (VisibleSpriteRecord* current = begin + 1;
         current != end;
         ++current) {
        const VisibleSpriteRecord value = *current;
        VisibleSpriteRecord* insertion_point = current;
        while (insertion_point != begin &&
               value.render_plane < (insertion_point - 1)->render_plane) {
            *insertion_point = *(insertion_point - 1);
            --insertion_point;
        }
        *insertion_point = value;
    }
    return end;
}

void stable_sort_visible_sprite_records_by_plane(
    VisibleSpriteRecord* begin,
    VisibleSpriteRecord* end,
    int record_count,
    VisibleSpriteRecord* scratch_buffer) {
    (void)scratch_buffer;
    if (record_count < 0x21) {
        insertion_sort_visible_sprite_records_by_plane(begin, end,
                                                        record_count);
        return;
    }
    std::stable_sort(begin, end, render_plane_precedes);
}

void inplace_merge_visible_sprite_records_by_plane(
    VisibleSpriteRecord* first,
    VisibleSpriteRecord* middle,
    VisibleSpriteRecord* last,
    int left_count,
    int right_count,
    VisibleSpriteRecord* scratch_buffer,
    int scratch_capacity,
    int total_count) {
    (void)left_count;
    (void)right_count;
    (void)scratch_buffer;
    (void)scratch_capacity;
    (void)total_count;
    if (first == middle || middle == last) {
        return;
    }
    std::inplace_merge(first, middle, last, render_plane_precedes);
}

void symmetric_merge_visible_sprite_record_ranges_by_plane(
    VisibleSpriteRecord* first,
    VisibleSpriteRecord* middle,
    VisibleSpriteRecord* last,
    int left_count,
    int right_count,
    VisibleSpriteRecord* scratch_buffer,
    int scratch_capacity,
    int total_count,
    VisibleSpriteRecord* left_split,
    VisibleSpriteRecord* right_split,
    int left_lower_count,
    int right_lower_count) {
    (void)total_count;

    const int remaining_left = left_count - left_lower_count;
    VisibleSpriteRecord* new_middle = right_split;
    if (remaining_left != 0 && right_lower_count != 0) {
        // The binary chooses the smaller side for temporary storage.  When
        // neither side fits, three reversals implement the same block
        // rotation without allocating memory.
        if (right_lower_count < remaining_left ||
            scratch_capacity < remaining_left) {
            if (scratch_capacity < right_lower_count) {
                new_middle = left_split;
                if (left_split != middle && middle != right_split) {
                    reverse_visible_sprite_record_range(left_split, middle);
                    reverse_visible_sprite_record_range(middle, right_split);
                    reverse_visible_sprite_record_range(left_split,
                                                        right_split);
                    new_middle = left_split + (right_split - middle);
                }
            } else {
                const std::size_t right_bytes =
                    static_cast<std::size_t>(right_split - middle) *
                    sizeof(VisibleSpriteRecord);
                std::memmove(scratch_buffer, middle, right_bytes);
                std::memmove(right_split - (middle - left_split), left_split,
                             static_cast<std::size_t>(middle - left_split) *
                             sizeof(VisibleSpriteRecord));
                std::memmove(left_split, scratch_buffer, right_bytes);
                new_middle = left_split + (right_split - middle);
            }
        } else {
            const std::size_t left_bytes =
                static_cast<std::size_t>(middle - left_split) *
                sizeof(VisibleSpriteRecord);
            std::memmove(scratch_buffer, left_split, left_bytes);
            std::memmove(left_split, middle,
                         static_cast<std::size_t>(right_split - middle) *
                         sizeof(VisibleSpriteRecord));
            new_middle = left_split + (right_split - middle);
            std::memmove(new_middle, scratch_buffer, left_bytes);
        }
    }

    inplace_merge_visible_sprite_records_by_plane(
        first, left_split, new_middle, left_lower_count,
        right_lower_count, scratch_buffer, scratch_capacity, total_count);
    inplace_merge_visible_sprite_records_by_plane(
        new_middle, right_split, last, remaining_left,
        right_count - right_lower_count, scratch_buffer, scratch_capacity,
        total_count);
}

WorldRenderer::WorldRenderer(WorldRendererHost& host,
                             void* owner_window,
                             int initial_viewport_left,
                             int initial_viewport_top,
                             int initial_viewport_width,
                             int initial_viewport_height,
                             ::creatures1::creatures::Creature* followed_creature,
                             bool smooth_scrolling_enabled,
                             int overlay_gallery_identifier)
    : host_(host),
      owner_window_(owner_window),
      palette_(host.game_palette()),
      smooth_scrolling_enabled_(smooth_scrolling_enabled),
      followed_creature_(nullptr),
      viewport_left_(initial_viewport_left),
      viewport_top_(initial_viewport_top),
      viewport_right_(initial_viewport_left + initial_viewport_width),
      viewport_bottom_(initial_viewport_top + initial_viewport_height),
      viewport_width_(initial_viewport_width),
      viewport_height_(initial_viewport_height),
      overlay_gallery_identifier_(overlay_gallery_identifier) {
    (void)followed_creature;
    followed_creature_ = nullptr;
    memory_dc_ = host_.create_memory_dc();
    for (world::WorldRect& rect : queued_dirty_rects_) {
        rect = {};
    }
    create_back_buffer_dib(8, 8);
}

WorldRenderer::~WorldRenderer() {
    if (memory_dc_ != nullptr && previous_selected_object_ != nullptr) {
        host_.select_bitmap(memory_dc_, previous_selected_object_);
    }
    if (dib_section_ != nullptr) {
        host_.delete_object(dib_section_);
    }
    if (memory_dc_ != nullptr) {
        host_.delete_dc(memory_dc_);
    }
}

void WorldRenderer::redraw_full_view(void* device_context) {
    host_.fill_client_background_black(device_context);
    full_redraw_pending_ = true;
    present_world_rect(device_context,
                       {viewport_left_, viewport_top_,
                        viewport_right_, viewport_bottom_});
}

void WorldRenderer::resize_back_buffer_for_viewport(void* viewport_window,
                                                    int viewport_width,
                                                    int viewport_height) {
    (void)viewport_window;
    if (viewport_width == 0 || viewport_height == 0) {
        return;
    }
    if ((viewport_width & 3) != 0) {
        viewport_width = (viewport_width & ~3) + 4;
    }
    void* previous_bitmap = create_back_buffer_dib(viewport_width,
                                                   viewport_height);
    if (previous_bitmap != nullptr) {
        host_.delete_object(previous_bitmap);
    }
    viewport_right_ = viewport_left_ + viewport_width;
    viewport_width_ = viewport_width;
    viewport_height_ = viewport_height;

    // Keep the same two-stage clamp as the original: first move a viewport
    // that runs below the world back up to the bottom edge, then handle a
    // viewport that is taller than the world as the special [0, world_height]
    // case.
    const int unclamped_bottom = viewport_top_ + viewport_height_;
    viewport_bottom_ = unclamped_bottom;
    if (unclamped_bottom > world::kWorldHeight) {
        viewport_bottom_ = world::kWorldHeight;
        viewport_top_ = viewport_top_ - unclamped_bottom +
                        world::kWorldHeight;
    }
    if (viewport_top_ < 0) {
        viewport_top_ = 0;
        viewport_bottom_ = std::min(viewport_height_, world::kWorldHeight);
    }
}

std::uint32_t WorldRenderer::realize_palette() {
    if (palette_ == nullptr) {
        return 1;
    }
    const std::uint32_t realized =
        host_.realize_palette(owner_window_, palette_);
    if (realized != 0) {
        host_.invalidate_window(owner_window_);
    }
    return realized;
}

void WorldRenderer::set_smooth_scrolling_enabled(bool enabled) {
    if (smooth_scrolling_enabled_ == enabled) {
        return;
    }
    smooth_scrolling_enabled_ = enabled;
    reset_navigation();
}

void WorldRenderer::render_world_rect_to_dib(
    const world::WorldRect& render_rect) {
    const world::WorldRect viewport_rect{
        viewport_left_, viewport_top_, viewport_right_, viewport_bottom_};

    // The map gallery is a 58-column by 8-row tile sheet stored column major:
    // a column's eight tiles are consecutive, so the image index is
    // column * 8 + row.  Horizontal tile selection wraps independently of the
    // world rectangle's unwrapped right edge; Image performs the final clip
    // into the viewport-sized DIB.
    Gallery* background = host_.background_gallery();
    const int first_tile_row = render_rect.min_y / 150;
    const int last_tile_row = (render_rect.max_y - 1) / 150;
    const int first_tile_column = render_rect.min_x / 144;
    const int last_tile_column = (render_rect.max_x - 1) / 144;
    if (background != nullptr) {
        for (int row = first_tile_row; row <= last_tile_row; ++row) {
            for (int column = first_tile_column;
                 column <= last_tile_column; ++column) {
                int wrapped_column = column % 58;
                if (wrapped_column < 0) {
                    wrapped_column += 58;
                }
                const std::size_t image_index =
                    static_cast<std::size_t>(wrapped_column) * 8U +
                    static_cast<std::size_t>(row);
                if (image_index >= background->image_count) {
                    continue;
                }
                host_.blit_image_to_dib(
                    background->images[image_index], dib_pixels_,
                    wrapped_column * 144, row * 150, render_rect,
                    viewport_rect, true);
            }
        }
    }

    constexpr std::size_t kMaximumVisibleSprites = 4000;
    const std::size_t registry_count =
        std::min(host_.entity_count(), kMaximumVisibleSprites);
    std::size_t visible_count = 0;
    for (std::size_t registry_index = 0; registry_index < registry_count;
         ++registry_index) {
        const objects::Entity* entity = host_.entity_at(registry_index);
        if (entity == nullptr || entity->gallery() == nullptr) {
            continue;
        }
        const Gallery* gallery = entity->gallery();
        const std::size_t image_index = entity->current_image_index();
        // The executable compares the byte index against image_count before
        // using the image array.  Valid C1 galleries contain the complete
        // index range, so retain that recovered contract here.
        if (image_index > gallery->image_count) {
            continue;
        }

        int relative_x = entity->world_x() - render_rect.min_x;
        if (relative_x < 0x1051) {
            if (relative_x < -0x1050) {
                relative_x += world::kWorldWidth;
            }
        } else {
            relative_x -= world::kWorldWidth;
        }
        const Image& image = gallery->images[image_index];
        const bool intersects =
            relative_x <= render_rect.max_x - render_rect.min_x &&
            -1 < image.width() + relative_x &&
            entity->world_y() <= render_rect.max_y &&
            render_rect.min_y <= image.height() + entity->world_y();
        if (intersects && visible_count < kMaximumVisibleSprites) {
            visible_sprite_records_[visible_count++] = {
                static_cast<std::int32_t>(registry_index),
                static_cast<std::int32_t>(entity->render_plane())};
        }
    }

    if (visible_count < 0x21) {
        insertion_sort_visible_sprite_records_by_plane(
            visible_sprite_records_.data(),
            visible_sprite_records_.data() + visible_count,
            static_cast<int>(visible_count));
    } else {
        const std::size_t requested_scratch =
            visible_count - visible_count / 2U;
        std::array<VisibleSpriteRecord, 512> stack_scratch{};
        std::vector<VisibleSpriteRecord> heap_scratch;
        VisibleSpriteRecord* scratch = stack_scratch.data();
        int scratch_capacity = static_cast<int>(
            std::min(requested_scratch, std::size_t{0x7fffffff}));
        if (requested_scratch > stack_scratch.size()) {
            heap_scratch.resize(requested_scratch);
            scratch = heap_scratch.data();
        }
        stable_sort_visible_sprite_records_by_plane(
            visible_sprite_records_.data(),
            visible_sprite_records_.data() + visible_count,
            static_cast<int>(visible_count), scratch);
        (void)scratch_capacity;
    }

    for (std::size_t record_index = 0; record_index < visible_count;
         ++record_index) {
        const std::size_t registry_index = static_cast<std::size_t>(
            visible_sprite_records_[record_index].entity_registry_index);
        if (registry_index >= host_.entity_count()) {
            host_.report_invalid_render_registry_index();
            return;
        }
        const objects::Entity* entity = host_.entity_at(registry_index);
        if (entity == nullptr || entity->gallery() == nullptr) {
            host_.report_invalid_render_registry_index();
            return;
        }
        host_.blit_image_to_dib(
            entity->gallery()->images[entity->current_image_index()],
            dib_pixels_, entity->world_x(), entity->world_y(), render_rect,
            viewport_rect, false);
    }

    if (overlay_gallery_identifier_ == 0) {
        return;
    }
    if (overlay_image_ == nullptr) {
        overlay_image_ = host_.acquire_overlay_gallery(
            static_cast<std::uint32_t>(overlay_gallery_identifier_));
        if (overlay_image_ == nullptr) {
            overlay_gallery_identifier_ = 0;
            return;
        }
    }
    if (overlay_image_->image_count != 0 && overlay_image_->images != nullptr) {
        host_.blit_image_to_dib(
            overlay_image_->images[0], dib_pixels_, viewport_left_,
            viewport_top_, render_rect, viewport_rect, false);
    }
}

void WorldRenderer::render_and_present_rect(
    void* target_context,
    const world::WorldRect& requested_rect,
    const world::WorldRect& clip_rect) {
    world::WorldRect render_rect = requested_rect;
    const int render_width = render_rect.max_x - render_rect.min_x;
    if ((render_width & 3) != 0) {
        const int alignment_padding = 4 - (render_width & 3);
        bool skip_left_padding = false;
        render_rect.max_x += alignment_padding;
        if (clip_rect.max_x < world::kWorldWidth) {
            if (clip_rect.max_x < render_rect.max_x) {
                render_rect.max_x -= alignment_padding;
                render_rect.min_x -= alignment_padding;
            }
        } else {
            int wrapped_clip_right = clip_rect.max_x - world::kWorldWidth;
            int aligned_right = render_rect.max_x - world::kWorldWidth;
            if (render_rect.max_x < world::kWorldWidth) {
                aligned_right = render_rect.max_x;
            }
            if (wrapped_clip_right < aligned_right) {
                render_rect.max_x -= alignment_padding;
                render_rect.min_x -= alignment_padding;
                if (render_rect.min_x < 0) {
                    render_rect.min_x += world::kWorldWidth;
                    render_rect.max_x += world::kWorldWidth;
                }
            }
            skip_left_padding = render_rect.min_x < clip_rect.min_x;
        }
        if (!skip_left_padding && render_rect.min_x < clip_rect.min_x) {
            render_rect.min_x += alignment_padding;
        }
    }

    render_world_rect_to_dib(render_rect);
    int destination_x = render_rect.min_x - viewport_left_;
    if (viewport_left_ - viewport_right_ + world::kWorldWidth <
        destination_x) {
        destination_x -= world::kWorldWidth;
    } else if (destination_x <
               viewport_right_ - viewport_left_ - world::kWorldWidth) {
        destination_x += world::kWorldWidth;
    }
    const int destination_y = render_rect.min_y - viewport_top_;
    host_.bit_blt(target_context, destination_x, destination_y,
                  render_rect.max_x - render_rect.min_x,
                  render_rect.max_y - render_rect.min_y, memory_dc_,
                  destination_x, destination_y, 0xcc0020U);
    // The executable gates the debug outline on the highlight's right edge
    // rather than a separate flag, so `sys: edit 0 0 0 0` clears it.
    if (dirty_world_rect_.max_x != 0) {
        host_.draw_dirty_world_outline(
            target_context, dirty_world_rect_,
            {viewport_left_, viewport_top_, viewport_right_, viewport_bottom_});
    }
}

void WorldRenderer::present_world_rect(void* device_context,
                                       const world::WorldRect& requested_rect) {
    if (host_.owner_is_minimized(owner_window_) || !full_redraw_pending_ ||
        requested_rect.min_x >= requested_rect.max_x ||
        requested_rect.min_y >= requested_rect.max_y) {
        return;
    }
    // intersect_wrapped_world_rects returns its result in the FIRST argument
    // and leaves the second partly rewritten, so the intersection is the
    // viewport copy that went in -- which is what the native renders, while
    // reading the viewport itself fresh for the second pair of arguments.
    world::WorldRect intersection{
        viewport_left_, viewport_top_, viewport_right_, viewport_bottom_};
    world::WorldRect requested = requested_rect;
    if (world::intersect_wrapped_world_rects(intersection, requested)) {
        const world::WorldRect viewport_rect{
            viewport_left_, viewport_top_, viewport_right_, viewport_bottom_};
        render_and_present_rect(device_context, intersection, viewport_rect);
        if (deferred_dirty_rect_rendering_) {
            queued_dirty_rect_count_ = 0;
        }
    }
}

void WorldRenderer::set_debug_highlight_rect(int left, int top, int right,
                                            int bottom) {
    dirty_world_rect_ = {left, top, right, bottom};
    queue_dirty_world_rect(viewport_left_, viewport_top_, viewport_right_,
                           viewport_bottom_);
}

void WorldRenderer::begin_deferred_dirty_rectangles() {
    deferred_dirty_rect_rendering_ = true;
    queued_dirty_rect_count_ = 0;
}

void WorldRenderer::flush_deferred_dirty_rectangles() {
    if (!deferred_dirty_rect_rendering_) {
        return;
    }
    // Native clears the mode whenever it was set, but only drains -- and only
    // resets the count -- when there is something queued and a full redraw is
    // pending.
    deferred_dirty_rect_rendering_ = false;
    if (queued_dirty_rect_count_ == 0 || !full_redraw_pending_) {
        return;
    }

    const world::WorldRect viewport_rect{
        viewport_left_, viewport_top_, viewport_right_, viewport_bottom_};
    for (int index = 0; index < queued_dirty_rect_count_; ++index) {
        host_.present_dirty_world_rect(queued_dirty_rects_[index],
                                       viewport_rect);
    }
    queued_dirty_rect_count_ = 0;
}

void WorldRenderer::queue_dirty_world_rect(int world_left,
                                           int world_top,
                                           int world_right,
                                           int world_bottom) {
    if (host_.owner_is_minimized(owner_window_) ||
        !full_redraw_pending_ || world_left >= world_right ||
        world_top >= world_bottom) {
        return;
    }

    // As above: the intersection comes back in the first argument.  Taking
    // the second one instead let a rectangle through with its Y range still
    // unclipped -- a creature dropped from a vehicle produced
    // (8316,509)-(9287,10010) against a 984x181 viewport, and presenting that
    // wrote far outside the surface.
    world::WorldRect clipped_rect{
        viewport_left_, viewport_top_, viewport_right_, viewport_bottom_};
    world::WorldRect requested_rect{
        world_left, world_top, world_right, world_bottom};
    if (!world::intersect_wrapped_world_rects(clipped_rect, requested_rect)) {
        return;
    }
    const world::WorldRect viewport_rect{
        viewport_left_, viewport_top_, viewport_right_, viewport_bottom_};

    // `dirty_world_rect_` is NOT the queue's business: in the executable the
    // only writers are the constructor, SetRectEmpty, and one CAOS
    // sub-command, because it is a debug highlight that
    // CWorldRenderer::RenderAndPresentRect outlines in magenta.  Setting it
    // here made every queued rectangle leave a stale outline on screen --
    // invisible only while a full-viewport repaint was overpainting it.

    if (!deferred_dirty_rect_rendering_) {
        host_.present_dirty_world_rect(clipped_rect, viewport_rect);
        return;
    }

    for (int index = 0; index < queued_dirty_rect_count_; ++index) {
        world::WorldRect& queued_rect = queued_dirty_rects_[index];
        if (world::wrapped_world_rects_overlap(queued_rect, clipped_rect)) {
            world::WorldRect merged_rect{};
            world::union_wrapped_world_rects(merged_rect, queued_rect,
                                             clipped_rect);
            queued_rect = merged_rect;
            return;
        }
    }

    if (queued_dirty_rect_count_ <
        static_cast<int>(queued_dirty_rects_.size())) {
        queued_dirty_rects_[queued_dirty_rect_count_++] = clipped_rect;
        return;
    }

    world::union_wrapped_world_rects(queued_dirty_rects_.back(),
                                     queued_dirty_rects_.back(),
                                     clipped_rect);
}

void WorldRenderer::request_viewport_origin(int world_x, int world_y) {
    if (!smooth_scrolling_enabled_) {
        set_viewport_origin(world_x, world_y);
    }
    int delta_x = world_x - viewport_left_;
    const int delta_y = world_y - viewport_top_;
    if (delta_x < -0x104f) {
        delta_x += world::kWorldWidth;
    } else if (delta_x > 0x1050) {
        delta_x -= world::kWorldWidth;
    }

    const int viewport_width = viewport_right_ - viewport_left_;
    const int viewport_height = viewport_bottom_ - viewport_top_;
    if (delta_x >= -viewport_width && delta_x <= viewport_width &&
        delta_y >= -viewport_height && delta_y <= viewport_height) {
        smooth_scroll_remaining_x_ = delta_x;
        smooth_scroll_remaining_y_ = delta_y;
        smooth_scroll_step_x_ = 0;
        smooth_scroll_step_y_ = 0;
        smooth_scroll_accumulated_x_ = 0;
        smooth_scroll_accumulated_y_ = 0;
        return;
    }
    smooth_scroll_remaining_x_ = 0;
    smooth_scroll_remaining_y_ = 0;
    smooth_scroll_step_x_ = 0;
    smooth_scroll_step_y_ = 0;
    smooth_scroll_accumulated_x_ = 0;
    smooth_scroll_accumulated_y_ = 0;
    set_viewport_origin(world_x, world_y);
}

void WorldRenderer::request_viewport_origin_for_selected_creature() {
    int foot_x = 0;
    int foot_y = 0;
    if (!host_.selected_creature_down_foot(foot_x, foot_y)) {
        followed_creature_ = nullptr;
        return;
    }
    if (!host_.contains_point(host_.navigation_bounds(), foot_x, foot_y)) {
        return;
    }
    int origin_x = foot_x -
        (viewport_right_ - viewport_left_) / 2;
    if (origin_x < 0) {
        origin_x += world::kWorldWidth;
    } else if (origin_x > world::kWorldWidth - 1) {
        origin_x -= world::kWorldWidth;
    }
    const int vertical_offset = (viewport_top_ - viewport_bottom_) * 5;
    request_viewport_origin(origin_x, foot_y + vertical_offset / 8);
}

void WorldRenderer::center_viewport_on_selected_creature_if_in_pan_region() {
    int foot_x = 0;
    int foot_y = 0;
    if (!host_.selected_creature_down_foot(foot_x, foot_y)) {
        followed_creature_ = nullptr;
        return;
    }
    if (!host_.contains_point(host_.navigation_bounds(), foot_x, foot_y)) {
        return;
    }

    int origin_x = foot_x -
        (viewport_right_ - viewport_left_) / 2;
    if (origin_x < 0) {
        origin_x += world::kWorldWidth;
    } else if (origin_x > world::kWorldWidth - 1) {
        origin_x -= world::kWorldWidth;
    }
    const int vertical_offset = (viewport_top_ - viewport_bottom_) * 5;
    set_viewport_origin(origin_x, foot_y + vertical_offset / 8);
}

void WorldRenderer::follow_selected_creature_viewport() {
    ::creatures1::creatures::Creature* selected =
        host_.selected_creature();
    if (selected == nullptr) {
        followed_creature_ = nullptr;
        return;
    }
    followed_creature_ = selected;

    if (host_.selected_creature_is_edit_object() ||
        !host_.selected_creature_is_bounded() ||
        (!smooth_scrolling_enabled_ &&
         is_selected_creature_within_safe_area())) {
        return;
    }

    int foot_x = 0;
    int foot_y = 0;
    if (!host_.selected_creature_down_foot(foot_x, foot_y)) {
        return;
    }

    int horizontal_delta =
        foot_x - (viewport_right_ + viewport_left_) / 2;
    const int vertical_target =
        (viewport_top_ * 3 + viewport_bottom_ * 5) / 8;
    int vertical_delta = foot_y - vertical_target;

    if (horizontal_delta < -0x1050) {
        horizontal_delta += world::kWorldWidth;
    } else if (horizontal_delta > 0x104f) {
        horizontal_delta -= world::kWorldWidth;
    }

    const int viewport_width = viewport_right_ - viewport_left_;
    const int viewport_height = viewport_bottom_ - viewport_top_;
    const bool large_delta =
        !smooth_scrolling_enabled_ ||
        viewport_width * 2 < std::abs(horizontal_delta) ||
        viewport_height * 2 < std::abs(vertical_delta);
    if (large_delta) {
        if (horizontal_delta != 0 || vertical_delta != 0) {
            scroll_viewport(horizontal_delta, vertical_delta);
        }
        return;
    }

    int smooth_x_adjustment = 0;
    int smooth_y_adjustment = 0;
    const int horizontal_quarter = viewport_width / 4;
    if (horizontal_delta < -horizontal_quarter &&
        smooth_scroll_step_x_ > -0x31) {
        smooth_x_adjustment = -9;
    }
    if (horizontal_delta > horizontal_quarter &&
        smooth_scroll_step_x_ < 0x31) {
        smooth_x_adjustment = 9;
    }

    const int negative_vertical_sixth = -(viewport_height / 6);
    if (vertical_delta < negative_vertical_sixth &&
        smooth_scroll_step_y_ > -0x31) {
        smooth_y_adjustment = -9;
    }
    if (viewport_height / 6 < vertical_delta &&
        smooth_scroll_step_y_ < 0x31) {
        smooth_y_adjustment = 9;
    }

    smooth_scroll_step_x_ += smooth_x_adjustment;
    smooth_scroll_step_y_ += smooth_y_adjustment;
    int scroll_x_step = smooth_scroll_step_x_ / 3;
    int scroll_y_step = smooth_scroll_step_y_ / 3;
    scroll_viewport(scroll_x_step, scroll_y_step);

    if (smooth_scroll_step_x_ < 0) {
        smooth_scroll_step_x_ = smooth_scroll_step_x_ < -3
                                    ? smooth_scroll_step_x_ + 3
                                    : 0;
    } else if (smooth_scroll_step_x_ > 0) {
        smooth_scroll_step_x_ = smooth_scroll_step_x_ < 4
                                    ? 0
                                    : smooth_scroll_step_x_ - 3;
    }

    if (smooth_scroll_step_y_ < 0) {
        smooth_scroll_step_y_ = smooth_scroll_step_y_ < -3
                                    ? smooth_scroll_step_y_ + 3
                                    : 0;
    } else if (smooth_scroll_step_y_ > 0) {
        smooth_scroll_step_y_ = smooth_scroll_step_y_ < 4
                                    ? 0
                                    : smooth_scroll_step_y_ - 3;
    }
}

bool WorldRenderer::is_selected_creature_within_safe_area() const {
    int foot_x = 0;
    int foot_y = 0;
    if (!host_.selected_creature_down_foot(foot_x, foot_y)) {
        return false;
    }

    const int horizontal_margin =
        (viewport_right_ - viewport_left_) / 6;
    const int safe_left = viewport_left_ + horizontal_margin;
    const int safe_right = viewport_right_ - horizontal_margin;

    int safe_top = viewport_top_;
    int safe_bottom = viewport_bottom_;
    const int vertical_margin = viewport_height_ / 4;
    if (safe_bottom < world::kWorldHeight) {
        safe_bottom -= vertical_margin;
    }
    if (safe_top > 0) {
        safe_top += vertical_margin;
    }

    if (foot_y < safe_top || foot_y >= safe_bottom) {
        return false;
    }
    if (safe_right < world::kWorldWidth) {
        return foot_x >= safe_left && foot_x < safe_right;
    }

    // The renderer represents a wrapped horizontal viewport as one interval
    // whose right edge extends past the world width.
    return foot_x < safe_right - world::kWorldWidth ||
           foot_x >= safe_left;
}

void WorldRenderer::scroll_viewport(int& in_out_delta_x,
                                    int& in_out_delta_y) {
    int adjusted_viewport_x = viewport_left_ + in_out_delta_x;
    int adjusted_viewport_y = viewport_top_ + in_out_delta_y;
    int world_wrap_direction = 0;

    if (adjusted_viewport_x < 0) {
        adjusted_viewport_x += world::kWorldWidth;
        world_wrap_direction = -1;
    } else if (adjusted_viewport_x >= world::kWorldWidth) {
        adjusted_viewport_x -= world::kWorldWidth;
        world_wrap_direction = 1;
    }

    if (adjusted_viewport_y < 0) {
        adjusted_viewport_y = 0;
    } else {
        const int maximum_viewport_top =
            viewport_top_ - viewport_bottom_ + world::kWorldHeight;
        if (adjusted_viewport_y > maximum_viewport_top) {
            adjusted_viewport_y = maximum_viewport_top;
        }
    }

    in_out_delta_x = adjusted_viewport_x - viewport_left_;
    in_out_delta_y = adjusted_viewport_y - viewport_top_;
    if (in_out_delta_x != 0 || in_out_delta_y != 0) {
        viewport_left_ += in_out_delta_x;
        viewport_top_ += in_out_delta_y;
        viewport_right_ += in_out_delta_x;
        viewport_bottom_ += in_out_delta_y;
        host_.move_renderable_objects(in_out_delta_x, in_out_delta_y);
        host_.update_view_anchored_objects();
        host_.present_current_view(
            owner_window_,
            {viewport_left_, viewport_top_, viewport_right_, viewport_bottom_});
    }

    in_out_delta_x += world_wrap_direction * world::kWorldWidth;
}

bool WorldRenderer::advance_smooth_scroll() {
    if (!smooth_scrolling_enabled_ ||
        (smooth_scroll_remaining_x_ == 0 &&
         smooth_scroll_remaining_y_ == 0)) {
        return false;
    }
    smooth_scroll_step_x_ = choose_scroll_step(
        smooth_scroll_remaining_x_, smooth_scroll_accumulated_x_,
        smooth_scroll_step_x_);
    smooth_scroll_step_y_ = choose_scroll_step(
        smooth_scroll_remaining_y_, smooth_scroll_accumulated_y_,
        smooth_scroll_step_y_);
    if (smooth_scroll_step_x_ != 0 || smooth_scroll_step_y_ != 0) {
        scroll_viewport(smooth_scroll_step_x_, smooth_scroll_step_y_);
    }
    smooth_scroll_accumulated_x_ += smooth_scroll_step_x_;
    smooth_scroll_accumulated_y_ += smooth_scroll_step_y_;
    smooth_scroll_remaining_x_ -= smooth_scroll_step_x_;
    smooth_scroll_remaining_y_ -= smooth_scroll_step_y_;
    return true;
}

void WorldRenderer::reset_navigation() {
    smooth_scroll_remaining_x_ = 0;
    smooth_scroll_remaining_y_ = 0;
    smooth_scroll_accumulated_x_ = 0;
    smooth_scroll_accumulated_y_ = 0;
    smooth_scroll_step_x_ = 0;
    smooth_scroll_step_y_ = 0;
}

void WorldRenderer::set_viewport_origin(int world_x, int world_y) {
    const int previous_left = viewport_left_;
    const int previous_top = viewport_top_;
    const int height = viewport_bottom_ - viewport_top_;
    viewport_left_ = wrap_world_x(world_x);
    viewport_top_ = clamp_vertical_origin(std::max(world_y, 0), height);
    viewport_right_ = viewport_left_ +
        (viewport_right_ - previous_left);
    viewport_bottom_ = viewport_top_ + height;
    host_.move_renderable_objects(viewport_left_ - previous_left,
                                  viewport_top_ - previous_top);
    host_.update_view_anchored_objects();
    host_.present_current_view(
        owner_window_,
        {viewport_left_, viewport_top_, viewport_right_, viewport_bottom_});
}

void* WorldRenderer::create_back_buffer_dib(int width, int height) {
    if (width <= 0 || height <= 0 || memory_dc_ == nullptr) {
        return nullptr;
    }
    std::uint8_t* pixels = nullptr;
    void* bitmap = host_.create_indexed_dib(memory_dc_, width, height,
                                            pixels);
    dib_section_ = bitmap;
    dib_pixels_ = pixels;
    if (bitmap == nullptr) {
        return nullptr;
    }
    void* previous_bitmap = host_.select_bitmap(memory_dc_, bitmap);
    update_dib_palette();
    previous_selected_object_ = previous_bitmap;
    return previous_bitmap;
}

void WorldRenderer::update_dib_palette() {
    if (memory_dc_ == nullptr || palette_ == nullptr) {
        return;
    }
    const auto source = host_.read_palette_entries(palette_);
    std::array<RendererDibColour, 0x100> colours{};
    for (std::size_t index = 0; index < colours.size(); ++index) {
        // UpdateDIBPalette @ 0x00413aa0 appears to swap red and blue, but it
        // does not: Ghidra types the GetPaletteEntries destination as RGBQUAD
        // when it holds PALETTEENTRY, so the decompile's `rgbBlue` is byte 0
        // (peRed) and its `rgbRed` is byte 2 (peBlue).  Read by offset the
        // native is a straight copy.  Transcribing its field NAMES onto
        // correctly named structs swaps the channels a second time, which is
        // what turned C1's blue sky salmon.
        colours[index].red = source[index].red;
        colours[index].green = source[index].green;
        colours[index].blue = source[index].blue;
    }
    host_.set_dib_colour_table(memory_dc_, colours);
}

int WorldRenderer::choose_scroll_step(int remaining,
                                      int accumulated,
                                      int previous_step) {
    if (remaining < 0) {
        if (remaining < -4) {
            if (remaining < accumulated - 4 + previous_step) {
                return previous_step - 4;
            }
            int step = -8;
            if (previous_step + 4 < -7) {
                step = previous_step + 4;
            }
            return step;
        }
        return remaining;
    }
    if (remaining == 0) {
        return 0;
    }
    if (remaining > 4) {
        if (accumulated + 4 + previous_step < remaining) {
            return previous_step + 4;
        }
        int step = 8;
        if (previous_step - 4 > 7) {
            step = previous_step - 4;
        }
        return step;
    }
    return remaining;
}

int WorldRenderer::wrap_world_x(int world_x) {
    const int wrapped = world_x % world::kWorldWidth;
    return wrapped < 0 ? wrapped + world::kWorldWidth : wrapped;
}

} // namespace creatures1::display
