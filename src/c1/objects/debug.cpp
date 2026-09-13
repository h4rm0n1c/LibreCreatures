#include "debug.hpp"

#include <cstddef>

namespace creatures1::objects {

const GoalDriveLabelCatalog k_goal_drive_labels{{
    "SELF (0)",       "system obj (1)", "callbutton (2)", "simp:03",
    "food (4)",       "egg (5)",       "toy (6)",        "bird (7)",
    "curio (8)",      "instrument (9)", "fish (10)",      "AV equip (11)",
    "shower (12)",    "simp:13",       "simp:14",        "simp:15",
    "simp:16",        "simp:17",       "simp:18",        "simp:19",
    "simp:20",        "simp:21",       "simp:22",        "simp:23",
    "simp:24",        "simp:25",       "vehicle (26)",   "lift (27)",
    "blackboard (28)", "AV equip (29)", "comp:30",        "comp:31",
    "comp:32",        "comp:33",       "comp:34",        "comp:35",
    "norn",           "gren",          "ettin",          "side",
}};

std::string_view GoalDriveLabelCatalog::at(std::size_t index) const {
    return index < labels.size() ? labels[index] : std::string_view{};
}

namespace {

std::size_t resolve_label_index(const ObjectDebugClassifier& classifier) {
    switch (classifier.family) {
    case ClassifierFamily::simple_object:
        return classifier.genus;
    case ClassifierFamily::compound_object:
        return static_cast<std::size_t>(classifier.genus) + 0x19;
    case ClassifierFamily::creature:
        return static_cast<std::size_t>(classifier.genus) + 0x23;
    case ClassifierFamily::scenery:
        return 0;
    }
    return 0;
}

} // namespace

std::string format_object_debug_label(
    const ObjectDebugLabelInput* object,
    const ObjectDebugLabelInput* pointer_tool,
    const GoalDriveLabelCatalog& labels) {
    if (object == nullptr) {
        return "NULL";
    }

    const std::size_t label_index = resolve_label_index(object->classifier);
    std::string result;
    if (label_index < labels.labels.size()) {
        result.assign(labels.labels[label_index]);
    } else {
        result = "<" + std::to_string(label_index) + ">";
    }

    if (object == pointer_tool) {
        result += ":Pointer";
    } else {
        result += ":" + std::to_string(object->classifier.species);
    }
    return result;
}

} // namespace creatures1::objects
