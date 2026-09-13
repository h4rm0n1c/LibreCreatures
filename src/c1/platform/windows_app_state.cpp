#include "windows_app_state.hpp"

namespace creatures1::platform {

const creatures1::application::SfcAppResourceDirectories*
    g_active_primary_directories = nullptr;
const creatures1::application::SfcAppResourceDirectories*
    g_active_secondary_directories = nullptr;
const creatures1::application::SfcAppState* g_active_app_state = nullptr;
const creatures1::brain::ClassifierNameMap* g_active_classifier_names =
    nullptr;
const std::string* g_active_world_save_path = nullptr;
creatures1::sound::SoundManager* g_active_sound_manager = nullptr;

} // namespace creatures1::platform
