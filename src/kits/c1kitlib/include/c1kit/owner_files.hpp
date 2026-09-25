#pragma once

// The files and records the Owner's Kit keeps for each creature, which the
// Funeral Kit also reads: the creature's registration record, "The Register"
// (every registered creature) and "<moniker>.Photo Album".  Header-only and
// portable, so it can be tested without Windows.
//
// Both files are MFC archives written by the 1996 kits: a uint16 count, then
// per entry the fields below.  Strings use MFC's CString archive form (see
// read_cstring).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace c1kit {

// ---------------------------------------------------------------------------
// MFC archive primitives
// ---------------------------------------------------------------------------

class ArchiveReader {
public:
    explicit ArchiveReader(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes) {}

    bool ok() const { return ok_; }
    bool at_end() const { return at_ >= bytes_.size(); }

    std::uint8_t u8() {
        if (!need(1)) return 0;
        return bytes_[at_++];
    }
    std::uint16_t u16() {
        if (!need(2)) return 0;
        const std::uint16_t value =
            static_cast<std::uint16_t>(bytes_[at_] | (bytes_[at_ + 1] << 8));
        at_ += 2;
        return value;
    }
    std::uint32_t u32() {
        if (!need(4)) return 0;
        const std::uint32_t value =
            static_cast<std::uint32_t>(bytes_[at_]) |
            (static_cast<std::uint32_t>(bytes_[at_ + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes_[at_ + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes_[at_ + 3]) << 24);
        at_ += 4;
        return value;
    }
    // CString: a byte length; 0xff means a uint16 length follows, and
    // 0xffff a uint32 length.  (The kits are ANSI; no Unicode marker.)
    std::string cstring() {
        std::uint32_t length = u8();
        if (length == 0xff) {
            length = u16();
            if (length == 0xffff) {
                length = u32();
            }
        }
        if (!need(length)) return std::string();
        std::string text(reinterpret_cast<const char*>(bytes_.data() + at_), length);
        at_ += length;
        return text;
    }
    bool bytes(std::size_t count, std::vector<std::uint8_t>& out) {
        if (!need(count)) return false;
        out.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(at_),
                   bytes_.begin() + static_cast<std::ptrdiff_t>(at_ + count));
        at_ += count;
        return true;
    }

private:
    bool need(std::size_t count) {
        if (!ok_ || bytes_.size() - at_ < count || at_ > bytes_.size()) {
            ok_ = false;
            return false;
        }
        return true;
    }

    const std::vector<std::uint8_t>& bytes_;
    std::size_t at_ = 0;
    bool ok_ = true;
};

class ArchiveWriter {
public:
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

    void u8(std::uint8_t value) { bytes_.push_back(value); }
    void u16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value));
        bytes_.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    void u32(std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void cstring(const std::string& text) {
        const std::size_t length = text.size();
        if (length < 0xff) {
            u8(static_cast<std::uint8_t>(length));
        } else if (length < 0xfffe) {
            u8(0xff);
            u16(static_cast<std::uint16_t>(length));
        } else {
            u8(0xff);
            u16(0xffff);
            u32(static_cast<std::uint32_t>(length));
        }
        bytes_.insert(bytes_.end(), text.begin(), text.end());
    }
    void bytes(const std::vector<std::uint8_t>& data) {
        bytes_.insert(bytes_.end(), data.begin(), data.end());
    }

private:
    std::vector<std::uint8_t> bytes_;
};

// ---------------------------------------------------------------------------
// The registration record
// ---------------------------------------------------------------------------

// Ten strings, in this order, in "The Register" (COwnerRecord @ 0x00407060,
// SerializeRegisterFields @ 0x004071e0) and in the creature's own `data`
// string, which the game keeps and fills in at birth (`dde: getb data`,
// fields joined with '|').
enum OwnerField : std::size_t {
    kOwnerMoniker = 0,
    kOwnerCreatureName = 1,
    kOwnerFatherMoniker = 2,
    kOwnerMotherMoniker = 3,
    kOwnerBirthTime = 4,     // "21:16 Sep 15 2026" (%H:%M %b %d %Y)
    kOwnerBirthplace = 5,
    kOwnerName = 6,
    kOwnerAddress = 7,
    kOwnerPhone = 8,
    kOwnerEmail = 9,
    kOwnerFieldCount = 10,
};

struct OwnerRecord {
    std::string fields[kOwnerFieldCount];
    std::string& operator[](OwnerField field) { return fields[field]; }
    const std::string& operator[](OwnerField field) const {
        return fields[field];
    }
};

// `dde: getb data`: the ten fields, each followed by '|'.  False when the
// creature has no record (an empty reply) or it is short.
inline bool parse_owner_data(const std::string& reply, OwnerRecord& out) {
    std::size_t at = 0;
    for (std::size_t i = 0; i < kOwnerFieldCount; ++i) {
        const std::size_t bar = reply.find('|', at);
        if (bar == std::string::npos) {
            return false;
        }
        out.fields[i] = reply.substr(at, bar - at);
        at = bar + 1;
    }
    return true;
}

// The record as `dde: putb [...] data,endm` stores it (TransferOwnerDataFields
// @ 0x00409ac0): fields joined by '|'.  '|', '[' and ']' would break the
// command, so they are dropped from the text.
inline std::string format_owner_data(const OwnerRecord& record) {
    std::string joined;
    for (std::size_t i = 0; i < kOwnerFieldCount; ++i) {
        for (const char c : record.fields[i]) {
            if (c != '|' && c != '[' && c != ']' && c != '\r' && c != '\n') {
                joined.push_back(c);
            }
        }
        joined.push_back('|');
    }
    return joined;
}

// ---------------------------------------------------------------------------
// The Register
// ---------------------------------------------------------------------------

// uint16 count, then ten CStrings per record (LoadOwnerRecordsFromRegister
// @ 0x00401630).  An empty file is an empty register.
inline bool parse_register(const std::vector<std::uint8_t>& bytes,
                           std::vector<OwnerRecord>& out) {
    out.clear();
    if (bytes.empty()) {
        return true;
    }
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        OwnerRecord record;
        for (std::string& field : record.fields) {
            field = reader.cstring();
        }
        if (reader.ok()) {
            out.push_back(std::move(record));
        }
    }
    return reader.ok();
}

inline std::vector<std::uint8_t> serialize_register(
    const std::vector<OwnerRecord>& records) {
    ArchiveWriter writer;
    writer.u16(static_cast<std::uint16_t>(records.size()));
    for (const OwnerRecord& record : records) {
        for (const std::string& field : record.fields) {
            writer.cstring(field);
        }
    }
    return writer.bytes();
}

inline const OwnerRecord* find_record(const std::vector<OwnerRecord>& records,
                                      const std::string& moniker) {
    for (const OwnerRecord& record : records) {
        if (record[kOwnerMoniker] == moniker) {
            return &record;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Photographs
// ---------------------------------------------------------------------------

// A photograph's pixels, as the game writes them for `dde: pict` (temp.spr)
// and as albums store them (CSprite::Serialize): uint32 row stride, uint32
// height, uint16 width, then height rows of palette indices, bottom row
// first.
struct PhotoBitmap {
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<std::uint8_t> pixels;  // height * stride, as stored

    // Palette index of pixel (x, y), y counted from the top.
    std::uint8_t at(int x, int y) const {
        return pixels[static_cast<std::size_t>(height - 1 - y) * stride + x];
    }
};

inline bool read_photo_bitmap(ArchiveReader& reader, PhotoBitmap& out) {
    const std::uint32_t stride = reader.u32();
    const std::uint32_t height = reader.u32();
    const std::uint16_t width = reader.u16();
    if (!reader.ok() || width > stride || stride > 4096 || height > 4096) {
        return false;
    }
    out.width = width;
    out.height = static_cast<int>(height);
    out.stride = static_cast<int>(stride);
    return reader.bytes(static_cast<std::size_t>(stride) * height, out.pixels);
}

inline void write_photo_bitmap(ArchiveWriter& writer, const PhotoBitmap& bitmap) {
    writer.u32(static_cast<std::uint32_t>(bitmap.stride));
    writer.u32(static_cast<std::uint32_t>(bitmap.height));
    writer.u16(static_cast<std::uint16_t>(bitmap.width));
    writer.bytes(bitmap.pixels);
}

// temp.spr, as the game writes it for `dde: pict`: one bitmap record.
inline bool parse_snapshot(const std::vector<std::uint8_t>& bytes,
                           PhotoBitmap& out) {
    ArchiveReader reader(bytes);
    return read_photo_bitmap(reader, out);
}

// One album entry (COwnerPhotoAlbumEntry::Serialize @ 0x00404e60): when it
// was taken ("%H:%M %d %B %Y"), the picture, then two strings: the caption
// typed under the photo and a second one the 1996 kit never filled.
struct Photo {
    std::string taken;
    PhotoBitmap bitmap;
    std::string caption;
    std::string reserved;
};

inline bool parse_album(const std::vector<std::uint8_t>& bytes,
                        std::vector<Photo>& out) {
    out.clear();
    if (bytes.empty()) {
        return true;
    }
    ArchiveReader reader(bytes);
    const std::uint16_t count = reader.u16();
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        Photo photo;
        photo.taken = reader.cstring();
        if (!read_photo_bitmap(reader, photo.bitmap)) {
            return false;
        }
        photo.caption = reader.cstring();
        photo.reserved = reader.cstring();
        if (reader.ok()) {
            out.push_back(std::move(photo));
        }
    }
    return reader.ok();
}

inline std::vector<std::uint8_t> serialize_album(const std::vector<Photo>& photos) {
    ArchiveWriter writer;
    writer.u16(static_cast<std::uint16_t>(photos.size()));
    for (const Photo& photo : photos) {
        writer.cstring(photo.taken);
        write_photo_bitmap(writer, photo.bitmap);
        writer.cstring(photo.caption);
        writer.cstring(photo.reserved);
    }
    return writer.bytes();
}

// The album file for a creature, in the world's directory.
inline std::string album_file_name(const std::string& moniker) {
    return moniker + ".Photo Album";
}

constexpr char kRegisterFileName[] = "The Register";

// Owner's Kit queries.
constexpr char kMonikerQuery[] = "dde: getb monk,endm";
constexpr char kOwnerDataQuery[] = "dde: getb data,endm";
constexpr char kSexQuery[] = "dde: putv gend,endm";
constexpr char kAgeQuery[] = "dde: getb ctim,endm";

// `dde: pict` takes its size as raw bytes, not numbers (the game reads the
// width byte, skips one, reads the height byte).  The Owner's Kit asks for
// 120 x 140 around the creature, after panning the camera to it
// (the traffic the 1996 kit sends for "Take photo").
inline std::string photo_query(std::uint8_t width = 120,
                               std::uint8_t height = 140) {
    std::string script = "inst,dde: panc,dde: pict ";
    script.push_back(static_cast<char>(width));
    script.push_back('|');
    script.push_back(static_cast<char>(height));
    script += ",endm";
    return script;
}

} // namespace c1kit
