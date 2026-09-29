#pragma once

#include "../display/frame_dump.hpp"
#include "../display/image_tiers.hpp"
#include "../display/rendering.hpp"
#include "../world/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace creatures1::platform {

class C1WindowsDocument;

// Not native (neorender).  Draws the world view through SDL3 into an
// existing window (the MFC view), from the same scene list the 8-bit
// software renderer blits: background tiles, sprites in plane order, the
// overlay.  The software renderer keeps its back buffer current underneath
// (snapshots read it); this only replaces what reaches the screen.
class SdlWorldView {
public:
    // Wraps `window` (an HWND).  `driver` picks an SDL render driver ("direct3d11",
    // "opengl", "software", ...) or leaves SDL to choose when empty.  Null
    // when SDL cannot draw into the window; the caller keeps using GDI.
    static std::unique_ptr<SdlWorldView> create(void* window,
                                                const std::string& driver);
    ~SdlWorldView();

    SdlWorldView(const SdlWorldView&) = delete;
    SdlWorldView& operator=(const SdlWorldView&) = delete;

    // One whole frame of `viewport` (world coordinates), `zoom` screen pixels
    // per world pixel.  With `capture` set, the frame is also read back
    // before it is shown.
    void render_frame(C1WindowsDocument& document,
                      const std::vector<display::SceneItem>& scene,
                      const world::WorldRect& viewport, float zoom,
                      const world::WorldRect* debug_highlight,
                      display::RgbFrame* capture);

    // Forget every texture (a new world, a new palette).
    void clear_textures();

    const char* driver_name() const;
    std::size_t texture_bytes() const { return texture_bytes_; }
    // Frames drawn so far.
    std::uint64_t frame_count() const { return frame_number_; }

private:
    SdlWorldView(SDL_Window* window, SDL_Renderer* renderer);

    struct TextureKey {
        const display::Image* image;
        display::SpriteFileId file;
        std::int32_t first_record;
        std::size_t index;
        display::ImageTier tier;
        bool opaque;
        bool operator<(const TextureKey& other) const {
            return std::tie(image, file, first_record, index, tier, opaque) <
                   std::tie(other.image, other.file, other.first_record,
                            other.index, other.tier, other.opaque);
        }
    };
    struct CachedTexture {
        SDL_Texture* texture = nullptr;
        std::uint32_t pixel_version = 0;
        std::size_t bytes = 0;
        std::uint64_t last_used = 0;
    };

    SDL_Texture* texture_for(C1WindowsDocument& document,
                             display::Gallery& gallery, std::size_t index,
                             display::ImageTier tier, bool opaque);
    SDL_Texture* upload(const std::uint8_t* rgba, int width, int height,
                        std::size_t& bytes);
    void evict_to_budget();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::map<TextureKey, CachedTexture> textures_;
    std::size_t texture_bytes_ = 0;
    std::uint64_t frame_number_ = 0;
    std::vector<std::uint8_t> scratch_rgba_;
};

} // namespace creatures1::platform
