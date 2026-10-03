#pragma once

#include <jni.h>
#include <cstdint>
#include <string>

#include "bridge/game_snapshot.hpp"

namespace pztrainer::bridge {

bool EnsureRageFireBridge(JNIEnv* env, std::string& error);

bool PrepareRageFireTarget(JNIEnv* env, int character_id, int target_id, int bone,
                          bool target_is_player, bool visible, bool automatic, bool redirect_hits,
                          WorldPoint& point, std::string& error);
void ClearRageFireTarget(JNIEnv* env);
void ReleaseRageAutomaticAim(JNIEnv* env);
bool RageAttackNeedsHitList(JNIEnv* env, int character_id);
void MarkRageAttackHitListWritten(JNIEnv* env);
std::uint64_t RageActualShotCalls(JNIEnv* env);
std::uint64_t RageJavaDirectionCalls(JNIEnv* env);
std::uint64_t RageJavaHitListCalls(JNIEnv* env);
std::uint64_t RageJavaBodyPartCalls(JNIEnv* env);

}  // namespace pztrainer::bridge
