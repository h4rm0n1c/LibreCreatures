#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace creatures1::display {

struct PaletteColour {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct PaletteDtaBuffer {
    std::array<PaletteColour, 0xec> colours{};
};

struct PaletteRemapTable {
    std::array<std::uint8_t, 0x100> remapped_palette_index{};
};

struct PaletteChannelRamp {
    std::array<std::uint8_t, 0x100> intensity_by_control{};
};

struct LogicalPaletteEntry {
    PaletteColour colour{};
    std::uint8_t flags = 0;
};

struct LogicalPalette {
    std::uint16_t version = 0x300;
    std::uint16_t entry_count = 0x100;
    std::array<LogicalPaletteEntry, 0x100> entries{};
};

class PaletteDtaFile {
public:
    virtual ~PaletteDtaFile() = default;
    virtual bool skip(std::size_t byte_count) = 0;
    virtual bool read_exact(std::uint8_t* destination,
                            std::size_t byte_count) = 0;
};

class PaletteDtaFileSystem {
public:
    virtual ~PaletteDtaFileSystem() = default;
    virtual std::unique_ptr<PaletteDtaFile> open_for_read(
        std::string_view path) = 0;
};

class NativePalette {
public:
    virtual ~NativePalette() = default;
};

class PalettePlatform {
public:
    virtual ~PalettePlatform() = default;

    // The implementation owns the screen DC and native palette selection;
    // this is the narrow boundary around GetDC/CreatePalette/SelectPalette/
    // RealizePalette/GetSystemPaletteEntries/ReleaseDC.
    virtual void prime_system_palette(const LogicalPalette& probe) = 0;
    virtual void fill_system_reserved_entries(LogicalPalette& palette) = 0;
    virtual std::unique_ptr<NativePalette> create_palette(
        const LogicalPalette& palette) = 0;
};

struct GamePaletteState {
    std::array<PaletteDtaBuffer, 4> dta_buffers{};
    std::unique_ptr<NativePalette> native_palette;
};

bool load_palette_dta_into_buffer(PaletteDtaBuffer& output,
                                  PaletteDtaFileSystem& files,
                                  std::string_view palette_directory);

void build_palette_channel_ramp(PaletteChannelRamp& output,
                                int low_intensity,
                                int middle_intensity,
                                int high_intensity);

void build_creature_palette_remap(PaletteRemapTable& output,
                                  const PaletteDtaBuffer& palette,
                                  std::uint8_t red_control,
                                  std::uint8_t green_control,
                                  std::uint8_t blue_control,
                                  std::uint8_t hue_rotation_control,
                                  std::uint8_t colour_swap_control,
                                  std::uint32_t& build_count);

bool initialize_game_palette(GamePaletteState& state,
                             PaletteDtaFileSystem& files,
                             PalettePlatform& platform,
                             std::string_view palette_directory);

} // namespace creatures1::display
