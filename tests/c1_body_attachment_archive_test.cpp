#include "../src/c1/creatures/body.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace creatures1;

struct Archive final : objects::EntityArchive {
    bool loading = false;
    std::size_t cursor = 0;
    std::vector<std::uint8_t> bytes;
    bool is_loading() const override { return loading; }
    display::Gallery* read_gallery() override { return nullptr; }
    void write_gallery(display::Gallery*) override {}
    std::uint8_t read_byte() override { return bytes.at(cursor++); }
    std::int32_t read_int32() override { return 0; }
    void write_byte(std::uint8_t value) override { bytes.push_back(value); }
    void write_int32(std::int32_t) override {}
    void read_bytes(void* destination, std::size_t count) override {
        assert(cursor + count <= bytes.size());
        std::memcpy(destination, bytes.data() + cursor, count);
        cursor += count;
    }
    void write_bytes(const void* source, std::size_t count) override {
        auto* data = static_cast<const std::uint8_t*>(source);
        bytes.insert(bytes.end(), data, data + count);
    }
    void* read_object_reference(std::string_view) override { return nullptr; }
    void write_object_reference(const void*, std::string_view) override {}
};

int main() {
    // Native Body::Serialize: six chain rows, each with ten X/Y pairs.
    // This independently specified wire order catches a symmetric wrong
    // reader/writer that would pass a simple round-trip test.
    Archive wire;
    wire.bytes = {0, 0, 0}; // Entity image, base, absent animation sequence.
    for (unsigned pair = 0; pair < 60; ++pair) {
        wire.bytes.push_back(pair);
        wire.bytes.push_back(100 + pair);
    }
    wire.loading = true;
    creatures::Body body;
    body.serialize(wire);
    assert(wire.cursor == wire.bytes.size());
    for (unsigned chain = 0; chain < 6; ++chain) {
        for (unsigned view = 0; view < 10; ++view) {
            assert(body.attachment_table.join_x[chain][view] == chain * 10 + view);
            assert(body.attachment_table.join_y[chain][view] == 100 + chain * 10 + view);
        }
    }
    Archive saved;
    body.serialize(saved);
    assert(saved.bytes == wire.bytes);
    std::cout << "native body attachment archive order: PASS\n";
}
