#pragma once

#include <jni.h>

namespace pztrainer::features::aim {

bool TryReplaceMagicBulletHitList(JNIEnv* env, int character_id,
                                  int target_id, int body_part,
                                  bool target_is_player);
void ResetMagicBulletShotTracking();

}  // namespace pztrainer::features::aim
