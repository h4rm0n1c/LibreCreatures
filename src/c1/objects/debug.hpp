#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::objects {

// This is the four-byte classifier value embedded in Object at offset 0x04.
// It is a semantic projection for the debug-label path, not a replacement
// for the complete Object ABI layout.
enum class ClassifierFamily : std::uint8_t {
    scenery = 1,
    simple_object = 2,
    compound_object = 3,
    creature = 4,
};

struct ObjectDebugClassifier {
    std::uint8_t event = 0;
    std::uint8_t species = 0;
    std::uint8_t genus = 0;
    ClassifierFamily family = ClassifierFamily::scenery;
};
static_assert(sizeof(ObjectDebugClassifier) == 4,
              "C1 classifier storage must remain four bytes");

struct ObjectDebugLabelInput {
    ObjectDebugClassifier classifier{};
};

struct GoalDriveLabelCatalog {
    std::array<std::string_view, 40> labels{};

    std::string_view at(std::size_t index) const;
};

extern const GoalDriveLabelCatalog k_goal_drive_labels;

// Formats the exact C1 object-debug label policy. pointer_tool identifies the
// singleton that receives the literal ":Pointer" suffix; ordinary objects
// receive the classifier species suffix instead.
std::string format_object_debug_label(
    const ObjectDebugLabelInput* object,
    const ObjectDebugLabelInput* pointer_tool = nullptr,
    const GoalDriveLabelCatalog& labels = k_goal_drive_labels);

} // namespace creatures1::objects
