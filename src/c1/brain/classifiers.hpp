#pragma once

#include <map>
#include <string>

namespace creatures1::brain {

struct ClassifierId {
    int family;
    int genus;
    int species;
};

using ClassifierNameMap = std::map<std::string, std::string>;

// Resolve the most specific display name recorded for a classifier.  The
// executable checks an exact key first, then a family/genus key with a
// wildcard species, and finally a family-only key.
std::string ResolveClassifierDisplayName(
    const ClassifierNameMap& names,
    ClassifierId classifier);

}  // namespace creatures1::brain
