#include "palette.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace creatures1::display {

namespace {

std::uint8_t expand_palette_component(std::uint8_t six_bit_component) {
    return static_cast<std::uint8_t>(six_bit_component << 2);
}

} // namespace

void build_palette_channel_ramp(PaletteChannelRamp& output,
                                int low_intensity,
                                int middle_intensity,
                                int high_intensity) {
    const int middle_to_low_delta = middle_intensity - low_intensity;
    const int middle_to_high_delta = high_intensity - middle_intensity;
    for (int control = 0; control < 0x40; ++control) {
        output.intensity_by_control[control] =
            static_cast<std::uint8_t>((control * low_intensity) / 0x40);
        output.intensity_by_control[control + 0x40] =
            static_cast<std::uint8_t>(low_intensity +
                                      (control * middle_to_low_delta) / 0x40);
        output.intensity_by_control[control + 0x80] =
            static_cast<std::uint8_t>(middle_intensity +
                                      (control * middle_to_high_delta) / 0x40);
        output.intensity_by_control[control + 0xc0] =
            static_cast<std::uint8_t>(high_intensity +
                                      (control * (0xff - high_intensity)) /
                                          0x40);
    }
}

namespace {

int centred_control_offset(std::uint8_t control, int scale) {
    const int centred = static_cast<int>(control) - 0x80;
    return (centred * scale) / 0x80;
}

int truncated_float_to_int(float value) {
    // The original uses CVTTSS2SI for the transformed channel values.
    return static_cast<int>(value);
}

} // namespace

void build_creature_palette_remap(PaletteRemapTable& output,
                                  const PaletteDtaBuffer& palette,
                                  std::uint8_t red_control,
                                  std::uint8_t green_control,
                                  std::uint8_t blue_control,
                                  std::uint8_t hue_rotation_control,
                                  std::uint8_t colour_swap_control,
                                  std::uint32_t& build_count) {
    const int red_offset = centred_control_offset(red_control, 0x14);
    const int green_offset = centred_control_offset(green_control, 0x14);
    const int blue_offset = centred_control_offset(blue_control, 0x14);

    const int red_range = centred_control_offset(red_control, 0x20);
    const int green_range = centred_control_offset(green_control, 0x20);
    const int blue_range = centred_control_offset(blue_control, 0x20);

    PaletteChannelRamp red_ramp;
    PaletteChannelRamp green_ramp;
    PaletteChannelRamp blue_ramp;
    build_palette_channel_ramp(red_ramp, red_offset + 0x40,
                               red_range + 0x80, red_offset + 0xc0);
    build_palette_channel_ramp(green_ramp, green_offset + 0x40,
                               green_range + 0x80, green_offset + 0xc0);
    build_palette_channel_ramp(blue_ramp, blue_offset + 0x40,
                               blue_range + 0x80, blue_offset + 0xc0);

    for (std::size_t index = 0; index < 10; ++index) {
        output.remapped_palette_index[index] =
            static_cast<std::uint8_t>(index);
    }
    for (std::size_t index = 0xf6; index < 0x100; ++index) {
        output.remapped_palette_index[index] =
            static_cast<std::uint8_t>(index);
    }

    const float colour_swap_factor =
        (static_cast<float>(colour_swap_control) - 128.0f) / 128.0f;
    const float colour_swap_complement = 1.0f - colour_swap_factor;

    for (std::size_t source_index = 0;
         source_index < palette.colours.size(); ++source_index) {
        const PaletteColour& source = palette.colours[source_index];
        const float red = static_cast<float>(
            red_ramp.intensity_by_control[source.red]);
        const float green = static_cast<float>(
            green_ramp.intensity_by_control[source.green]);
        const float blue = static_cast<float>(
            blue_ramp.intensity_by_control[source.blue]);

        float hue_factor;
        float transformed_red;
        float transformed_green;
        float transformed_blue;
        if (hue_rotation_control < 0x80) {
            hue_factor = (127.0f - static_cast<float>(hue_rotation_control)) /
                         127.0f;
            const float unchanged_factor = 1.0f - hue_factor;
            transformed_red = blue * hue_factor + unchanged_factor * red;
            transformed_green = unchanged_factor * green + hue_factor * red;
            transformed_blue = hue_factor * green + unchanged_factor * blue;
        } else {
            hue_factor = (static_cast<float>(hue_rotation_control) - 128.0f) /
                         127.0f;
            const float unchanged_factor = 1.0f - hue_factor;
            transformed_red = hue_factor * green + unchanged_factor * red;
            transformed_green = hue_factor * blue + unchanged_factor * green;
            transformed_blue = hue_factor * red + unchanged_factor * blue;
        }

        const int target_red = truncated_float_to_int(
            transformed_red * colour_swap_complement +
            transformed_blue * colour_swap_factor);
        const int target_green = truncated_float_to_int(transformed_green);
        const int target_blue = truncated_float_to_int(
            transformed_red * colour_swap_factor +
            transformed_blue * colour_swap_complement);

        std::size_t nearest_index = 0;
        std::uint32_t minimum_distance =
            std::numeric_limits<std::uint32_t>::max();
        for (std::size_t candidate_index = 0;
             candidate_index < palette.colours.size(); ++candidate_index) {
            const PaletteColour& candidate = palette.colours[candidate_index];
            const int red_delta = target_red - static_cast<int>(candidate.red);
            const int green_delta =
                target_green - static_cast<int>(candidate.green);
            const int blue_delta = target_blue - static_cast<int>(candidate.blue);
            const std::uint32_t distance = static_cast<std::uint32_t>(
                red_delta * red_delta + green_delta * green_delta +
                blue_delta * blue_delta);
            if (distance < minimum_distance) {
                minimum_distance = distance;
                nearest_index = candidate_index;
            }
        }

        output.remapped_palette_index[source_index + 10] =
            static_cast<std::uint8_t>(nearest_index + 10);
    }

    ++build_count;
}

bool load_palette_dta_into_buffer(PaletteDtaBuffer& output,
                                  PaletteDtaFileSystem& files,
                                  std::string_view palette_directory) {
    std::string path(palette_directory);
    path += "PALETTE.DTA";
    std::unique_ptr<PaletteDtaFile> file = files.open_for_read(path);
    if (!file || !file->skip(0x1e)) {
        return false;
    }

    std::array<std::uint8_t, 3> encoded_colour{};
    for (PaletteColour& colour : output.colours) {
        if (!file->read_exact(encoded_colour.data(), encoded_colour.size())) {
            return false;
        }
        colour.red = expand_palette_component(encoded_colour[0]);
        colour.green = expand_palette_component(encoded_colour[1]);
        colour.blue = expand_palette_component(encoded_colour[2]);
    }
    return true;
}

bool initialize_game_palette(GamePaletteState& state,
                             PaletteDtaFileSystem& files,
                             PalettePlatform& platform,
                             std::string_view palette_directory) {
    for (PaletteDtaBuffer& buffer : state.dta_buffers) {
        if (!load_palette_dta_into_buffer(buffer, files, palette_directory)) {
            return false;
        }
    }

    LogicalPalette logical_palette{};
    for (LogicalPaletteEntry& entry : logical_palette.entries) {
        entry.flags = 4;
    }
    platform.prime_system_palette(logical_palette);
    platform.fill_system_reserved_entries(logical_palette);

    logical_palette.entries[0].flags = 0;
    for (std::size_t index = 10; index < 0xf6; ++index) {
        logical_palette.entries[index].flags = 4;
    }
    for (std::size_t index = 0xf6; index < logical_palette.entries.size();
         ++index) {
        logical_palette.entries[index].flags = 0;
    }

    for (std::size_t index = 0; index < state.dta_buffers[0].colours.size();
         ++index) {
        logical_palette.entries[index + 10].colour =
            state.dta_buffers[0].colours[index];
    }

    state.native_palette = platform.create_palette(logical_palette);
    return state.native_palette != nullptr;
}

} // namespace creatures1::display
