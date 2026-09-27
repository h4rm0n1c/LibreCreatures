#pragma once

// The 1996 kits' dosage syringe, for the classic look: the Biochemistry
// Kit's (CInjectPage) and the Science Kit's injection pages.
//
// DOSE.bmp (156 x 244, the game's folder) is the picture; over it a dosage
// sprite pair (Dosage.spr and its coloured versions): six frames of the
// drop at the needle, drawn at (80, 190), and the barrel empty and full,
// drawn at (35, 10) -- full up to the dose's level, filling from the needle
// end (CInjectPage's constructor geometry and SetDosagePreviewFrame
// @ 0x00407540).  An injection squeezes the drop out, drains the barrel four
// pixels a tick and lets the drop form again (OnDosagePreviewTimer
// @ 0x00408970, ticked every 10 ms).  The original then showed the syringe
// empty while its amount still held the dose; here the barrel refills to it.

#include "c1kitshell/kit_art.hpp"

#include <afxwin.h>

#include <string>

namespace c1kitshell {

class Syringe {
public:
    static constexpr unsigned kTickMs = 10;

    // DOSE.bmp from `directory` (with its trailing slash), the palette, and
    // a dosage sprite file there.  False, and nothing drawn, if any is
    // missing.
    bool load(const std::string& directory, const std::string& dosage_file,
              const GamePalette& palette);
    // Another liquid (a coloured dosage file).
    bool set_liquid(const std::string& dosage_file);
    bool loaded() const { return loaded_; }

    // 0 .. 255.  Shown at once unless an injection is running.
    void set_dose(int dose);
    int dose() const { return dose_; }
    // The level shown now, as a dose (for a slider that follows the
    // barrel as it drains).
    int shown_dose() const;

    void start_injection();
    bool injecting() const { return phase_ != Phase::idle; }
    // One animation step; false once the injection is over.
    bool tick();

    // Centred in `area`, on white with a black frame (the 1996 picture
    // control, SS_BLACKFRAME).
    void draw(CDC& dc, const CRect& area);

private:
    enum class Phase { idle, squeeze, drain, reform };
    int level_for(int dose) const;
    void compose();

    std::string directory_;
    const GamePalette* palette_ = nullptr;
    KitSprite drops_;
    KitSprite barrel_;
    Canvas canvas_;
    bool loaded_ = false;
    int dose_ = 0;
    int level_ = 0;       // pixels of barrel filled
    int drop_frame_ = 0;
    Phase phase_ = Phase::idle;
};

} // namespace c1kitshell
