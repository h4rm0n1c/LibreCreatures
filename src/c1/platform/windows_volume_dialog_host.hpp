#pragma once

#include "windows_prelude.hpp"
#include <afxcmn.h>
#include <afxwin.h>

#include "../ui/volume_dialog.hpp"
#include "windows_app_state.hpp"
#include "windows_registry_host.hpp"

namespace creatures1::platform {

// Concrete MFC volume dialog behind the semantic ui volume boundary.
class C1NativeVolumeDialog final
    : public CDialog,
      public creatures1::ui::VolumeSliderApi,
      public creatures1::ui::VolumePersistenceApi {
public:
    explicit C1NativeVolumeDialog(CWnd* owner)
        : CDialog(MAKEINTRESOURCEA(147), owner) {}

    BOOL OnInitDialog() override {
        const BOOL result = CDialog::OnInitDialog();
        sound_state_.effect_volume_setting =
            g_active_sound_manager == nullptr
                ? 0
                : g_active_sound_manager->effect_volume_setting();
        sound_state_.target_master_attenuation =
            g_active_sound_manager == nullptr
                ? 0
                : g_active_sound_manager->target_master_attenuation();
        sound_state_.volume_target_update_inhibited =
            g_active_sound_manager != nullptr &&
            g_active_sound_manager->volume_target_update_inhibited();
        creatures1::ui::initialise_volume_dialog(
            *this, g_active_sound_manager == nullptr ? nullptr : &sound_state_);
        return result;
    }

    void OnHScroll(UINT scroll_code, UINT position, CScrollBar* scroll_bar) {
        if (scroll_bar != nullptr &&
            scroll_bar->GetDlgCtrlID() == 1034) {
            creatures1::ui::apply_volume_slider_change(
                *this, &sound_state_, *this);
            if (g_active_sound_manager != nullptr) {
                g_active_sound_manager->set_effect_volume_setting(
                    sound_state_.effect_volume_setting);
            }
        }
        CDialog::OnHScroll(scroll_code, position, scroll_bar);
    }

    std::int32_t position() const override {
        CSliderCtrl* control = slider_control();
        return control == nullptr ? 255 : control->GetPos();
    }

    void set_range(std::int32_t minimum, std::int32_t maximum) override {
        if (CSliderCtrl* control = slider_control(); control != nullptr) {
            control->SetRange(minimum, maximum, TRUE);
        }
    }

    void set_line_increment(std::int32_t increment) override {
        if (CSliderCtrl* control = slider_control(); control != nullptr) {
            control->SetLineSize(increment);
        }
    }

    void set_position(std::int32_t position_value) override {
        if (CSliderCtrl* control = slider_control(); control != nullptr) {
            control->SetPos(position_value);
        }
    }

    void persist_effect_volume(std::int32_t attenuation) override {
        HKEY registry_key = nullptr;
        if (open_c1_secondary_registry(registry_key, KEY_SET_VALUE)) {
            write_registry_dword(
                registry_key, "EffectVolume",
                static_cast<std::uint32_t>(attenuation));
            RegCloseKey(registry_key);
        }
    }

private:
    CSliderCtrl* slider_control() const {
        CWnd* window = const_cast<C1NativeVolumeDialog*>(this)->GetDlgItem(1034);
        return window == nullptr
                   ? nullptr
                   : static_cast<CSliderCtrl*>(
                         CSliderCtrl::FromHandle(window->GetSafeHwnd()));
    }

    creatures1::ui::SoundVolumeState sound_state_{};

    DECLARE_MESSAGE_MAP()
};

} // namespace creatures1::platform
