#pragma once
#include <jni.h>
#include "bridge/item_bridge.hpp"

namespace pztrainer::bridge {
std::size_t AppendMoveableItemCatalog(JNIEnv* env,std::vector<ItemCatalogEntry>& entries,
                                    std::string& error);
}
