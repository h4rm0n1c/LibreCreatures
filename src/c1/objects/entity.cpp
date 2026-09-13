#include "entity.hpp"

#include <array>
#include <algorithm>
#include <memory>
#include <new>

namespace creatures1::objects {

Entity::Entity() : Entity(nullptr) {}

Entity::Entity(EntityRegistryHost* registry) : registry_(registry) {
    if (registry_ != nullptr) {
        registry_->add_entity(*this);
    }
}

std::unique_ptr<Entity> Entity::create_object(EntityRegistryHost* registry) {
    return std::unique_ptr<Entity>(new (std::nothrow) Entity(registry));
}

Entity::~Entity() {
    if (registry_ != nullptr) {
        unregister_from_registry(*registry_);
    }
}

void Entity::unregister_from_registry(EntityRegistryHost& registry) {
    for (std::size_t index = 0; index < registry.entity_count(); ++index) {
        if (registry.entity_at(index) != this) {
            continue;
        }
        registry.remove_entity_at(index);
        registry_ = nullptr;
        return;
    }
    registry_ = nullptr;
}

void Entity::serialize(EntityArchive& archive) {
    if (archive.is_loading()) {
        gallery_ = archive.read_gallery();
        current_image_index_ = archive.read_byte();
        image_index_base_ = archive.read_byte();
        render_plane_ = archive.read_int32();
        world_x_ = archive.read_int32();
        world_y_ = archive.read_int32();

        const bool has_image_sequence = archive.read_byte() != 0;
        image_sequence_.reset();
        image_sequence_cursor_ = 0;
        if (has_image_sequence) {
            image_sequence_cursor_ = archive.read_byte();
            image_sequence_ = std::make_unique<ImageSequenceBuffer>();
            archive.read_bytes(image_sequence_->data(), image_sequence_->size());
        }
        return;
    }

    archive.write_gallery(gallery_);
    archive.write_byte(current_image_index_);
    archive.write_byte(image_index_base_);
    archive.write_int32(render_plane_);
    archive.write_int32(world_x_);
    archive.write_int32(world_y_);

    const bool has_image_sequence = image_sequence_ != nullptr;
    archive.write_byte(has_image_sequence ? 1 : 0);
    if (!has_image_sequence) {
        return;
    }

    archive.write_byte(static_cast<std::uint8_t>(image_sequence_cursor_));
    std::array<char, 32> sequence_bytes{};
    std::copy_n(image_sequence_->data(), sequence_bytes.size(),
                sequence_bytes.data());
    archive.write_bytes(sequence_bytes.data(), sequence_bytes.size());
}

void Entity::get_current_image_bounds(world::WorldRect& out_bounds) const {
    if (current_image_index_ > gallery_->image_count) {
        out_bounds = {};
        return;
    }

    out_bounds.min_x = world_x_;
    out_bounds.min_y = world_y_;
    out_bounds.max_x = world_x_ + current_image_width();
    out_bounds.max_y = world_y_ + current_image_height();
}

void Entity::advance_image_sequence(EntityImageSequenceRenderHost& renderer) {
    if (image_sequence_ == nullptr || image_sequence_is_empty()) {
        return;
    }

    world::WorldRect old_bounds{};
    get_current_image_bounds(old_bounds);

    if ((*image_sequence_)[image_sequence_cursor_] == 'R') {
        image_sequence_cursor_ = 0;
    }
    const char sequence_byte = (*image_sequence_)[image_sequence_cursor_];
    const int mapped_index =
        static_cast<int>(image_index_base_) +
        static_cast<int>(sequence_byte);
    const int next_image_index =
        sequence_byte < 'a' ? mapped_index - '0' : mapped_index - 'W';
    ++image_sequence_cursor_;
    current_image_index_ = static_cast<std::uint8_t>(next_image_index);

    world::WorldRect new_bounds{};
    get_current_image_bounds(new_bounds);
    renderer.redraw_image_sequence_change(*this, old_bounds, new_bounds);
}

void Entity::set_image_index_and_redraw(
    std::uint8_t image_index, EntityImageSequenceRenderHost& renderer) {
    world::WorldRect old_bounds{};
    get_current_image_bounds(old_bounds);

    // The absolute form moves the base as well, so a later relative select
    // is measured from the newly chosen image.
    image_index_base_ = image_index;
    current_image_index_ = image_index;

    world::WorldRect new_bounds{};
    get_current_image_bounds(new_bounds);
    renderer.redraw_image_sequence_change(*this, old_bounds, new_bounds);
}

void Entity::set_relative_image_index_and_redraw(
    std::int32_t relative_index, EntityImageSequenceRenderHost& renderer) {
    world::WorldRect old_bounds{};
    get_current_image_bounds(old_bounds);

    // Selecting an image explicitly abandons any running sequence; the base
    // is left alone so the relative index stays anchored to it.
    image_sequence_.reset();
    image_sequence_cursor_ = 0;
    current_image_index_ = static_cast<std::uint8_t>(
        static_cast<std::int32_t>(image_index_base_) + relative_index);

    world::WorldRect new_bounds{};
    get_current_image_bounds(new_bounds);
    renderer.redraw_image_sequence_change(*this, old_bounds, new_bounds);
}

void Entity::advance_image_sequence_for_moving_vehicle() {
    if (image_sequence_ == nullptr || image_sequence_is_empty()) {
        return;
    }

    char sequence_byte = (*image_sequence_)[image_sequence_cursor_];
    if (sequence_byte == 'R') {
        image_sequence_cursor_ = 0;
        sequence_byte = (*image_sequence_)[image_sequence_cursor_];
    }

    // The moving Vehicle path uses the executable's direct byte formula;
    // unlike Entity::advance_image_sequence it does not map lowercase
    // sequence bytes through the general animation convention.
    current_image_index_ = static_cast<std::uint8_t>(
        static_cast<int>(image_index_base_) - '0' +
        static_cast<unsigned char>(sequence_byte));
    ++image_sequence_cursor_;
}

void Entity::fill_current_image_rect(std::uint8_t palette_index,
                                     int min_x, int min_y,
                                     int max_x, int max_y,
                                     EntityRasterHost& raster) {
    int image_width = 0;
    int image_height = 0;
    std::uint8_t* pixels =
        raster.current_image_pixels(*this, image_width, image_height);
    (void)image_height;
    if (pixels == nullptr) {
        return;
    }

    for (int y = min_y; y < max_y; ++y) {
        std::uint8_t* row = pixels + y * image_width;
        for (int x = min_x; x < max_x; ++x) {
            row[x] = palette_index;
        }
    }
}

void Entity::draw_text_to_current_image(int pixel_x, int pixel_y,
                                        const char* text,
                                        std::uint8_t palette_index_0,
                                        std::uint8_t palette_index_1,
                                        std::uint8_t palette_index_2,
                                        EntityRasterHost& raster) {
    int image_width = 0;
    int image_height = 0;
    std::uint8_t* pixels =
        raster.current_image_pixels(*this, image_width, image_height);
    (void)image_height;
    if (pixels == nullptr || text == nullptr) {
        return;
    }

    const std::array<std::uint8_t, 3> palette = {
        palette_index_0, palette_index_1, palette_index_2};
    const int line_start_x = pixel_x;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const std::uint8_t character_code =
            static_cast<std::uint8_t>(*cursor);
        if (character_code == static_cast<std::uint8_t>('|')) {
            pixel_y += 12;
            pixel_x = line_start_x;
            continue;
        }

        const std::uint8_t* glyph =
            raster.charset_glyph_rows(character_code);
        if (glyph != nullptr) {
            for (int row = 0; row < 12; ++row) {
                std::uint8_t* destination =
                    pixels + (pixel_y + row) * image_width + pixel_x;
                for (int column = 0; column < 6; ++column) {
                    destination[column] = palette[glyph[row * 6 + column]];
                }
            }
        }
        pixel_x += 1 + raster.charset_glyph_advance_width(character_code);
    }
}

} // namespace creatures1::objects
