// Function-level equivalence: the skeleton genes of a genome at each life
// stage, Skeleton::LoadGenome @0043c800 (CE 10.3, called by the genome
// re-expression @0040d930 at every stage change) against
// creatures::Skeleton::load_genome.
//
//   skelgenes_equivalence.exe <Creatures.exe> <native-root> <lc-root> <genome.gen>...
//                             [-cases N] [-seed S]
//
// LoadGenome reads the genus, appearance and pigment genes, finds each body
// part's sprite and attachment files (falling back to a lower life stage or
// variant, then the other sex), writes the creature's own sprite file with
// every body-part image remapped through the pigment palette, loads that
// file as the creature's gallery, chooses a render plane and builds the body
// and its 14 limbs.  Native runs all of it, on real files: each root is a
// scratch copy of the game's Images, Body Data and Palettes folders, with an
// empty "out" folder as the secondary image directory, where the sprite file
// is written.  The roots are separate, so neither side reads what the other
// wrote.
//
// Native's outside calls go to the test: MFC CFile (open, read, write,
// seek, close) on Win32, as MFC's, CPtrArray::SetAtGrow/RemoveAt, the MFC
// operator new on malloc, the attachment file stream (the stream object's
// constructor and destructor and operator>>(int&)), strcpy_s, strcat_s,
// GetFileAttributesA (the real one), and rand (the MSVC formula on both
// sides).  The app object holds the directory paths.  The sprite-file cache
// index is built by its own start-up initializer (004016c0).  The palette
// is palette.dta, loaded once and given to both sides.  The native skeleton
// starts from its own initializer (0043ac70); LC's from a new Creature.  The
// down foot, a room around it and the movement bounds of that room are the
// same on both sides.
//
// LC uses the portable resource host (application::C1ResourceHost), on a
// Win32 file backend in the test: the game's StandardResourceFileBackend
// reads through std::ifstream, which MSVC opens without regard to case, but
// the MinGW runtime under Wine does not.  Its gallery release counts references as
// native's does.  (The game's WorldRuntime also keeps released galleries for
// reuse; that is not part of this test.)
//
// Each case: one genome, either sex, a last life stage 0 to 6; in half the
// cases up to 40 bytes of the genome change (never in a gene or end marker).
// Both sides load stage 0, then each later stage up to the last, on the same
// skeleton, as the game does.  After each load these must match:
// - the creature's sprite file, byte for byte;
// - the classifier, the genome file, the parent monikers, the render plane;
// - the gallery's file and image count;
// - every body part: image base, image, plane, position, pose, view, and its
//   attachment table; the chain lengths;
// - the down foot, the chain ends and the sprite bounds;
// - the rand state.
// Before the cases, BuildCreaturePaletteRemap @00413d20 alone is compared
// with LC's remap on random pigment, rotation and swap controls (a quarter
// of them the extremes 0, 1, 127, 128, 129, 254 and 255): all 256 entries
// must match.  A palette difference counts as one differing case.

#include "native_image.hpp"

#include "application/resource_hosts.hpp"
#include "common/compat.hpp"
#include "creatures/body.hpp"
#include "creatures/creature.hpp"
#include "creatures/genome.hpp"
#include "display/gallery.hpp"
#include "display/image.hpp"
#include "display/palette.hpp"
#include "display/sprite_cache.hpp"
#include "objects/object.hpp"
#include "world/map.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::Skeleton;

constexpr std::uint32_t kLoadGenome = 0x0043c800, kInitSkeleton = 0x0043ac70,
                        kClearBodyParts = 0x0043dc50, kClearSpriteCache = 0x00441ee0,
                        kInitCacheIndex = 0x004016c0, kIfstreamOpen = 0x0043dd40,
                        kIfstreamClose = 0x0043c610, kGenomeVtable = 0x00456094,
                        kNativeDebugLog = 0x00418000, kDirtyQueue = 0x00412610, kPlaySound = 0x0042ce20;
constexpr std::uint32_t kPaletteRemap = 0x00413d20;
constexpr std::uint32_t kApp = 0x00468d80, kDocument = 0x00468d84, kMapData = 0x00468d64,
                        kEditObject = 0x00468d68, kPaletteDta = 0x00470510;
constexpr std::uint32_t kTranslate = 0x0043c0b0, kCopySpriteBounds = 0x00406d40, kBodyPlane = 0x00406d60,
                        kSpriteMinX = 0x00406d70, kSpriteMinY = 0x00406d80;
// Import address table slots.
constexpr std::uint32_t kIatCFileCtor = 0x00450c58, kIatCFileDtor = 0x00450c54, kIatCFileClose = 0x00450c4c,
                        kIatCFileRead = 0x00450c50, kIatCFileSeek = 0x004507b0, kIatCFileWrite = 0x004508a0,
                        kIatSetAtGrow = 0x0045097c, kIatRemoveAt = 0x00450980, kIatNew = 0x00450aec, kIatNewArray = 0x00450db0,
                        kIatStreamInt = 0x00450214, kIatStrcpy = 0x004504c4, kIatStrcat = 0x004504c0,
                        kIatFileAttributes = 0x00450134, kIatRand = 0x004504e8;

// Native SFCApp: primary directories at +304, secondary at +2384, 260 each.
constexpr std::size_t kAppSize = 4464, kPrimaryDirs = 304, kSecondaryDirs = 2384, kDirSize = 260;
// Native Skeleton.
constexpr std::size_t kBlockSize = 0x2d00, kClassifier = 0x4, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kGallery = 0x40, kGenomeFile = 0x50, kMother = 0x54, kFather = 0x58, kBody = 0x5c,
                      kChains = 0x60, kFootX = 0x7c, kFootY = 0x80, kPlaneNormal = 0x84, kChainEndX = 0xcc,
                      kChainEndY = 0xe4, kSpriteBounds = 0xfc, kExpression = 0x11d, kEyesOpen = 0x11e;
// Native Entity (Body, Limb).
constexpr std::size_t kPartImage = 0x8, kPartBase = 0x9, kPartPlane = 0xc, kPartX = 0x10, kPartY = 0x14,
                      kPartPose = 0x20, kPartView = 0x24, kLimbNext = 0x28, kLimbAnchors = 0x2c,
                      kBodyJoinX = 0x28, kBodyJoinY = 0x64;
// Native CGallery.
constexpr std::size_t kGalleryImageCount = 0x8, kGalleryFile = 0xc;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next()); }
    std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
    bool chance(std::uint32_t one_in) { return below(one_in) == 0; }
    int range(int low, int high) { return low + static_cast<int>(below(static_cast<std::uint32_t>(high - low + 1))); }
};

// ---- rand: the MSVC formula on both sides ----
std::uint32_t g_native_rand_state = 1, g_lc_rand_state = 1;
extern "C" int __cdecl native_rand() {
    g_native_rand_state = g_native_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_native_rand_state >> 16) & 0x7fff);
}
extern "C" int __wrap_rand() {
    g_lc_rand_state = g_lc_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_lc_rand_state >> 16) & 0x7fff);
}
extern "C" void __wrap_srand(unsigned seed) { g_lc_rand_state = seed; }

[[noreturn]] void fail(const char* what, const char* detail = "") {
    std::fprintf(stderr, "%s%s\n", what, detail);
    std::exit(6);
}

// ---- C runtime and allocation ----
int __cdecl native_strcpy_s(char* d, std::size_t n, const char* s) {
    std::size_t i = 0;
    for (; i + 1 < n && s[i] != '\0'; ++i) d[i] = s[i];
    d[i] = '\0';
    return 0;
}
int __cdecl native_strcat_s(char* d, std::size_t n, const char* s) {
    std::size_t i = std::strlen(d);
    for (; i + 1 < n && *s != '\0'; ++i) d[i] = *s++;
    d[i] = '\0';
    return 0;
}
void* __cdecl native_new(std::size_t n) {
    void* p = std::malloc(n == 0 ? 1 : n);
    if (p == nullptr) fail("native operator new failed");
    return p;
}
void __cdecl native_free(void* p) { std::free(p); }
double __cdecl native_ceil(double value) { return std::ceil(value); }
// The cache index's start-up initializer registers its destructor.
int __cdecl native_register_onexit(void*, void*) { return 0; }

// ---- native's file exception ----
// MFC's CFile constructor throws CFileException when it cannot open a file.
// The test does not run MFC's exception path: an open that fails during a
// load returns to the test (the SEH chain head saved before the call is put
// back), and the load counts as refused.  The files the aborted load opened
// for writing are closed.
void* g_abort[5];
bool g_abort_armed = false;
std::vector<HANDLE> g_write_handles;
std::string g_abort_path;
void* fs_chain_head() {
    void* head;
    asm volatile("movl %%fs:0, %0" : "=r"(head));
    return head;
}
void set_fs_chain_head(void* head) { asm volatile("movl %0, %%fs:0" : : "r"(head) : "memory"); }

// ---- MFC CFile, on Win32 as MFC's is ----
// The object: the test's vtable at +0, the HANDLE at +4 (native allocates
// 0x14 bytes for a CFile, or a larger stack slot).
void* g_cfile_vtable[40];
HANDLE& handle_of(void* self) { return reinterpret_cast<HANDLE*>(self)[1]; }
void* __attribute__((thiscall)) cfile_ctor(void* self, const char* path, unsigned flags) {
    // modeCreate 0x1000, modeWrite 1, modeReadWrite 2; shareDenyNone 0x40.
    const bool create = (flags & 0x1000) != 0, write = (flags & 3) != 0;
    HANDLE h = CreateFileA(path, GENERIC_READ | (write ? GENERIC_WRITE : 0), FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, create ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (g_abort_armed) {
            g_abort_path = path;
            __builtin_longjmp(g_abort, 1);
        }
        std::fprintf(stderr, "native CFile cannot open %s (error %lu)\n", path, GetLastError());
        std::exit(6);
    }
    reinterpret_cast<void**>(self)[0] = g_cfile_vtable;
    handle_of(self) = h;
    if (write) g_write_handles.push_back(h);
    return self;
}
void __attribute__((thiscall)) cfile_close(void* self) {
    if (handle_of(self) != nullptr) {
        g_write_handles.erase(std::remove(g_write_handles.begin(), g_write_handles.end(), handle_of(self)),
                              g_write_handles.end());
        CloseHandle(handle_of(self));
    }
    handle_of(self) = nullptr;
}
void __attribute__((thiscall)) cfile_dtor(void* self) { cfile_close(self); }
void* __attribute__((thiscall)) cfile_deleting_dtor(void* self, unsigned flags) {
    cfile_close(self);
    if ((flags & 1) != 0) std::free(self);
    return self;
}
unsigned long long __attribute__((thiscall)) cfile_seek(void* self, std::uint32_t low, std::int32_t high,
                                                         unsigned from) {
    LONG high_part = high;
    const DWORD result = SetFilePointer(handle_of(self), static_cast<LONG>(low), &high_part, from);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) fail("native CFile seek failed");
    return (static_cast<unsigned long long>(static_cast<std::uint32_t>(high_part)) << 32) | result;
}
unsigned __attribute__((thiscall)) cfile_read(void* self, void* buffer, unsigned count) {
    DWORD done = 0;
    if (!ReadFile(handle_of(self), buffer, count, &done, nullptr)) fail("native CFile read failed");
    return done;
}
void __attribute__((thiscall)) cfile_write(void* self, const void* buffer, unsigned count) {
    DWORD done = 0;
    if (!WriteFile(handle_of(self), buffer, count, &done, nullptr) || done != count) {
        fail("native CFile write failed");
    }
}
template <int Slot>
void __attribute__((thiscall)) cfile_unexpected(void*) {
    std::fprintf(stderr, "native called CFile virtual slot %d\n", Slot);
    std::exit(6);
}
template <int... Slots>
void fill_cfile_vtable(std::integer_sequence<int, Slots...>) {
    ((g_cfile_vtable[Slots] = reinterpret_cast<void*>(&cfile_unexpected<Slots>)), ...);
}

// ---- MFC CPtrArray: vptr, m_pData, m_nSize, m_nMaxSize, m_nGrowBy ----
struct PtrArray {
    void* vptr;
    void** data;
    int size, max_size, grow_by;
};
void __attribute__((thiscall)) ptr_array_set_at_grow(PtrArray* self, int index, void* value) {
    if (index >= self->max_size) {
        const int capacity = index + 64;
        self->data = static_cast<void**>(std::realloc(self->data, sizeof(void*) * capacity));
        for (int i = self->max_size; i < capacity; ++i) self->data[i] = nullptr;
        self->max_size = capacity;
    }
    if (index >= self->size) self->size = index + 1;
    self->data[index] = value;
}
void __attribute__((thiscall)) ptr_array_remove_at(PtrArray* self, int index, int count) {
    if (index < 0 || count < 0 || index + count > self->size) fail("native CPtrArray::RemoveAt out of range");
    std::memmove(self->data + index, self->data + index + count,
                 sizeof(void*) * static_cast<std::size_t>(self->size - index - count));
    self->size -= count;
}

// ---- the attachment file stream ----
struct TextStream {
    std::string text;
    std::size_t at = 0;
};
std::map<void*, TextStream> g_streams;
void* __attribute__((thiscall)) stream_open(void* self, const char* path, int, int, int) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) fail("native ifstream cannot open ", path);
    TextStream stream;
    char buffer[4096];
    DWORD done = 0;
    while (ReadFile(h, buffer, sizeof buffer, &done, nullptr) && done != 0) stream.text.append(buffer, done);
    CloseHandle(h);
    g_streams[self] = std::move(stream);
    return self;
}
void __attribute__((thiscall)) stream_close(void* self) {
    if (g_streams.erase(self) == 0) fail("native closed a stream it did not open");
}
void* __attribute__((thiscall)) stream_read_int(void* self, int* value) {
    auto found = g_streams.find(self);
    if (found == g_streams.end()) fail("native read a stream it did not open");
    TextStream& stream = found->second;
    const char* begin = stream.text.c_str() + stream.at;
    char* end = nullptr;
    const long parsed = std::strtol(begin, &end, 10);
    if (end == begin) {
        *value = 0;
    } else {
        *value = static_cast<int>(parsed);
        stream.at += static_cast<std::size_t>(end - begin);
    }
    return self;
}

// ---- recorders ----
void __attribute__((thiscall)) native_dirty(void*, int, int, int, int) {}
void __attribute__((thiscall)) native_sound(void*, std::uint32_t, int, int) {}
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on the native skeleton\n");
    std::exit(5);
}
void* g_skeleton_vtable[64];

// ---- LC hosts ----
class Hosts final : public creatures::SkeletonRenderPlaneHost,
                    public objects::ObjectSoundPlaybackHost,
                    public objects::ObjectMovementBoundsHost {
public:
    world::MapRoomTable rooms;
    // SkeletonRenderPlaneHost: no other creature.
    std::uint32_t next_random_value() override { return static_cast<std::uint32_t>(std::rand()); }
    std::size_t creature_count() const override { return 0; }
    objects::Object* creature_at(std::size_t) const override { return nullptr; }
    // ObjectSoundPlaybackHost: muted.
    world::ViewportBounds sound_viewport() const override { return {}; }
    bool sounds_muted() const override { return true; }
    sound::SoundManager& sound_manager() override { fail("sound_manager was not expected"); }
    bool debug_console_available() const override { return false; }
    void log_sound_event(LogEvent, sound::SoundId, int) override {}
    // ObjectMovementBoundsHost, as the game's.
    void clear_edit_object() override {}
    void find_nearest_room_bounds_at_point(int x, int y, world::WorldRect& out) const override {
        world::MapRectangle m{out.min_x, out.min_y, out.max_x, out.max_y};
        world::find_nearest_room_bounds_at_point(rooms, x, y, m);
        out = {m.left, m.top, m.right, m.bottom};
    }
    world::WorldRect vehicle_local_bounds(const objects::Object&) const override { return {}; }
    int vehicle_primary_entity_x(const objects::Object&) const override { return 0; }
    int vehicle_primary_entity_y(const objects::Object&) const override { return 0; }
    objects::Object* edit_object() const override { return nullptr; }
};

// LC's file boundary on Win32, as the game's ifstream is under MSVC (file
// names are not case sensitive).  The MinGW C runtime under Wine is.
class Win32ReadFile final : public application::ResourceReadFile {
public:
    explicit Win32ReadFile(HANDLE h) : h_(h) {}
    ~Win32ReadFile() override { CloseHandle(h_); }
    bool seek_from_beginning(std::uint32_t offset) override {
        return SetFilePointer(h_, static_cast<LONG>(offset), nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER;
    }
    bool read_exact(std::uint8_t* destination, std::size_t count) override {
        DWORD done = 0;
        return ReadFile(h_, destination, static_cast<DWORD>(count), &done, nullptr) && done == count;
    }

private:
    HANDLE h_;
};
class Win32WriteFile final : public application::ResourceWriteFile {
public:
    explicit Win32WriteFile(HANDLE h) : h_(h) {}
    ~Win32WriteFile() override { close(); }
    void write(const std::uint8_t* bytes, std::size_t count) override {
        DWORD done = 0;
        if (h_ == INVALID_HANDLE_VALUE || !WriteFile(h_, bytes, static_cast<DWORD>(count), &done, nullptr) ||
            done != count) {
            fail("LC write failed");
        }
    }
    bool seek_from_beginning() override {
        return SetFilePointer(h_, 0, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER;
    }
    void close() override {
        if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
        h_ = INVALID_HANDLE_VALUE;
    }

private:
    HANDLE h_;
};
class Win32Backend final : public application::ResourceFileBackend {
public:
    std::unique_ptr<application::ResourceReadFile> open_for_read(std::string_view path) override {
        HANDLE h = CreateFileA(std::string(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return nullptr;
        return std::make_unique<Win32ReadFile>(h);
    }
    std::unique_ptr<application::ResourceWriteFile> open_for_create(std::string_view path) override {
        HANDLE h = CreateFileA(std::string(path).c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE) return nullptr;
        return std::make_unique<Win32WriteFile>(h);
    }
    bool regular_file_exists(std::string_view path) const override {
        const DWORD a = GetFileAttributesA(std::string(path).c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
    bool read_binary_file(std::string_view path, std::vector<std::uint8_t>& bytes) const override {
        HANDLE h = CreateFileA(std::string(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        bytes.clear();
        std::uint8_t buffer[65536];
        DWORD done = 0;
        while (ReadFile(h, buffer, sizeof buffer, &done, nullptr) && done != 0) {
            bytes.insert(bytes.end(), buffer, buffer + done);
        }
        CloseHandle(h);
        return true;
    }
    bool read_text_file(std::string_view path, std::string& text) const override {
        std::vector<std::uint8_t> bytes;
        if (!read_binary_file(path, bytes)) return false;
        text.assign(bytes.begin(), bytes.end());
        return true;
    }
};

// Releases as native does: a gallery goes at its last reference.
class Lifetime final : public creatures::SkeletonLifetimeHost {
public:
    explicit Lifetime(application::C1ResourceHost& host) : host_(host) {}
    void destroy_limb(creatures::LimbPart& limb) override { delete &limb; }
    void stop_continuous_sound(int) override {}
    void remove_from_renderable_set(Skeleton&) override {}
    void unregister_from_object_registry(Skeleton&) override {}
    void release_gallery(display::Gallery& gallery) override {
        if (gallery.reference_count > 0) --gallery.reference_count;
        if (gallery.reference_count == 0 && !host_.release_gallery(gallery)) {
            fail("LC released a gallery the host does not hold");
        }
    }

private:
    application::C1ResourceHost& host_;
};

class OneGenomeFile final : public genome::GenomeFileStore {
public:
    explicit OneGenomeFile(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::string secondary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    std::string primary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    bool regular_file_exists(const std::string&) const override { return true; }
    std::vector<std::uint8_t> read_file(const std::string&) override { return bytes_; }
    void write_secondary_genetics_file(genome::GenomeFilenameId, const std::vector<std::uint8_t>&) override {}

private:
    std::vector<std::uint8_t> bytes_;
};

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}
template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}
std::uint8_t* pointer_at(std::uint8_t* block, std::size_t offset) { return field<std::uint8_t*>(block, offset); }
void put32(std::uint8_t* p, std::uint32_t v) { std::memcpy(p, &v, 4); }
std::string with_slash(std::string path) {
    if (!path.empty() && path.back() != '\\' && path.back() != '/') path += '\\';
    return path;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <native-root> <lc-root> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    const std::string native_root = with_slash(argv[2]), lc_root = with_slash(argv[3]);
    long cases = 200;
    std::uint64_t seed = 0x5ce1e9u;
    std::vector<std::vector<std::uint8_t>> genomes;
    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) cases = std::atol(argv[++i]);
        else if (arg == "-seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
        else {
            genomes.push_back(read_file(argv[i]));
            if (genomes.back().empty()) fail("cannot read ", argv[i]);
        }
    }

    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) fail(error.c_str());
    long differing_palettes = 0;
    auto set_iat = [&](std::uint32_t slot, const void* function) {
        *image.at<const void**>(slot) = function;
    };
    fill_cfile_vtable(std::make_integer_sequence<int, 40>{});
    g_cfile_vtable[1] = reinterpret_cast<void*>(&cfile_deleting_dtor);
    g_cfile_vtable[13] = reinterpret_cast<void*>(&cfile_seek);
    g_cfile_vtable[16] = reinterpret_cast<void*>(&cfile_read);
    g_cfile_vtable[22] = reinterpret_cast<void*>(&cfile_close);
    set_iat(kIatCFileCtor, reinterpret_cast<const void*>(&cfile_ctor));
    set_iat(kIatCFileDtor, reinterpret_cast<const void*>(&cfile_dtor));
    set_iat(kIatCFileClose, reinterpret_cast<const void*>(&cfile_close));
    set_iat(kIatCFileRead, reinterpret_cast<const void*>(&cfile_read));
    set_iat(kIatCFileSeek, reinterpret_cast<const void*>(&cfile_seek));
    set_iat(kIatCFileWrite, reinterpret_cast<const void*>(&cfile_write));
    set_iat(kIatSetAtGrow, reinterpret_cast<const void*>(&ptr_array_set_at_grow));
    set_iat(kIatNew, reinterpret_cast<const void*>(&native_new));
    set_iat(kIatNewArray, reinterpret_cast<const void*>(&native_new));
    set_iat(kIatStreamInt, reinterpret_cast<const void*>(&stream_read_int));
    set_iat(kIatStrcpy, reinterpret_cast<const void*>(&native_strcpy_s));
    set_iat(kIatStrcat, reinterpret_cast<const void*>(&native_strcat_s));
    set_iat(kIatFileAttributes, reinterpret_cast<const void*>(&GetFileAttributesA));
    set_iat(kIatRand, reinterpret_cast<const void*>(&native_rand));
    image.bind_import("!free", reinterpret_cast<void*>(&native_free));
    // MFC operator delete (#1509) and delete[] (#266), beside new (#1507)
    // and new[] (#265).
    image.bind_import("!#1509", reinterpret_cast<void*>(&native_free));
    image.bind_import("!#266", reinterpret_cast<void*>(&native_free));
    image.bind_import("!ceil", reinterpret_cast<void*>(&native_ceil));
    image.bind_import("!_register_onexit_function", reinterpret_cast<void*>(&native_register_onexit));
    image.redirect(kIfstreamOpen, reinterpret_cast<const void*>(&stream_open));
    image.redirect(kIfstreamClose, reinterpret_cast<const void*>(&stream_close));
    image.make_return(kNativeDebugLog);
    image.redirect(kDirtyQueue, reinterpret_cast<const void*>(&native_dirty));
    image.redirect(kPlaySound, reinterpret_cast<const void*>(&native_sound));
    set_iat(kIatRemoveAt, reinterpret_cast<const void*>(&ptr_array_remove_at));
    for (auto& slot : g_skeleton_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_skeleton_vtable[20] = image.at<void*>(kTranslate);
    g_skeleton_vtable[26] = image.at<void*>(kCopySpriteBounds);
    g_skeleton_vtable[27] = image.at<void*>(kBodyPlane);
    g_skeleton_vtable[30] = image.at<void*>(kSpriteMinX);
    g_skeleton_vtable[31] = image.at<void*>(kSpriteMinY);
    std::vector<std::uint8_t> document(0x200, 0), map_data(0x1000, 0);
    *image.at<void**>(kDocument) = document.data();
    *image.at<void**>(kMapData) = map_data.data();
    *image.at<void**>(kEditObject) = nullptr;

    // The app's directories.
    std::vector<std::uint8_t> app(kAppSize, 0);
    auto set_dir = [&](std::size_t base, std::size_t slot, const std::string& path) {
        native_strcpy_s(reinterpret_cast<char*>(app.data() + base + kDirSize * slot), kDirSize, path.c_str());
    };
    set_dir(kPrimaryDirs, 0, native_root);
    set_dir(kPrimaryDirs, 3, native_root + "Palettes\\");
    set_dir(kPrimaryDirs, 4, native_root + "Images\\");
    set_dir(kPrimaryDirs, 6, native_root + "Body Data\\");
    set_dir(kSecondaryDirs, 0, native_root);
    set_dir(kSecondaryDirs, 4, native_root + "out\\");
    set_dir(kSecondaryDirs, 6, native_root + "Body Data\\");
    *image.at<void**>(kApp) = app.data();
    using Cdecl0 = void(__cdecl*)();
    image.at<Cdecl0>(kInitCacheIndex)();

    // LC's resource host.
    application::C1ResourceDirectories directories;
    directories.primary_resource_directories[0] = lc_root;
    directories.primary_resource_directories[3] = lc_root + "Palettes\\";
    directories.primary_resource_directories[4] = lc_root + "Images\\";
    directories.primary_resource_directories[6] = lc_root + "Body Data\\";
    directories.secondary_resource_directories[0] = lc_root;
    directories.secondary_resource_directories[4] = lc_root + "out\\";
    directories.secondary_resource_directories[6] = lc_root + "Body Data\\";
    directories.body_data_directory = lc_root + "Body Data\\";
    Win32Backend files;
    application::C1ResourceHost host(directories, files);
    Lifetime lifetime(host);

    // The palette, for both sides.
    application::C1PaletteDtaHost palette_files(files);
    display::PaletteDtaBuffer palette;
    if (!display::load_palette_dta_into_buffer(palette, palette_files, lc_root + "Palettes\\")) {
        fail("cannot load palette.dta under ", lc_root.c_str());
    }
    static_assert(sizeof(palette) == 708, "the palette buffer is 236 RGB entries");
    std::memcpy(image.at<void*>(kPaletteDta), &palette, sizeof(palette));

    // ---- the palette remap alone: random controls, and the extremes ----
    {
        using RemapFn = void(__attribute__((stdcall)) *)(void*, std::uint32_t, std::uint32_t, std::uint32_t,
                                                         std::uint32_t, std::uint32_t);
        const auto native_remap = image.at<RemapFn>(kPaletteRemap);
        const long palette_cases = std::getenv("EQUIVALENCE_PALETTE_CASES")
                                       ? std::atol(std::getenv("EQUIVALENCE_PALETTE_CASES"))
                                       : 20000;
        Random palette_random{seed ^ 0x9a1e77eu};
        const std::uint8_t extremes[] = {0, 1, 127, 128, 129, 254, 255};
        long palette_differing = 0;
        for (long p = 0; p < palette_cases; ++p) {
            std::uint8_t c[5];
            for (auto& v : c) {
                v = palette_random.chance(4) ? extremes[palette_random.below(7)] : palette_random.byte();
            }
            display::PaletteRemapTable lc_table{};
            std::uint32_t builds = 0;
            display::build_creature_palette_remap(lc_table, palette, c[0], c[1], c[2], c[3], c[4], builds);
            std::uint8_t native_table[256] = {};
            native_remap(native_table, c[0], c[1], c[2], c[3], c[4]);
            if (std::memcmp(native_table, lc_table.remapped_palette_index.data(), 256) != 0) {
                int at = 0;
                while (native_table[at] == lc_table.remapped_palette_index[at]) ++at;
                if (palette_differing < 10) {
                    std::printf("palette %u %u %u %u %u: entry %d native %u lc %u\n", c[0], c[1], c[2], c[3], c[4],
                                at, native_table[at], lc_table.remapped_palette_index[at]);
                    if (at >= 10 && at < 246 && std::getenv("EQUIVALENCE_PALETTE_DETAIL") != nullptr) {
                        const auto& source = palette.colours[at - 10];
                        const auto& n = palette.colours[native_table[at] - 10];
                        const auto& l = palette.colours[lc_table.remapped_palette_index[at] - 10];
                        std::printf("  source %u %u %u, native pick %u %u %u, lc pick %u %u %u\n", source.red,
                                    source.green, source.blue, n.red, n.green, n.blue, l.red, l.green, l.blue);
                    }
                }
                ++palette_differing;
            }
        }
        std::printf("palette remap: %ld control sets, %ld differ\n", palette_cases, palette_differing);
        if (palette_differing != 0) ++differing_palettes;
    }

    using LoadFn = void(__attribute__((thiscall)) *)(void*, void*, std::uint32_t);
    using Thiscall0 = void(__attribute__((thiscall)) *)(void*);
    const auto native_load = image.at<LoadFn>(kLoadGenome);
    const auto native_init = image.at<Thiscall0>(kInitSkeleton);
    const auto native_clear = image.at<Thiscall0>(kClearBodyParts);
    const auto native_clear_cache = image.at<Cdecl0>(kClearSpriteCache);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    // The creature's sprite file: the genome file id 'zq00'.
    const std::uint32_t moniker = 'z' | ('q' << 8) | ('0' << 16) | ('0' << 24);
    const std::string sprite_name = "zq00.spr";
    // One pixel cache and one sprite-file cache for the run, as the game's
    // document has; load_genome clears the file cache at each load.
    display::PixelCacheState pixel_cache;
    display::SpriteFileCache sprite_files;
    std::uint32_t palette_builds = 0;
    Random random{seed};
    long differing = 0, loads = 0, parts_compared = 0, sprite_bytes = 0, refused = 0;
    for (long index = 0; index < cases; ++index) {
        const std::size_t which = random.below(static_cast<std::uint32_t>(genomes.size()));
        const auto sex = random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female;
        const std::uint32_t last_stage = random.below(7);
        std::vector<std::uint8_t> bytes = genomes[which];
        if (random.chance(2)) {
            for (std::uint32_t n = 1 + random.below(40); n != 0; --n) {
                std::uint8_t value = random.byte();
                if (value != 0 && std::strchr("gends", value) != nullptr) value = 0;
                const std::size_t at = 4 + random.below(static_cast<std::uint32_t>(bytes.size() - 8));
                bool in_marker = false;
                for (std::size_t start = at - 3; start <= at; ++start) {
                    in_marker = in_marker || std::memcmp(&bytes[start], "gen", 3) == 0;
                }
                if (!in_marker) bytes[at] = value;
            }
        }
        OneGenomeFile file(bytes);

        // ---- a room around the creature, and the creature ----
        std::vector<world::MapRoom> rooms(1);
        const int foot_x = random.range(500, 7500), foot_y = random.range(300, 1000);
        rooms[0].bounds = {foot_x - random.range(200, 600), foot_y - random.range(200, 400),
                           foot_x + random.range(200, 600), foot_y + random.range(0, 60)};
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = 1;
        std::memcpy(map_data.data() + kRooms, &rooms[0].bounds, 16);
        Hosts hosts;
        hosts.rooms = {rooms.data(), rooms.size()};

        auto creature = std::make_unique<creatures::Creature>();
        Skeleton& lc = creature->skeleton();
        lc.down_foot_x = foot_x;
        lc.down_foot_y = foot_y;
        std::vector<std::uint8_t> native_storage(kBlockSize, 0);
        std::uint8_t* native = native_storage.data();
        field<void*>(native, 0) = g_skeleton_vtable;
        native_init(native);
        field<std::int32_t>(native, kFootX) = foot_x;
        field<std::int32_t>(native, kFootY) = foot_y;
        // The head's expression and eyes, as each initializer left them.
        if (native[kExpression] != lc.drive_threshold_state || (native[kEyesOpen] != 0) != lc.eyes_open) {
            std::printf("case %ld: start: native expression %u eyes %u, lc %u %u\n", index, native[kExpression],
                        native[kEyesOpen], lc.drive_threshold_state, lc.eyes_open ? 1u : 0u);
            ++differing;
            continue;
        }
        // A creature in the world has its room's movement bounds.
        const world::WorldRect movement = {rooms[0].bounds.left, rooms[0].bounds.top, rooms[0].bounds.right,
                                           rooms[0].bounds.bottom};
        lc.set_movement_bounds(movement);
        field<world::WorldRect>(native, kMovementBounds) = movement;
        field<std::uint32_t>(native, kGenomeFile) = moniker;
        const std::uint32_t rand_seed = random.next();
        g_native_rand_state = g_lc_rand_state = rand_seed;

        std::string first;
        char text[300];
        bool refused_case = false;
        auto note = [&](const std::string& what) { if (first.empty()) first = what; };
        for (std::uint32_t stage = 0; stage <= last_stage && first.empty(); ++stage) {
            const auto life_stage = static_cast<genome::GenomeLifeStage>(stage);
            genome::Genome g(moniker, sex, life_stage, &file);
            creatures::SkeletonSpriteBuildServices services{
                host, lifetime, host, host, host, host, pixel_cache, sprite_files, palette, palette_builds,
                lc_root + "out\\", lc_root + "Images\\", nullptr, 100, nullptr};
            const bool lc_loaded = lc.load_genome(g, services, hosts, hosts);

            std::vector<std::uint8_t> reader_bytes = bytes;
            reader_bytes.resize(bytes.size() + 1024, 0);
            reader_bytes.insert(reader_bytes.end(), {'g', 'e', 'n', 'd'});
            std::vector<std::uint8_t> reader(0x20, 0);
            put32(reader.data(), kGenomeVtable);
            field<std::uint8_t*>(reader.data(), 4) = reader_bytes.data();
            field<std::uint8_t*>(reader.data(), 8) = reader_bytes.data();
            put32(reader.data() + 0xc, static_cast<std::uint32_t>(bytes.size()));
            put32(reader.data() + 0x10, moniker);
            put32(reader.data() + 0x14, static_cast<std::uint32_t>(sex));
            reader[0x18] = static_cast<std::uint8_t>(stage);
            bool native_refused = false;
            {
                void* const chain = fs_chain_head();
                if (__builtin_setjmp(g_abort) == 0) {
                    g_abort_armed = true;
                    native_load(native, reader.data(), stage);
                    g_abort_armed = false;
                } else {
                    g_abort_armed = false;
                    set_fs_chain_head(chain);
                    for (HANDLE h : g_write_handles) CloseHandle(h);
                    g_write_handles.clear();
                    native_refused = true;
                }
            }
            ++loads;
            if (native_refused || !lc_loaded) {
                // Native's load stopped at a file it could not open (its
                // CFile throws); LC's load must refuse the same genome.
                if (native_refused && !lc_loaded) {
                    ++refused;
                    if (index == verbose_case || std::getenv("EQUIVALENCE_REFUSED") != nullptr) {
                        std::printf("  case %ld stage %u: both refuse (native could not open %s; genus %u)\n", index,
                                    stage, g_abort_path.c_str(), lc.creature_genus_selector);
                    }
                } else {
                    std::snprintf(text, sizeof text, "stage %u: native %s, lc %s", stage,
                                  native_refused ? "refused" : "loaded", lc_loaded ? "loaded" : "refused");
                    note(text);
                }
                refused_case = true;
                break;
            }

            // ---- compare ----
            auto prefix = [&](const char* what) {
                std::snprintf(text, sizeof text, "stage %u: %s", stage, what);
                return std::string(text);
            };
            const std::vector<std::uint8_t> native_sprite = read_file(native_root + "out\\" + sprite_name);
            const std::vector<std::uint8_t> lc_sprite = read_file(lc_root + "out\\" + sprite_name);
            sprite_bytes += static_cast<long>(native_sprite.size());
            if (native_sprite.empty()) note(prefix("native wrote no sprite file"));
            if (native_sprite != lc_sprite) {
                std::size_t at = 0;
                while (at < native_sprite.size() && at < lc_sprite.size() && native_sprite[at] == lc_sprite[at]) ++at;
                std::snprintf(text, sizeof text, "stage %u: sprite file: sizes native %u lc %u, first difference at %u",
                              stage, static_cast<unsigned>(native_sprite.size()),
                              static_cast<unsigned>(lc_sprite.size()), static_cast<unsigned>(at));
                note(text);
            }
            if (field<std::uint32_t>(native, kClassifier) != lc.classifier_base() ||
                field<std::uint32_t>(native, kGenomeFile) != lc.genome_source_filename ||
                field<std::uint32_t>(native, kMother) != lc.mother_moniker ||
                field<std::uint32_t>(native, kFather) != lc.father_moniker ||
                field<std::int32_t>(native, kPlaneNormal) != lc.normal_render_plane) {
                std::snprintf(text, sizeof text,
                              "stage %u: identity: native classifier %08x file %08x parents %08x/%08x plane %d, "
                              "lc %08x %08x %08x/%08x %d",
                              stage, field<std::uint32_t>(native, kClassifier), field<std::uint32_t>(native, kGenomeFile),
                              field<std::uint32_t>(native, kMother), field<std::uint32_t>(native, kFather),
                              field<std::int32_t>(native, kPlaneNormal), lc.classifier_base(),
                              lc.genome_source_filename, lc.mother_moniker, lc.father_moniker,
                              lc.normal_render_plane);
                note(text);
            }
            std::uint8_t* native_gallery = pointer_at(native, kGallery);
            const display::Gallery* lc_gallery = lc.gallery();
            if ((native_gallery == nullptr) != (lc_gallery == nullptr) ||
                (native_gallery != nullptr &&
                 (field<std::uint32_t>(native_gallery, kGalleryFile) != lc_gallery->sprite_file_id ||
                  field<std::uint32_t>(native_gallery, kGalleryImageCount) != lc_gallery->image_count))) {
                note(prefix("gallery"));
            }
            // Every part: the body, then each chain in order.
            std::vector<std::pair<const creatures::BodyPart*, std::uint8_t*>> parts;
            if (lc.body != nullptr && pointer_at(native, kBody) != nullptr) {
                parts.push_back({lc.body.get(), pointer_at(native, kBody)});
                const auto& joins = lc.body->attachment_table;
                std::uint8_t* raw = pointer_at(native, kBody);
                for (std::size_t c = 0; c < 6; ++c) {
                    for (std::size_t v = 0; v < 10; ++v) {
                        if (raw[kBodyJoinX + 10 * c + v] != joins.join_x[c][v] ||
                            raw[kBodyJoinY + 10 * c + v] != joins.join_y[c][v]) {
                            note(prefix("body joins"));
                        }
                    }
                }
            } else if ((lc.body == nullptr) != (pointer_at(native, kBody) == nullptr)) {
                note(prefix("body: one side has none"));
            }
            for (std::size_t c = 0; c < 6; ++c) {
                const creatures::LimbPart* limb = lc.limb_chain_heads[c];
                std::uint8_t* raw = pointer_at(native, kChains + 4 * c);
                int depth = 0;
                while (limb != nullptr && raw != nullptr) {
                    parts.push_back({limb, raw});
                    const auto& t = limb->attachment_table;
                    for (std::size_t v = 0; v < 10; ++v) {
                        if (raw[kLimbAnchors + v] != t.anchor_a_x[v] || raw[kLimbAnchors + 10 + v] != t.anchor_a_y[v] ||
                            raw[kLimbAnchors + 20 + v] != t.anchor_b_x[v] ||
                            raw[kLimbAnchors + 30 + v] != t.anchor_b_y[v]) {
                            std::snprintf(text, sizeof text, "stage %u: chain %u limb %d anchors", stage,
                                          static_cast<unsigned>(c), depth);
                            note(text);
                        }
                    }
                    limb = limb->next_in_chain;
                    raw = pointer_at(raw, kLimbNext);
                    ++depth;
                }
                if ((limb == nullptr) != (raw == nullptr)) {
                    std::snprintf(text, sizeof text, "stage %u: chain %u lengths differ", stage, static_cast<unsigned>(c));
                    note(text);
                }
            }
            for (std::size_t p = 0; p < parts.size(); ++p) {
                const creatures::BodyPart& part = *parts[p].first;
                std::uint8_t* raw = parts[p].second;
                ++parts_compared;
                if (raw[kPartImage] != part.current_image_index() || raw[kPartBase] != part.image_index_base() ||
                    field<std::int32_t>(raw, kPartX) != part.world_x() ||
                    field<std::int32_t>(raw, kPartY) != part.world_y() ||
                    field<std::int32_t>(raw, kPartPose) != part.pose_frame_index ||
                    field<std::int32_t>(raw, kPartView) != part.attachment_frame_index ||
                    field<std::int32_t>(raw, kPartPlane) != part.render_plane()) {
                    std::snprintf(text, sizeof text,
                                  "stage %u: part %u: native img %u base %u (%d,%d) pose %d view %d plane %d, lc img %u "
                                  "base %u (%d,%d) pose %u view %u plane %d",
                                  stage, static_cast<unsigned>(p), raw[kPartImage], raw[kPartBase],
                                  field<std::int32_t>(raw, kPartX), field<std::int32_t>(raw, kPartY),
                                  field<std::int32_t>(raw, kPartPose), field<std::int32_t>(raw, kPartView),
                                  field<std::int32_t>(raw, kPartPlane), part.current_image_index(),
                                  part.image_index_base(), part.world_x(), part.world_y(), part.pose_frame_index,
                                  part.attachment_frame_index, part.render_plane());
                    note(text);
                }
            }
            if (field<std::int32_t>(native, kFootX) != lc.down_foot_x ||
                field<std::int32_t>(native, kFootY) != lc.down_foot_y) {
                note(prefix("down foot"));
            }
            for (std::size_t c = 0; c < 6; ++c) {
                if (field<std::int32_t>(native, kChainEndX + 4 * c) != lc.limb_chain_end_x[c] ||
                    field<std::int32_t>(native, kChainEndY + 4 * c) != lc.limb_chain_end_y[c]) {
                    note(prefix("chain ends"));
                }
            }
            const auto& bounds = field<world::WorldRect>(native, kSpriteBounds);
            if (bounds.min_x != lc.sprite_bounds.min_x || bounds.min_y != lc.sprite_bounds.min_y ||
                bounds.max_x != lc.sprite_bounds.max_x || bounds.max_y != lc.sprite_bounds.max_y) {
                note(prefix("sprite bounds"));
            }
            if (g_native_rand_state != g_lc_rand_state) note(prefix("rand state"));
            if (index == verbose_case) {
                const auto& mb = field<world::WorldRect>(native, kMovementBounds);
                std::printf("  stage %u: native foot (%d,%d) bounds {%d,%d,%d,%d} movement {%d,%d,%d,%d} flags %02x; "
                            "lc foot (%d,%d) bounds {%d,%d,%d,%d}\n",
                            stage, field<std::int32_t>(native, kFootX), field<std::int32_t>(native, kFootY),
                            bounds.min_x, bounds.min_y, bounds.max_x, bounds.max_y, mb.min_x, mb.min_y, mb.max_x,
                            mb.max_y, native[kBoundsFlags], lc.down_foot_x, lc.down_foot_y, lc.sprite_bounds.min_x,
                            lc.sprite_bounds.min_y, lc.sprite_bounds.max_x, lc.sprite_bounds.max_y);
            }
        }

        // ---- release both creatures, as the game would ----
        // (After a refused load native holds no parts and no gallery.)
        if (!refused_case) native_clear(native);
        native_clear_cache();
        creature.reset();
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: genome %u sex %u last stage %u: %s\n", index, static_cast<unsigned>(which),
                        static_cast<unsigned>(sex), last_stage, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld loads (%ld refused by both), %ld parts compared, %ld sprite file bytes compared\n",
                loads, refused, parts_compared, sprite_bytes);
    std::printf("%ld cases: %ld differ%s\n", cases, differing + differing_palettes,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing + differing_palettes != 0 ? 1 : 0;
}
