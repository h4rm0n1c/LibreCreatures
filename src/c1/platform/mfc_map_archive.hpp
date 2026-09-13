#pragma once

#include "mfc_archive.hpp"

#include "../world/map.hpp"

#include <functional>
#include <utility>

namespace creatures1::platform {

// MapData's archive contract is the first concrete composition point below
// CArchive. Gallery identity remains an application/runtime concern; these
// callbacks do not allocate a second gallery registry or expose MFC classes
// to the clean world model.
class MfcMapDataArchive final : public world::MapDataArchive {
public:
    using GalleryReader = std::function<display::Gallery*()>;
    using GalleryWriter = std::function<void(display::Gallery*)>;

    MfcMapDataArchive(MfcArchiveStream& stream,
                      GalleryReader read_gallery,
                      GalleryWriter write_gallery)
        : stream_(stream),
          read_gallery_(std::move(read_gallery)),
          write_gallery_(std::move(write_gallery)) {}

    bool is_loading() const override;
    display::Gallery* read_gallery() override;
    void write_gallery(display::Gallery* gallery) override;
    std::uint8_t read_byte() override;
    std::int32_t read_int32() override;
    std::uint32_t read_uint32() override;
    void write_byte(std::uint8_t value) override;
    void write_int32(std::int32_t value) override;
    void write_uint32(std::uint32_t value) override;

private:
    MfcArchiveStream& stream_;
    GalleryReader read_gallery_;
    GalleryWriter write_gallery_;
};

} // namespace creatures1::platform
