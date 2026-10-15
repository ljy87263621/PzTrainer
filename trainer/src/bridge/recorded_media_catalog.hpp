#pragma once
#include "bridge/item_bridge.hpp"
#include <jni.h>

namespace pztrainer::bridge {
// Borrowed global reference; the helper is retained for the lifetime of the DLL.
jclass RecordedMediaItemHelper(JNIEnv* env, std::string& error);
std::size_t AppendRecordedMediaCatalog(JNIEnv* env, std::vector<ItemCatalogEntry>& entries,
                                     std::string& error);
}
