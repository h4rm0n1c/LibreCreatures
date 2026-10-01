#include "windows_shell.hpp"

#include "sdl_world_view.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace creatures1::platform {
namespace {

// Textures kept on the GPU before the least recently drawn are dropped.
constexpr std::size_t kTextureBudgetBytes = 256u * 1024u * 1024u;

bool ensure_sdl_video() {
    static bool attempted = false;
    static bool ready = false;
    if (attempted) {
        return ready;
    }
    attempted = true;
    // SDL is a guest in an MFC window: MFC runs the message loop, owns the
    // mouse capture, the menus and Alt+F4.  SDL only draws.
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
    SDL_SetHint(SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, "0");
    ready = SDL_Init(SDL_INIT_VIDEO);
    return ready;
}

// The on-screen position of something at world_x, with the world's
// horizontal wrap: as Image::blit_to_dib does it, the copy nearest the
// viewport, moved a world-width either way if that is the one that shows.
int wrapped_screen_x(int world_x, int width, const world::WorldRect& view) {
    const int view_width = view.max_x - view.min_x;
    int x = world_x - view.min_x;
    if (x > world::kWorldWidth / 2) {
        x -= world::kWorldWidth;
    } else if (x < -(world::kWorldWidth / 2)) {
        x += world::kWorldWidth;
    }
    const auto shows = [&](int candidate) {
        return candidate < view_width && candidate + width > 0;
    };
    if (!shows(x)) {
        if (shows(x + world::kWorldWidth)) {
            x += world::kWorldWidth;
        } else if (shows(x - world::kWorldWidth)) {
            x -= world::kWorldWidth;
        }
    }
    return x;
}

// The window property that holds SDL's procedure for the cursor hook.
constexpr char kSdlWindowProcProperty[] = "LibreCreatures.SdlWindowProc";

// SDL's own procedure answers WM_SETCURSOR over the client area with SDL's
// cursor, the system arrow, so the Windows pointer showed through the hand
// each time Windows asked.  The window class's cursor is what Windows would
// have used: blank for the world view (resource 132, as in the original),
// the arrow for the eye view.
LRESULT CALLBACK class_cursor_window_proc(HWND window, UINT message,
                                          WPARAM w_param, LPARAM l_param) {
    const auto sdl_proc = reinterpret_cast<WNDPROC>(
        ::GetPropA(window, kSdlWindowProcProperty));
    if (message == WM_SETCURSOR && LOWORD(l_param) == HTCLIENT) {
        ::SetCursor(reinterpret_cast<HCURSOR>(
            ::GetClassLongPtrA(window, GCLP_HCURSOR)));
        return TRUE;
    }
    return sdl_proc == nullptr
               ? ::DefWindowProcA(window, message, w_param, l_param)
               : ::CallWindowProcA(sdl_proc, window, message, w_param,
                                   l_param);
}

} // namespace

std::unique_ptr<SdlWorldView> SdlWorldView::create(void* window,
                                                   const std::string& driver) {
    if (window == nullptr || !ensure_sdl_video()) {
        return nullptr;
    }
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties,
                           SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, window);
    SDL_Window* sdl_window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (sdl_window == nullptr) {
        return nullptr;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(
        sdl_window, driver.empty() ? nullptr : driver.c_str());
    if (renderer == nullptr) {
        SDL_DestroyWindow(sdl_window);
        return nullptr;
    }
    std::unique_ptr<SdlWorldView> view(new SdlWorldView(sdl_window, renderer));
    view->hook_cursor(window);
    return view;
}

void SdlWorldView::hook_cursor(void* window) {
    HWND hwnd = static_cast<HWND>(window);
    const LONG_PTR sdl_proc = ::GetWindowLongPtrA(hwnd, GWLP_WNDPROC);
    if (sdl_proc == 0 ||
        !::SetPropA(hwnd, kSdlWindowProcProperty,
                    reinterpret_cast<HANDLE>(sdl_proc))) {
        return;
    }
    ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(&class_cursor_window_proc));
    hooked_window_ = window;
    sdl_window_proc_ = sdl_proc;
}

void SdlWorldView::unhook_cursor() {
    if (hooked_window_ == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(hooked_window_);
    // SDL puts MFC's procedure back when it lets go of the window, so its
    // own must be back in place first.
    if (::GetWindowLongPtrA(hwnd, GWLP_WNDPROC) ==
        reinterpret_cast<LONG_PTR>(&class_cursor_window_proc)) {
        ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, sdl_window_proc_);
    }
    ::RemovePropA(hwnd, kSdlWindowProcProperty);
    hooked_window_ = nullptr;
}

SdlWorldView::SdlWorldView(SDL_Window* window, SDL_Renderer* renderer)
    : window_(window), renderer_(renderer) {}

SdlWorldView::~SdlWorldView() {
    unhook_cursor();
    clear_textures();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        // A wrapped window is only let go of, not destroyed: MFC owns it.
        SDL_DestroyWindow(window_);
    }
}

const char* SdlWorldView::driver_name() const {
    const char* name = SDL_GetRendererName(renderer_);
    return name == nullptr ? "" : name;
}

void SdlWorldView::clear_textures() {
    for (auto& [key, cached] : textures_) {
        if (cached.texture != nullptr) {
            SDL_DestroyTexture(cached.texture);
        }
    }
    textures_.clear();
    texture_bytes_ = 0;
}

SDL_Texture* SdlWorldView::upload(const std::uint8_t* rgba, int width,
                                  int height, std::size_t& bytes) {
    SDL_Texture* texture =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                          SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture == nullptr) {
        return nullptr;
    }
    if (!SDL_UpdateTexture(texture, nullptr, rgba, width * 4)) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    return texture;
}

SDL_Texture* SdlWorldView::texture_for(C1WindowsDocument& document,
                                       display::Gallery& gallery,
                                       std::size_t index,
                                       display::ImageTier tier, bool opaque) {
    display::Image& image = gallery.images[index];
    const TextureKey key{&image, gallery.sprite_file_id,
                         gallery.header_record_index, index, tier, opaque};
    auto found = textures_.find(key);
    if (found != textures_.end()) {
        if (found->second.content_serial == image.content_serial() &&
            (tier != display::ImageTier::spr ||
             found->second.pixel_version == image.pixel_version())) {
            found->second.last_used = frame_number_;
            return found->second.texture;
        }
        // The game has drawn into the image since it was uploaded, or it is
        // a new image where an old one was (a creature's body gallery,
        // rebuilt as it ages).
        SDL_DestroyTexture(found->second.texture);
        texture_bytes_ -= found->second.bytes;
        textures_.erase(found);
    }

    CachedTexture cached;
    cached.pixel_version = image.pixel_version();
    cached.content_serial = image.content_serial();
    cached.last_used = frame_number_;
    if (tier == display::ImageTier::spr) {
        const std::uint8_t* pixels = document.indexed_image_pixels(image);
        const int width = image.width();
        const int height = image.height();
        if (pixels == nullptr || width <= 0 || height <= 0) {
            return nullptr;
        }
        const auto& palette = document.frame_palette();
        scratch_rgba_.resize(static_cast<std::size_t>(width) * height * 4u);
        std::uint8_t* out = scratch_rgba_.data();
        for (std::size_t i = 0, n = static_cast<std::size_t>(width) * height;
             i < n; ++i, out += 4) {
            const std::uint8_t value = pixels[i];
            const display::FrameColour& colour = palette[value];
            out[0] = colour.red;
            out[1] = colour.green;
            out[2] = colour.blue;
            // The 8-bit blitter skips index 0 on sprites and copies it on
            // background tiles; alpha says the same thing.
            out[3] = (value == 0 && !opaque) ? 0 : 255;
        }
        cached.texture = upload(scratch_rgba_.data(), width, height,
                                cached.bytes);
    } else {
        display::RgbaImage decoded;
        if (!document.decode_image_tier(gallery, index, tier, decoded)) {
            return nullptr;
        }
        cached.texture = upload(decoded.rgba.data(), decoded.width,
                                decoded.height, cached.bytes);
    }
    if (cached.texture == nullptr) {
        return nullptr;
    }
    SDL_SetTextureBlendMode(cached.texture,
                            opaque ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    texture_bytes_ += cached.bytes;
    SDL_Texture* texture = cached.texture;
    textures_.emplace(key, cached);
    return texture;
}

void SdlWorldView::evict_to_budget() {
    while (texture_bytes_ > kTextureBudgetBytes && !textures_.empty()) {
        auto oldest = std::min_element(
            textures_.begin(), textures_.end(),
            [](const auto& left, const auto& right) {
                return left.second.last_used < right.second.last_used;
            });
        if (oldest->second.last_used == frame_number_) {
            return;  // everything left is on screen now
        }
        SDL_DestroyTexture(oldest->second.texture);
        texture_bytes_ -= oldest->second.bytes;
        textures_.erase(oldest);
    }
}

void SdlWorldView::render_frame(C1WindowsDocument& document,
                                const std::vector<display::SceneItem>& scene,
                                const world::WorldRect& viewport, float zoom,
                                const world::WorldRect* debug_highlight,
                                display::RgbFrame* capture) {
    ++frame_number_;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    for (const display::SceneItem& item : scene) {
        display::Gallery* gallery = item.gallery;
        if (gallery == nullptr || gallery->images == nullptr ||
            item.image_index >= gallery->image_count) {
            continue;
        }
        const display::Image& image = gallery->images[item.image_index];
        const int width = image.width();
        const int height = image.height();
        if (width <= 0 || height <= 0) {
            continue;
        }
        display::ImageTier tier =
            document.draw_tier_for(*gallery, item.image_index, zoom);
        SDL_Texture* texture = texture_for(document, *gallery, item.image_index,
                                           tier, item.opaque);
        if (texture == nullptr && tier != display::ImageTier::spr) {
            tier = display::ImageTier::spr;
            texture = texture_for(document, *gallery, item.image_index, tier,
                                  item.opaque);
        }
        if (texture == nullptr) {
            continue;
        }
        // Whole-number magnification of a tier's pixels stays crisp
        // (nearest); anything else is filtered.
        const float scale =
            zoom / static_cast<float>(display::tier_scale(tier));
        SDL_SetTextureScaleMode(texture,
                                scale >= 1.0f && scale == std::floor(scale)
                                    ? SDL_SCALEMODE_NEAREST
                                    : SDL_SCALEMODE_LINEAR);
        const int x = wrapped_screen_x(item.world_x, width, viewport);
        const int y = item.world_y - viewport.min_y;
        // Edges on whole screen pixels, so neighbouring background tiles
        // meet exactly at any zoom.
        const float left = std::round(static_cast<float>(x) * zoom);
        const float top = std::round(static_cast<float>(y) * zoom);
        const SDL_FRect destination{
            left, top,
            std::round(static_cast<float>(x + width) * zoom) - left,
            std::round(static_cast<float>(y + height) * zoom) - top};
        SDL_RenderTexture(renderer_, texture, nullptr, &destination);
    }

    if (debug_highlight != nullptr && debug_highlight->max_x != 0) {
        SDL_SetRenderDrawColor(renderer_, 255, 0, 255, 255);
        const SDL_FRect outline{
            static_cast<float>(debug_highlight->min_x - viewport.min_x) * zoom,
            static_cast<float>(debug_highlight->min_y - viewport.min_y) * zoom,
            static_cast<float>(debug_highlight->max_x - debug_highlight->min_x) *
                zoom,
            static_cast<float>(debug_highlight->max_y - debug_highlight->min_y) *
                zoom};
        SDL_RenderRect(renderer_, &outline);
    }

    if (capture != nullptr) {
        *capture = {};
        const SDL_Rect area{
            0, 0,
            static_cast<int>(static_cast<float>(viewport.max_x - viewport.min_x) * zoom),
            static_cast<int>(static_cast<float>(viewport.max_y - viewport.min_y) * zoom)};
        if (SDL_Surface* read = SDL_RenderReadPixels(renderer_, &area)) {
            if (SDL_Surface* xrgb =
                    SDL_ConvertSurface(read, SDL_PIXELFORMAT_XRGB8888)) {
                capture->width = xrgb->w;
                capture->height = xrgb->h;
                capture->pixels.resize(static_cast<std::size_t>(xrgb->w) *
                                       static_cast<std::size_t>(xrgb->h));
                for (int row = 0; row < xrgb->h; ++row) {
                    const auto* source = reinterpret_cast<const std::uint32_t*>(
                        static_cast<const std::uint8_t*>(xrgb->pixels) +
                        static_cast<std::ptrdiff_t>(row) * xrgb->pitch);
                    for (int column = 0; column < xrgb->w; ++column) {
                        capture->pixels[static_cast<std::size_t>(row) * xrgb->w +
                                        column] = source[column] & 0x00ffffffu;
                    }
                }
                SDL_DestroySurface(xrgb);
            }
            SDL_DestroySurface(read);
        }
    }

    SDL_RenderPresent(renderer_);
    // SDL queues events for the window it wraps; MFC is the one handling
    // them, so SDL's copies are dropped.
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    evict_to_budget();
}

} // namespace creatures1::platform
