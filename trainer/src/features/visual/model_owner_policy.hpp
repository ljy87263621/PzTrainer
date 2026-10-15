#pragma once
#include "bridge/game_snapshot.hpp"
#include "features/visual/visibility_options.hpp"
#include <unordered_map>
namespace pztrainer::features::visual {
struct ModelOwner { int group=-1; bool visible=false; };
using ModelOwnerMap=std::unordered_map<std::int32_t,ModelOwner>;
inline ModelOwnerMap BuildModelOwnerMap(const bridge::FrameSnapshot& frame) {
    ModelOwnerMap owners;
    owners.reserve(frame.zombies.size()+frame.players.size()+frame.animals.size()+frame.vehicles.size());
    const auto append=[&](const auto& objects,int base) {
        for(const auto& object:objects)if(object.identity!=0) {
            const auto inserted=owners.emplace(object.identity,ModelOwner{base+(object.behind_wall?1:object.in_view?2:0),object.game_visible});
            if(!inserted.second)inserted.first->second=ModelOwner{};
        }
    };
    append(frame.zombies,0);append(frame.players,3);append(frame.animals,6);append(frame.vehicles,9);
    return owners;
}
inline ModelOwner ResolveModelOwner(std::int32_t identity,const ModelOwnerMap& owners) {
    const auto found=owners.find(identity);return found==owners.end()?ModelOwner{}:found->second;
}
inline bool ModelLayerVisible(const ModelOwner& owner,VisualFeature feature) {
    return owner.group>=0 && GetVisibilityOptions().Allows(static_cast<VisualCategory>(owner.group/3),feature,owner.visible);
}
}
