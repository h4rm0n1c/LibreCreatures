#pragma once

#include "../application/application.hpp"
#include <string>

#include "../brain/classifiers.hpp"
#include "../sound/sound.hpp"

namespace creatures1::platform {

// Borrowed process-wide pointers shared by the startup host, the document and
// the view.  The startup host publishes them; nothing here owns them.
extern const creatures1::application::SfcAppResourceDirectories*
    g_active_primary_directories;
extern const creatures1::application::SfcAppResourceDirectories*
    g_active_secondary_directories;
extern const creatures1::application::SfcAppState* g_active_app_state;
extern creatures1::sound::SoundManager* g_active_sound_manager;
// g_classifier_name_map in the native: written once at startup from the
// classifier name file, read by the classifier tip and magic profiler.
extern const creatures1::brain::ClassifierNameMap* g_active_classifier_names;
// The native reads the world-save path from a process global when deriving
// the profiler report filename; the startup host owns the string.
extern const std::string* g_active_world_save_path;

} // namespace creatures1::platform
