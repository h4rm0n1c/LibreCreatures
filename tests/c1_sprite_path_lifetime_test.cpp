#include "../src/c1/display/sprite_cache.hpp"
#include "../src/c1/creatures/skeleton.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

using creatures1::display::SpriteFileSearchPaths;
using creatures1::creatures::SkeletonSpriteBuildServices;

// Both returned bundles must own their paths, including the service bundle
// retained by creature construction/import adapters after the factory returns.
static_assert(std::is_same_v<
    decltype(SkeletonSpriteBuildServices::secondary_image_directory), std::string>);
static_assert(std::is_same_v<
    decltype(SkeletonSpriteBuildServices::primary_image_directory), std::string>);

static SpriteFileSearchPaths resolve_paths(const std::string& root) {
    return {root + "\\Images\\", std::string("C:\\Creatures\\Images\\")};
}

int main() {
    for (const std::string root : {std::string("C:\\w"),
            std::string("C:\\Users\\Tester\\Documents\\Creatures\\Creatures 1\\knowngood")}) {
        const auto expected = root + "\\Images\\";
        auto paths = resolve_paths(root);
        auto copied = paths;
        paths = resolve_paths("C:\\another-world");
        std::vector<std::string> churn(1000, std::string(expected.size(), 'x'));
        assert(copied.secondary_image_directory == expected);
        assert(copied.primary_image_directory == "C:\\Creatures\\Images\\");
        assert(paths.secondary_image_directory == "C:\\another-world\\Images\\");
    }
    std::string source = "C:\\original\\Images\\";
    SpriteFileSearchPaths from_views{std::string_view(source), std::string_view(source)};
    source.assign(source.size(), 'x');
    assert(from_views.secondary_image_directory == "C:\\original\\Images\\");
    assert(from_views.primary_image_directory == "C:\\original\\Images\\");
    std::cout << "sprite path ownership: PASS\n";
}
