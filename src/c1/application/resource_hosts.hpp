#pragma once

#include "resources.hpp"
#include "../creatures/body.hpp"
#include "../display/gallery.hpp"
#include "../display/palette.hpp"
#include "../display/sprite_cache.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::application {

// The application owns the directory slots recovered from SFCApp.  Paths are
// stored with the native trailing separator, just as the original string
// concatenation expects.
struct C1ResourceDirectories {
    std::array<std::string, kPrimaryResourceDirectoryCount>
        primary_resource_directories{};
    std::string body_data_directory;
};

// This is the platform file boundary.  A Windows build can implement it with
// MFC CFile/ifstream-compatible handles; the clean C1 owners below never see
// HANDLE, CFile, or MSVC stream layout.
class ResourceReadFile {
public:
    virtual ~ResourceReadFile() = default;
    virtual bool seek_from_beginning(std::uint32_t byte_offset) = 0;
    virtual bool read_exact(std::uint8_t* destination,
                            std::size_t byte_count) = 0;
};

class ResourceWriteFile {
public:
    virtual ~ResourceWriteFile() = default;
    virtual void write(const std::uint8_t* bytes, std::size_t byte_count) = 0;
    virtual bool seek_from_beginning() = 0;
    virtual void close() = 0;
};

class ResourceFileBackend {
public:
    virtual ~ResourceFileBackend() = default;
    virtual std::unique_ptr<ResourceReadFile> open_for_read(
        std::string_view path) = 0;
    virtual std::unique_ptr<ResourceWriteFile> open_for_create(
        std::string_view path) = 0;
    virtual bool regular_file_exists(std::string_view path) const = 0;
    virtual bool read_binary_file(std::string_view path,
                                  std::vector<std::uint8_t>& bytes) const = 0;
    virtual bool read_text_file(std::string_view path,
                                std::string& text) const = 0;
};

// Portable backend used by the clean project and its host-level tests.  The
// Windows/MFC executable may substitute a backend that preserves the native
// CFile open/share flags without changing any C1 owner code.
class StandardResourceFileBackend final : public ResourceFileBackend {
public:
    std::unique_ptr<ResourceReadFile> open_for_read(
        std::string_view path) override;
    std::unique_ptr<ResourceWriteFile> open_for_create(
        std::string_view path) override;
    bool regular_file_exists(std::string_view path) const override;
    bool read_binary_file(std::string_view path,
                          std::vector<std::uint8_t>& bytes) const override;
    bool read_text_file(std::string_view path,
                        std::string& text) const override;
};

// Concrete application-side composition for the resource stages used by
// Skeleton::LoadGenome.  It supplies BodyResourceHost, GalleryHost, the
// binary/palette file interfaces, the temporary SPR output interface, and the
// native gallery registry boundary without moving any format logic here.
class C1ResourceHost final : public creatures1::creatures::BodyResourceHost,
                             public creatures1::display::GalleryHost,
                             public creatures1::display::GalleryRegistry,
                             public creatures1::display::BinaryResourceFileSystem,
                             public creatures1::display::SpriteIndexOutputFileSystem {
public:
    C1ResourceHost(C1ResourceDirectories directories,
                   ResourceFileBackend& files);
    ~C1ResourceHost() override;

    std::string resource_directory(int directory_index) const override;
    std::string body_data_directory() const override;
    bool regular_file_exists(std::string_view path) const override;
    bool read_text_file(std::string_view path,
                        std::string& text) const override;

    std::string sprite_metadata_path(
        std::string_view directory,
        creatures1::display::SpriteFileId file_id) const override;
    std::vector<creatures1::display::GalleryImageMetadata>
        read_image_metadata(std::string_view path,
                            std::size_t file_offset,
                            std::size_t image_count) override;

    std::size_t gallery_count() const override;
    creatures1::display::Gallery* gallery_at(
        std::size_t index) const override;
    creatures1::display::Gallery* add_gallery(
        std::unique_ptr<creatures1::display::Gallery> gallery) override;

    std::unique_ptr<creatures1::display::BinaryResourceFile> open_for_read(
        std::string_view path) override;
    std::unique_ptr<creatures1::display::SpriteIndexOutputFile>
        open_for_create(std::string_view path) override;

    // Gallery release is an application registry operation.  Skeleton calls
    // it through its lifetime host; this public operation lets that adapter
    // remove a zero-reference gallery without exposing registry storage to
    // the display format code.
    bool release_gallery(creatures1::display::Gallery& gallery);
    void clear_galleries();

private:
    C1ResourceDirectories directories_;
    ResourceFileBackend& files_;
    std::vector<std::unique_ptr<creatures1::display::Gallery>> galleries_;
};

// PaletteDtaFileSystem and BinaryResourceFileSystem intentionally retain the
// native method name open_for_read, but their return types are different. C++
// cannot implement both in one class, so palette loading gets this separate
// application adapter rather than a type-erased or duplicated file path.
class C1PaletteDtaHost final : public creatures1::display::PaletteDtaFileSystem {
public:
    explicit C1PaletteDtaHost(ResourceFileBackend& files);

    std::unique_ptr<creatures1::display::PaletteDtaFile> open_for_read(
        std::string_view path) override;

private:
    ResourceFileBackend& files_;
};

} // namespace creatures1::application
