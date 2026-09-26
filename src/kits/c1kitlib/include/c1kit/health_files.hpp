#pragma once

// The Health Kit's data: its shop file and the chemicals its pages read.
// Header-only and portable, so it can be tested without Windows.
//
// "Health", in the game's installation folder, holds the Doctor's page's
// stock (CAddObjectPage::LoadItemCollection @ 0x004052b0, CShopItem
// @ 0x00410740): a uint16 count, then per item
//
//   uint16   how many are left
//   CString  the CAOS that puts one in the world ("inst,new: simp herb
//            ...,edit": made, and handed to the pointer)
//   picture  {uint32 stride, uint32 height, uint16 width}, bottom-up
//            palette indices (the album photographs' format)
//   CString  name
//   CString  description
//
// The kit writes it back with the counts reduced.  ("Aphro" is the same
// format, for the Breeder's Kit.)

#include "c1kit/owner_files.hpp"

#include <string>
#include <vector>

namespace c1kit {

constexpr char kHealthShopFileName[] = "Health";

struct ShopItem {
    int quantity = 0;
    std::string command;
    PhotoBitmap picture;
    std::string name;
    std::string description;
};

inline bool parse_shop(const std::vector<std::uint8_t>& bytes,
                       std::vector<ShopItem>& out) {
    out.clear();
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        ShopItem item;
        item.quantity = reader.u16();
        item.command = reader.cstring();
        if (!read_photo_bitmap(reader, item.picture)) {
            return false;
        }
        item.name = reader.cstring();
        item.description = reader.cstring();
        if (reader.ok()) {
            out.push_back(std::move(item));
        }
    }
    return reader.ok();
}

inline std::vector<std::uint8_t> serialize_shop(const std::vector<ShopItem>& items) {
    ArchiveWriter writer;
    writer.u16(static_cast<std::uint16_t>(items.size()));
    for (const ShopItem& item : items) {
        writer.u16(static_cast<std::uint16_t>(item.quantity < 0 ? 0 : item.quantity));
        writer.cstring(item.command);
        write_photo_bitmap(writer, item.picture);
        writer.cstring(item.name);
        writer.cstring(item.description);
    }
    return writer.bytes();
}

// ---------------------------------------------------------------------------
// Chemicals
// ---------------------------------------------------------------------------

// The Fitness page (CFitnessPage, its query at 0x00416618): carbon dioxide
// and glycogen set the 1996 heartbeat, and coldness against hotness the
// thermometer.
enum HealthChemical : int {
    kChemicalPain = 1,
    kChemicalHunger = 3,
    kChemicalColdness = 4,
    kChemicalHotness = 5,
    kChemicalTiredness = 6,
    kChemicalSleepiness = 7,
    kChemicalBoredom = 11,
    kChemicalAgeing = 56,
    kChemicalStarch = 57,
    kChemicalGlucose = 58,
    kChemicalGlycogen = 59,
    kChemicalCarbonDioxide = 62,
};

// The drives, chemicals 1 to 13 (Pain to Sex Drive; 14-16 are spare).  The
// 1996 "Drives and needs" page showed five of them: pain, hunger, tiredness
// (as "Exhaustion"), sleepiness and boredom (CStatePage::
// InitializeChemicalControls @ 0x00403bf0).
constexpr int kFirstDrive = 1;
constexpr int kDriveCount = 13;

} // namespace c1kit
