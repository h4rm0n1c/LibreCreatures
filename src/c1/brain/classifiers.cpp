#include "classifiers.hpp"

namespace creatures1::brain {
namespace {

std::string ExactKey(ClassifierId classifier) {
    return std::to_string(classifier.family) + ", " +
           std::to_string(classifier.genus) + ", " +
           std::to_string(classifier.species);
}

std::string GenusKey(ClassifierId classifier) {
    return std::to_string(classifier.family) + ", " +
           std::to_string(classifier.genus) + ", ";
}

std::string FamilyKey(ClassifierId classifier) {
    return std::to_string(classifier.family) + ", , ";
}

const std::string* Find(const ClassifierNameMap& names, const std::string& key) {
    const auto found = names.find(key);
    return found == names.end() ? nullptr : &found->second;
}

}  // namespace

std::string ResolveClassifierDisplayName(
    const ClassifierNameMap& names,
    ClassifierId classifier) {
    for (const std::string& key : {
             ExactKey(classifier), GenusKey(classifier), FamilyKey(classifier)}) {
        if (const std::string* name = Find(names, key)) {
            return *name;
        }
    }
    return {};
}

}  // namespace creatures1::brain
