// The classic look's dosage syringe.  See kit_syringe.hpp.

#include "c1kitshell/kit_syringe.hpp"

#include <algorithm>

namespace c1kitshell {
namespace {

constexpr int kPictureWidth = 156;   // DOSE.bmp
constexpr int kPictureHeight = 244;
constexpr int kDropX = 80;
constexpr int kDropY = 190;
constexpr int kBarrelX = 35;
constexpr int kBarrelTop = 10;
constexpr int kBarrelBottom = 223;  // the level fills up from here
constexpr int kDrainStep = 4;
constexpr int kDropFrames = 6;

} // namespace

bool Syringe::load(const std::string& directory, const std::string& dosage_file,
                   const GamePalette& palette) {
    directory_ = directory;
    palette_ = &palette;
    loaded_ = canvas_.create(kPictureWidth, kPictureHeight) && set_liquid(dosage_file);
    return loaded_;
}

bool Syringe::set_liquid(const std::string& dosage_file) {
    if (!drops_.load_pair(directory_ + dosage_file, barrel_) || drops_.frame_count() < kDropFrames ||
        barrel_.frame_count() < 2) {
        return false;
    }
    compose();
    return true;
}

// CInjectPage::OnInitDialog's frame_index_by_dosage table: dose * height / 256.
int Syringe::level_for(int dose) const {
    return dose * (kBarrelBottom - kBarrelTop) / 256;
}

int Syringe::shown_dose() const {
    const int full = level_for(dose_);
    return full > 0 ? level_ * dose_ / full : 0;
}

void Syringe::set_dose(int dose) {
    dose_ = (std::max)(0, (std::min)(255, dose));
    if (phase_ == Phase::idle) {
        level_ = level_for(dose_);
        drop_frame_ = 0;
        compose();
    }
}

void Syringe::start_injection() {
    phase_ = Phase::squeeze;
    drop_frame_ = 0;
}

bool Syringe::tick() {
    switch (phase_) {
    case Phase::squeeze:
        if (++drop_frame_ >= kDropFrames - 1) {
            drop_frame_ = kDropFrames - 1;
            phase_ = Phase::drain;
        }
        break;
    case Phase::drain:
        level_ = (std::max)(0, level_ - kDrainStep);
        if (level_ == 0) phase_ = Phase::reform;
        break;
    case Phase::reform:
        if (--drop_frame_ <= 0) {
            drop_frame_ = 0;
            phase_ = Phase::idle;
            level_ = level_for(dose_);  // (the original left it empty)
        }
        break;
    case Phase::idle:
        return false;
    }
    compose();
    return phase_ != Phase::idle;
}

void Syringe::compose() {
    if (palette_ == nullptr || canvas_.width() == 0) return;
    canvas_.draw_bitmap_file(directory_ + "DOSE.bmp", 0, 0);
    const KitSprite::Frame* empty = barrel_.frame(0);
    const KitSprite::Frame* full = barrel_.frame(1);
    if (empty != nullptr && full != nullptr) {
        // Row by row: full below the level, empty above.
        const int level_row = kBarrelBottom - level_;
        for (int row = 0; row < empty->height; ++row) {
            const KitSprite::Frame* source = kBarrelTop + row >= level_row ? full : empty;
            canvas_.draw_indexed(source->pixels.data() + static_cast<std::size_t>(row) * source->width,
                                 source->width, 1, source->width, false, kBarrelX, kBarrelTop + row,
                                 *palette_);
        }
    }
    canvas_.draw_frame(drops_, drop_frame_, kDropX, kDropY, *palette_);
}

void Syringe::draw(CDC& dc, const CRect& area) {
    dc.FillSolidRect(area, RGB(255, 255, 255));
    CBrush frame(RGB(0, 0, 0));
    dc.FrameRect(area, &frame);
    if (!loaded_) return;
    canvas_.present(dc, area.left + (area.Width() - kPictureWidth) / 2,
                    area.top + (area.Height() - kPictureHeight) / 2, kPictureWidth, kPictureHeight);
}

} // namespace c1kitshell
