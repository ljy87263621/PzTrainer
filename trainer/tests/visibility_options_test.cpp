#include "features/visual/visibility_options.hpp"
#include "features/visual/visual_settings.hpp"
#include <cassert>
#include <sstream>
using namespace pztrainer::features::visual;
int main() {
    VisibilityOptions options{};
    assert(options.Allows(VisualCategory::Zombies, VisualFeature::Box, false));
    options.only_visible[0][static_cast<unsigned>(VisualFeature::Box)] = 1;
    assert(!options.Allows(VisualCategory::Zombies, VisualFeature::Box, false));
    assert(options.Allows(VisualCategory::Zombies, VisualFeature::Box, true));
    assert(options.Allows(VisualCategory::Zombies, VisualFeature::Skeleton, false));
    VisualSettings settings{}; settings.colored_marker=true; settings.show_skeleton=true;
    auto filtered=FilterVisibility(settings,options,VisualCategory::Zombies,false);
    assert(!filtered.colored_marker && filtered.show_skeleton && settings.colored_marker);
    std::stringstream stream; assert(WriteVisibilityOptions(stream,options));
    VisibilityOptions decoded{}; assert(ReadVisibilityOptions(stream,decoded));
    assert(decoded.only_visible==options.only_visible);
    auto bytes=stream.str(); bytes.back()=2; std::stringstream invalid(bytes);
    assert(!ReadVisibilityOptions(invalid,decoded));
    std::stringstream missing; assert(!ReadVisibilityOptions(missing,decoded));
}
