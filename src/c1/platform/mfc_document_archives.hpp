#pragma once

#include "mfc_archive.hpp"
#include "mfc_object_archive.hpp"

#include "../application/document.hpp"
#include "../scripting/macro.hpp"
#include "../ui/event_bar.hpp"
#include "../ui/score.hpp"
#include "../ui/toolbars.hpp"
#include "../world/places.hpp"

namespace creatures1::platform {

// These adapters are deliberately boring: each one maps a typed C1 archive
// interface to the already verified MFC byte primitives.  They keep record
// ownership and ordering in the game classes instead of growing a second
// document serializer in the Windows entry point.
class MfcFavouritePlaceArchive final
    : public application::FavouritePlaceArchive {
public:
    explicit MfcFavouritePlaceArchive(MfcArchiveStream& stream)
        : stream_(stream) {}

    bool is_loading() const override;
    std::string read_string() override;
    void write_string(std::string_view value) override;
    std::int16_t read_int16() override;
    void write_int16(std::int16_t value) override;

private:
    MfcArchiveStream& stream_;
};

class MfcToolbarArchive final : public ui::ToolbarArchiveApi {
public:
    explicit MfcToolbarArchive(MfcArchiveStream& stream) : stream_(stream) {}

    bool loading() const override;
    std::int16_t read_selector_count() override;
    void write_selector_count(std::int16_t count) override;
    std::string read_string() override;
    void write_string(std::string_view value) override;

private:
    MfcArchiveStream& stream_;
};

class MfcScoreArchive final : public ui::ScoreArchive {
public:
    explicit MfcScoreArchive(MfcArchiveStream& stream) : stream_(stream) {}

    bool is_loading() const override;
    std::int32_t read_int32() override;
    void write_int32(std::int32_t value) override;

private:
    MfcArchiveStream& stream_;
};

class MfcEventBarArchive final : public ui::EventBarArchiveApi {
public:
    MfcEventBarArchive(MfcArchiveStream& stream, MfcDynamicObjectTable& objects)
        : stream_(stream), objects_(objects) {}

    bool is_loading() const override;
    std::int32_t read_displayed_object_count() override;
    objects::Object* read_object() override;
    void write_displayed_object_count(std::int32_t count) override;
    void write_object(objects::Object* object) override;
    std::int32_t read_legacy_state_word() override;
    void write_legacy_state_word(std::int32_t value) override;

private:
    MfcArchiveStream& stream_;
    MfcDynamicObjectTable& objects_;
};

class MfcMacroArchive final : public scripting::MacroArchive {
public:
    MfcMacroArchive(MfcArchiveStream& stream, MfcDynamicObjectTable& objects)
        : stream_(stream), objects_(objects) {}

    bool loading() const override;
    std::uint32_t read_uint32() override;
    std::int32_t read_int32() override;
    std::string read_string() override;
    void write_uint32(std::uint32_t value) override;
    void write_int32(std::int32_t value) override;
    void write_string(std::string_view value) override;
    objects::Object* read_object() override;
    void write_object(objects::Object* object) override;

private:
    MfcArchiveStream& stream_;
    MfcDynamicObjectTable& objects_;
};

} // namespace creatures1::platform
