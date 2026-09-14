#pragma once

#include "../display/image.hpp"
#include "../world/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace creatures1::objects {

class Entity;

// Entity registration is an application-owned concern.  The native C1
// factory inserted each Entity into an MFC pointer array and the destructor
// removed it; this interface preserves that lifetime contract without
// leaking CPtrArray layout into the Entity class.
class EntityRegistryHost {
public:
    virtual ~EntityRegistryHost() = default;
    virtual void add_entity(Entity& entity) = 0;
    virtual std::size_t entity_count() const = 0;
    virtual Entity* entity_at(std::size_t index) const = 0;
    virtual void remove_entity_at(std::size_t index) = 0;
    virtual void report_invalid_index() const = 0;
};

// The archive stream owns buffering, dynamic-object framing, and MFC error
// policy. Entity owns the proven field order and the optional 32-byte image
// sequence payload.
class EntityArchive {
public:
    virtual ~EntityArchive() = default;
    virtual bool is_loading() const = 0;
    virtual display::Gallery* read_gallery() = 0;
    virtual void write_gallery(display::Gallery* gallery) = 0;
    virtual std::uint8_t read_byte() = 0;
    virtual std::int32_t read_int32() = 0;
    virtual void write_byte(std::uint8_t value) = 0;
    virtual void write_int32(std::int32_t value) = 0;
    virtual void read_bytes(void* destination, std::size_t count) = 0;
    virtual void write_bytes(const void* source, std::size_t count) = 0;
    // Dynamic object framing is supplied by the archive/runtime boundary.
    // Body serialization uses this for the polymorphic Limb chain reference;
    // the clean game model does not reproduce MFC's CArchive object table.
    virtual void* read_object_reference(
        std::string_view runtime_class_name) = 0;
    virtual void write_object_reference(
        const void* object, std::string_view runtime_class_name) = 0;
};

// Image loading is a display/cache concern. Entity only supplies the image
// selected by a sequence; the host performs the same cache-warming operation
// as the original CImage::GetPixelData call.
class ImagePreloadHost {
public:
    virtual ~ImagePreloadHost() = default;
    virtual void preload_image(const display::Image& image) = 0;
};

// Entity owns the sequence cursor and gallery-index transition. The renderer
// owns viewport clipping, world-wrap dirty unions, DC lifetime, and its
// deferred 64-entry queue; it receives the old and new sprite rectangles.
class EntityImageSequenceRenderHost {
public:
    virtual ~EntityImageSequenceRenderHost() = default;
    virtual void redraw_image_sequence_change(
        Entity& entity, const world::WorldRect& old_bounds,
        const world::WorldRect& new_bounds) = 0;
};

// Pixel storage and CHARSET.DTA ownership belong to the display subsystem.
// Entity owns only the indexed-image traversal and palette selection policy.
class EntityRasterHost {
public:
    virtual ~EntityRasterHost() = default;
    virtual std::uint8_t* current_image_pixels(Entity& entity,
                                                int& out_width,
                                                int& out_height) = 0;
    virtual const std::uint8_t* charset_glyph_rows(
        std::uint8_t character_code) const = 0;
    virtual int charset_glyph_advance_width(
        std::uint8_t character_code) const = 0;
};

// The recovered 32-byte C1 Entity record: gallery/image selection, render
// order, world position, and two pointers for the optional image sequence.
// The owning allocation is represented explicitly so an empty parsed sequence
// remains distinct from an Entity that has never received an `anim` sequence.
class Entity {
public:
    static constexpr std::size_t kImageSequenceCapacity = 0x20;
    using ImageSequenceBuffer = std::array<char, kImageSequenceCapacity>;

    static std::unique_ptr<Entity> create_object(
        EntityRegistryHost* registry = nullptr);

    Entity();
    explicit Entity(EntityRegistryHost* registry);
    virtual ~Entity();

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    int world_x() const { return world_x_; }
    int world_y() const { return world_y_; }
    void set_world_x(int value) { world_x_ = value; }
    void set_world_y(int value) { world_y_ = value; }
    void move_to(int x, int y) {
        world_x_ = x;
        world_y_ = y;
    }

    int render_plane() const { return render_plane_; }
    void set_render_plane(int value) { render_plane_ = value; }

    void set_gallery(display::Gallery* gallery) { gallery_ = gallery; }
    display::Gallery* gallery() const { return gallery_; }
    std::uint8_t current_image_index() const { return current_image_index_; }
    std::uint8_t image_index_base() const { return image_index_base_; }
    void set_image_index(std::uint8_t index) { current_image_index_ = index; }
    void set_image_index_base(std::uint8_t index) { image_index_base_ = index; }

    bool has_current_image() const {
        return gallery_ != nullptr && gallery_->images != nullptr &&
               current_image_index_ < gallery_->image_count;
    }

    const display::Image& current_image() const {
        return gallery_->images[current_image_index_];
    }
    int current_image_width() const {
        return has_current_image() ? current_image().width() : 0;
    }
    int current_image_height() const {
        return has_current_image() ? current_image().height() : 0;
    }

    char* parse_image_sequence(char* sequence_text) {
        char* read_cursor = sequence_text + 1;
        if (image_sequence_ == nullptr) {
            image_sequence_ = std::make_unique<ImageSequenceBuffer>();
        }
        char* write_cursor = image_sequence_->data();
        while (*read_cursor != ']') {
            *write_cursor++ = *read_cursor;
            ++read_cursor;
        }
        *write_cursor = '\0';
        image_sequence_cursor_ = 0;
        return read_cursor + 2;
    }

    bool image_sequence_is_empty() const {
        return image_sequence_ == nullptr ||
               image_sequence_cursor_ >= image_sequence_->size() ||
               (*image_sequence_)[image_sequence_cursor_] == '\0';
    }
    int relative_image_index() const {
        return static_cast<int>(current_image_index_) -
               static_cast<int>(image_index_base_);
    }
    void get_current_image_bounds(world::WorldRect& out_bounds) const;
    void advance_image_sequence(EntityImageSequenceRenderHost& renderer);
    // Vehicle::Tick uses the native direct cursor update while moving. It
    // deliberately does not perform the stationary path's redraw.
    void advance_image_sequence_for_moving_vehicle();

    // Native image selection captures the sprite bounds either side of the
    // index change and hands both rectangles to the renderer, which owns the
    // wrapped dirty union and viewport clipping.
    void set_image_index_and_redraw(std::uint8_t image_index,
                                    EntityImageSequenceRenderHost& renderer);
    void set_relative_image_index_and_redraw(
        std::int32_t relative_index, EntityImageSequenceRenderHost& renderer);
    void unregister_from_registry(EntityRegistryHost& registry);
    void serialize(EntityArchive& archive);
    void fill_current_image_rect(std::uint8_t palette_index,
                                 int min_x, int min_y, int max_x, int max_y,
                                 EntityRasterHost& raster);
    void draw_text_to_current_image(int pixel_x, int pixel_y,
                                    const char* text,
                                    std::uint8_t palette_index_0,
                                    std::uint8_t palette_index_1,
                                    std::uint8_t palette_index_2,
                                    EntityRasterHost& raster);
    char* preload_image_sequence(char* sequence_text,
                                 ImagePreloadHost& preload_host) const {
        char* read_cursor = sequence_text + 1;
        while (*read_cursor != ']') {
            const int image_index =
                static_cast<int>(*read_cursor) + image_index_base_ - '0';
            if (gallery_ != nullptr && gallery_->images != nullptr &&
                image_index >= 0 &&
                static_cast<std::uint32_t>(image_index) < gallery_->image_count) {
                preload_host.preload_image(gallery_->images[image_index]);
            }
            ++read_cursor;
        }
        return read_cursor + 2;
    }

private:
    display::Gallery* gallery_ = nullptr;
    std::uint8_t current_image_index_ = 0;
    std::uint8_t image_index_base_ = 0;
    int render_plane_ = 0;
    int world_x_ = 0;
    int world_y_ = 0;
    std::unique_ptr<ImageSequenceBuffer> image_sequence_;
    std::size_t image_sequence_cursor_ = 0;
    EntityRegistryHost* registry_ = nullptr;
};

} // namespace creatures1::objects
