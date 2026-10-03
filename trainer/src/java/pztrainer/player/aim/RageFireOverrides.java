package pztrainer.player.aim;

import java.lang.reflect.Field;

public final class RageFireOverrides {
    private static final String[] BONES = {"Bip01_Head", "Bip01_Neck", "Bip01_Spine1", "Bip01_Spine", "Bip01_Pelvis",
        "Bip01_L_Clavicle", "Bip01_L_UpperArm", "Bip01_L_Forearm", "Bip01_L_Hand", "Bip01_R_Clavicle",
        "Bip01_R_UpperArm", "Bip01_R_Forearm", "Bip01_R_Hand", "Bip01_L_Thigh", "Bip01_L_Calf", "Bip01_L_Foot",
        "Bip01_R_Thigh", "Bip01_R_Calf", "Bip01_R_Foot"};
    private static final int[] BODY_PARTS = {2, 2, 1, 1, 0, 7, 7, 8, 8, 9, 9, 10, 10, 3, 4, 4, 5, 6, 6};
    private record Target(int characterId, int targetId, int bone, boolean player, boolean visible,
                          boolean automatic, boolean redirectHits, float x, float y, float z, long expires, Object victim) {}
    private static volatile Target requested;
    private static volatile Target prepared;
    private static final ThreadLocal<Target> attack = new ThreadLocal<>();
    private static final ThreadLocal<Boolean> hitListWritten = new ThreadLocal<>();
    public static volatile long shotCalls;
    public static volatile long directionCalls;
    public static volatile long hitListCalls;
    public static volatile long bodyPartCalls;
    private static volatile Object forcedActor;
    private static boolean previousForceAim;
    public static volatile String error = "";

    private static Object call(Object object, String name) throws ReflectiveOperationException {
        return object.getClass().getMethod(name).invoke(object);
    }
    private static Object localPlayer() throws ReflectiveOperationException {
        return Class.forName("zombie.characters.IsoPlayer").getMethod("getInstance").invoke(null);
    }
    private static boolean valid(Target target, Object actor) throws ReflectiveOperationException {
        return target != null && System.nanoTime() <= target.expires && actor != null
            && actor == localPlayer() && ((Number) call(actor, "getID")).intValue() == target.characterId;
    }
    private static boolean same(Target a, Target b) {
        return a != null && b != null && a.characterId == b.characterId && a.targetId == b.targetId
            && a.bone == b.bone && a.player == b.player && a.visible == b.visible && a.automatic == b.automatic
            && a.redirectHits == b.redirectHits;
    }
    private static float component(Object vector, String name) throws ReflectiveOperationException {
        return vector.getClass().getField(name).getFloat(vector);
    }
    private static void vector(Object value, float x, float y, float z) throws ReflectiveOperationException {
        value.getClass().getMethod("set", float.class, float.class, float.class).invoke(value, x, y, z);
    }

    public static void publish(int actor, int target, int bone, boolean player, boolean visible, boolean automatic, boolean redirectHits,
                               float x, float y, float z) {
        requested = new Target(actor, target, bone, player, visible, automatic, redirectHits, x, y, z, System.nanoTime() + 250_000_000L, null);
    }
    public static void clear() { requested = null; prepared = null; }

    public static boolean prepare() {
        Target target = requested;
        boolean ready = false;
        error = "";
        try {
            Object local = localPlayer();
            if (!valid(target, local) || target.bone < 0 || target.bone >= BONES.length) return false;
            if (target.automatic) {
                if (forcedActor == null) {
                    previousForceAim = (boolean) call(local, "isForceAim");
                    forcedActor = local;
                }
                local.getClass().getMethod("setForceAim", boolean.class).invoke(local, true);
                local.getClass().getMethod("setIsAiming", boolean.class).invoke(local, true);
                local.getClass().getField("isCharging").setBoolean(local, true);
                if (call(local, "getBallisticsController") == null) call(local, "updateBallistics");
            } else {
                releaseAim();
            }
            Object world = Class.forName("zombie.iso.IsoWorld").getField("instance").get(null);
            Object cell = call(world, "getCell");
            Iterable<?> objects = (Iterable<?>) call(cell, target.player ? "getObjectList" : "getZombieList");
            Class<?> expected = Class.forName(target.player ? "zombie.characters.IsoPlayer" : "zombie.characters.IsoZombie");
            Object victim = null;
            for (Object object : objects) {
                if (expected.isInstance(object) && ((Number) call(object, "getID")).intValue() == target.targetId) {
                    victim = object; break;
                }
            }
            if (victim == null || (boolean) call(victim, "isDead")) return false;
            Object animation = call(victim, "getAnimationPlayer");
            if (animation == null || !(boolean) call(animation, "isReady")) return false;
            Class<?> boneType = Class.forName("zombie.core.skinnedmodel.model.SkeletonBone");
            Object bone = boneType.getField(BONES[target.bone]).get(null);
            Class<?> vectorType = Class.forName("org.lwjgl.util.vector.Vector3f");
            Object point = vectorType.getConstructor().newInstance();
            animation.getClass().getMethod("getBoneWorldPosition", boneType, vectorType).invoke(animation, bone, point);
            float x = component(point, "x"), y = component(point, "y"), z = component(point, "z");
            if (!Float.isFinite(x) || !Float.isFinite(y) || !Float.isFinite(z) || !same(target, requested)) return false;
            prepared = new Target(target.characterId, target.targetId, target.bone, target.player, target.visible,
                target.automatic, target.redirectHits, x, y, z, System.nanoTime() + 250_000_000L, victim);
            if (target.visible) aim(local, prepared);
            error = "";
            ready = true;
            return true;
        } catch (ReflectiveOperationException | RuntimeException failure) {
            error = failure.toString();
            return false;
        } finally {
            if (!ready) prepared = null;
        }
    }

    public static float[] prepared() {
        Target target = prepared;
        return same(target, requested) && System.nanoTime() <= target.expires
            ? new float[]{target.targetId, target.x, target.y, target.z, target.bone} : null;
    }

    public static boolean hasForcedAim() { return forcedActor != null; }
    public static void releaseAim() {
        Object actor = forcedActor;
        forcedActor = null;
        try {
            if (actor != null && actor == localPlayer()) actor.getClass().getMethod("setForceAim", boolean.class)
                .invoke(actor, previousForceAim);
        } catch (ReflectiveOperationException failure) { error = failure.toString(); }
    }

    public static void finishHitList(Object actor) {
        try {
            Target target = attack.get();
            if (!valid(target, actor) || !target.redirectHits || target.victim == null || (boolean) call(target.victim, "isDead")) return;
            Class<?> moving = Class.forName("zombie.iso.IsoMovingObject");
            if (target.player && !(boolean) Class.forName("zombie.CombatManager")
                .getMethod("checkPVP", moving, moving, boolean.class).invoke(null, actor, target.victim, false)) return;
            Object weapon = call(actor, "getPrimaryHandItem");
            if (weapon == null || !(boolean) call(weapon, "isRanged")) return;
            float dx = target.x - ((Number) call(actor, "getX")).floatValue();
            float dy = target.y - ((Number) call(actor, "getY")).floatValue();
            float dz = target.z - ((Number) call(actor, "getZ")).floatValue();
            Class<?> infoType = Class.forName("zombie.network.fields.hit.HitInfo");
            Object info = infoType.getConstructor().newInstance();
            infoType.getMethod("init", moving, float.class, float.class, float.class, float.class, float.class)
                .invoke(info, target.victim, 1.0f, dx * dx + dy * dy + dz * dz, target.x, target.y, target.z);
            infoType.getField("chance").setInt(info, 100);
            call(actor, "clearHitInfo");
            @SuppressWarnings("unchecked")
            java.util.List<Object> list = (java.util.List<Object>) call(actor, "getHitInfoList");
            list.add(info);
            hitListWritten.set(true);
            hitListCalls++;
        } catch (ReflectiveOperationException | RuntimeException failure) { error = failure.toString(); }
    }

    private static Target hitTarget(Object actor, Object victim) throws ReflectiveOperationException {
        Target target = attack.get();
        return valid(target, actor) && target.redirectHits && target.victim == victim
            && target.bone >= 0 && target.bone < BODY_PARTS.length ? target : null;
    }

    public static String hitReaction(String original, Object actor, Object victim) {
        try { if (hitTarget(actor, victim) != null) return "Shot"; }
        catch (ReflectiveOperationException failure) { error = failure.toString(); }
        return original;
    }
    public static boolean cameraHit(boolean original, Object actor, Object victim) {
        try { if (hitTarget(actor, victim) != null) return true; }
        catch (ReflectiveOperationException failure) { error = failure.toString(); }
        return original;
    }
    public static int bodyPart(int original, Object actor, Object victim) {
        try {
            Target target = hitTarget(actor, victim);
            if (target != null) {
                bodyPartCalls++;
                return BODY_PARTS[target.bone];
            }
        } catch (ReflectiveOperationException failure) { error = failure.toString(); }
        return original;
    }

    private static void aim(Object actor, Target target) throws ReflectiveOperationException {
        float x = target.x - ((Number) call(actor, "getAimOriginPosX")).floatValue();
        float y = target.y - ((Number) call(actor, "getAimOriginPosY")).floatValue();
        float length = (float) Math.hypot(x, y);
        if (length > 0.001f) actor.getClass().getMethod("setForwardDirection", float.class, float.class)
            .invoke(actor, x / length, y / length);
        Object controller = call(actor, "getBallisticsController");
        if (controller != null) {
            Object muzzle = call(controller, "getMuzzlePosition");
            float horizontal = (float) Math.hypot(target.x - component(muzzle, "x"), target.y - component(muzzle, "y"));
            if (horizontal > 0.001f) actor.getClass().getMethod("setTargetVerticalAimAngle", float.class)
                .invoke(actor, (float) Math.toDegrees(Math.atan2((target.z - component(muzzle, "z")) * 2.44949f, horizontal)));
        }
    }

    public static void beginAttack(Object actor) {
        attack.remove();
        hitListWritten.set(false);
        try {
            Target target = prepared;
            if (same(target, requested) && valid(target, actor)) {
                if (target.visible) aim(actor, target);
                attack.set(target);
            }
        } catch (ReflectiveOperationException failure) { error = failure.toString(); }
    }
    public static void endAttack() { attack.remove(); hitListWritten.remove(); }
    public static boolean needsHitList(int actor) {
        Target target = attack.get();
        return target != null && target.characterId == actor && System.nanoTime() <= target.expires
            && !Boolean.TRUE.equals(hitListWritten.get());
    }
    public static void hitListWritten() { hitListWritten.set(true); }
    public static void markShot(Object weapon, Object actor) {
        try { if (weapon != null && (boolean) call(weapon, "isRanged") && valid(attack.get(), actor)) shotCalls++; }
        catch (ReflectiveOperationException failure) { error = failure.toString(); }
    }

    public static float adjustMuzzle(float original, Object controller, Object position, Object direction) {
        try {
            Target target = attack.get();
            Field owner = controller.getClass().getDeclaredField("isoGameCharacter");
            owner.setAccessible(true);
            if (!valid(target, owner.get(controller))) return original;
            float x = target.x - component(position, "x");
            float y = target.y - component(position, "y");
            float z = target.z - component(position, "z");
            float length = (float) Math.sqrt(x * x + y * y + z * z);
            if (Float.isFinite(length) && length > 0.001f) {
                vector(direction, x / length, y / length, z / length);
                directionCalls++;
            }
        } catch (ReflectiveOperationException | RuntimeException failure) { error = failure.toString(); }
        return original;
    }
}
