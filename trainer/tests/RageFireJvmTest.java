import java.lang.reflect.Field;
import java.lang.reflect.Method;
import pztrainer.player.aim.RageFireOverrides;

public class RageFireJvmTest {
    private static native String install();
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    private static void prepareControlledTarget() throws Exception {
        Field requested = RageFireOverrides.class.getDeclaredField("requested");
        Field prepared = RageFireOverrides.class.getDeclaredField("prepared");
        requested.setAccessible(true);
        prepared.setAccessible(true);
        prepared.set(null, requested.get(null));
    }
    private static float component(Object vector, String name) throws Exception {
        return vector.getClass().getField(name).getFloat(vector);
    }
    private static void field(Object object, Class<?> owner, String name, Object value) throws Exception {
        Field field = owner.getDeclaredField(name);
        field.setAccessible(true);
        field.set(object, value);
    }
    public static void main(String[] args) throws Exception {
        System.load(args[0]);
        String error = install();
        check(error.isEmpty(), error);
        Class<?> boneType = Class.forName("zombie.core.skinnedmodel.model.SkeletonBone");
        String[] bones = {"Bip01_Head", "Bip01_Neck", "Bip01_Spine1", "Bip01_Spine", "Bip01_Pelvis",
            "Bip01_L_Clavicle", "Bip01_L_UpperArm", "Bip01_L_Forearm", "Bip01_L_Hand", "Bip01_R_Clavicle",
            "Bip01_R_UpperArm", "Bip01_R_Forearm", "Bip01_R_Hand", "Bip01_L_Thigh", "Bip01_L_Calf",
            "Bip01_L_Foot", "Bip01_R_Thigh", "Bip01_R_Calf", "Bip01_R_Foot"};
        for (String bone : bones) boneType.getField(bone);
        Class<?> animationType = Class.forName("zombie.core.skinnedmodel.animation.AnimationPlayer");
        animationType.getMethod("isReady");
        animationType.getMethod("getBoneWorldPosition", boneType, Class.forName("org.lwjgl.util.vector.Vector3f"));
        Class.forName("zombie.iso.IsoWorld").getField("instance");
        Class.forName("zombie.iso.IsoCell").getMethod("getZombieList");
        Class.forName("zombie.iso.IsoCell").getMethod("getObjectList");
        Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
        Field unsafeField = unsafeType.getDeclaredField("theUnsafe");
        unsafeField.setAccessible(true);
        Object unsafe = unsafeField.get(null);
        Method allocate = unsafeType.getMethod("allocateInstance", Class.class);
        Class<?> playerType = Class.forName("zombie.characters.IsoPlayer");
        Object player = allocate.invoke(unsafe, playerType);
        playerType.getMethod("setInstance", playerType).invoke(null, player);
        int id = ((Number) playerType.getMethod("getID").invoke(player)).intValue();
        Class<?> characterType = Class.forName("zombie.characters.IsoGameCharacter");
        Class<?> controllerType = Class.forName("zombie.core.physics.BallisticsController");
        var constructor = controllerType.getDeclaredConstructor();
        constructor.setAccessible(true);
        Object controller = constructor.newInstance();
        controllerType.getMethod("setIsoGameCharacter", characterType).invoke(controller, player);
        Class<?> vectorType = Class.forName("zombie.iso.Vector3");
        Object position = vectorType.getConstructor(float.class, float.class, float.class).newInstance(1f, 2f, 3f);
        Object direction = vectorType.getConstructor(float.class, float.class, float.class).newInstance(-1f, 0f, 0f);
        Method calculate = controllerType.getMethod("calculateMuzzlePosition", vectorType, vectorType);
        Method set = vectorType.getMethod("set", float.class, float.class, float.class);
        RageFireOverrides.publish(id, 99, 0, false, false, false, true, 5, 10, 3);
        prepareControlledTarget();
        RageFireOverrides.beginAttack(player);
        calculate.invoke(controller, position, direction);
        check(Math.abs(component(direction, "x") - 4f / (float) Math.sqrt(80)) < 0.001, "Real muzzle X was not redirected");
        check(Math.abs(component(direction, "y") - 8f / (float) Math.sqrt(80)) < 0.001, "Real muzzle Y was not redirected");
        check(component(direction, "z") == 0, "Incorrect world height direction");
        check(RageFireOverrides.directionCalls == 1, "Real firing calculation not recorded");
        check(RageFireOverrides.needsHitList(id), "Attack hit list should be prepared once");
        RageFireOverrides.hitListWritten();
        check(!RageFireOverrides.needsHitList(id), "Attack hit list repeated");
        RageFireOverrides.endAttack();
        set.invoke(direction, -1f, 0f, 0f);
        calculate.invoke(controller, position, direction);
        check(component(direction, "x") == -1, "Preview or ordinary aim was modified");
        Object other = allocate.invoke(unsafe, playerType);
        controllerType.getMethod("setIsoGameCharacter", characterType).invoke(controller, other);
        RageFireOverrides.beginAttack(other);
        calculate.invoke(controller, position, direction);
        check(component(direction, "x") == -1, "Another player's muzzle was modified");
        RageFireOverrides.endAttack();
        Class<?> zombieType = Class.forName("zombie.characters.IsoZombie");
        Object victim = allocate.invoke(unsafe, zombieType);
        characterType.getMethod("setHealth", float.class).invoke(victim, 1f);
        Class<?> weaponType = Class.forName("zombie.inventory.types.HandWeapon");
        Object weapon = allocate.invoke(unsafe, weaponType);
        weaponType.getMethod("setRanged", boolean.class).invoke(weapon, true);
        field(player, characterType, "leftHandItem", weapon);
        Class<?> infoType = Class.forName("zombie.network.fields.hit.HitInfo");
        Class<?> listType = Class.forName("zombie.util.list.PZArrayList");
        Object list = listType.getConstructor(Class.class, int.class).newInstance(infoType, 2);
        field(player, characterType, "hitInfoList", list);
        RageFireOverrides.publish(id, 99, 0, false, false, false, true, 5, 10, 3);
        Field requested = RageFireOverrides.class.getDeclaredField("requested");
        Field prepared = RageFireOverrides.class.getDeclaredField("prepared");
        requested.setAccessible(true);
        prepared.setAccessible(true);
        Object raw = requested.get(null);
        var targetConstructor = raw.getClass().getDeclaredConstructors()[0];
        targetConstructor.setAccessible(true);
        Object controlled = targetConstructor.newInstance(id, 99, 0, false, false, false, true,
            5f, 10f, 3f, System.nanoTime() + 5_000_000_000L, victim);
        prepared.set(null, controlled);
        RageFireOverrides.beginAttack(player);
        RageFireOverrides.finishHitList(player);
        check(((java.util.List<?>) list).size() == 1, "Damage target list not replaced: " + RageFireOverrides.error);
        Object info = ((java.util.List<?>) list).get(0);
        check(infoType.getMethod("getObject").invoke(info) == victim, "Damage list points to the cursor target");
        check(infoType.getField("chance").getInt(info) == 100, "Damage target chance was not set");
        check(RageFireOverrides.hitListCalls == 1, "Damage target replacement not recorded");
        check(RageFireOverrides.hitReaction("ShotLegL", player, victim).equals("Shot"), "Old cursor hit reaction retained");
        check(RageFireOverrides.cameraHit(false, player, victim), "Selected target not recognized by damage processing");
        Class<?> bodyType = Class.forName("zombie.core.physics.RagdollBodyPart");
        int head = ((Enum<?>) bodyType.getField("BODYPART_HEAD").get(null)).ordinal();
        check(RageFireOverrides.bodyPart(4, player, victim) == head, "Head aim was treated as a leg hit");
        check(RageFireOverrides.bodyPart(4, other, victim) == 4, "Another player's hit part was modified");
        Object otherVictim = allocate.invoke(unsafe, zombieType);
        check(RageFireOverrides.bodyPart(4, player, otherVictim) == 4, "An unselected victim's hit part was modified");
        Class<?> randomType = Class.forName("zombie.core.random.RandStandard");
        randomType.getMethod("init").invoke(randomType.getField("INSTANCE").get(null));
        Class<?> variableType = Class.forName("zombie.core.skinnedmodel.advancedanimation.AnimationVariableSource");
        field(player, characterType, "gameVariables", variableType.getConstructor().newInstance());
        field(other, characterType, "gameVariables", variableType.getConstructor().newInstance());
        Class<?> vector2Type = Class.forName("zombie.iso.Vector2");
        field(player, characterType, "forwardDirection", vector2Type.getConstructor(float.class, float.class).newInstance(1f, 0f));
        field(other, characterType, "forwardDirection", vector2Type.getConstructor(float.class, float.class).newInstance(1f, 0f));
        controllerType.getMethod("setIsoGameCharacter", characterType).invoke(controller, player);
        field(player, characterType, "ballisticsController", controller);
        weaponType.getMethod("setAlwaysKnockdown", boolean.class).invoke(weapon, true);
        weaponType.getMethod("setWeaponCategories", java.util.Set.class).invoke(weapon, java.util.Set.of());
        Class<?> clientType = Class.forName("zombie.network.GameClient");
        clientType.getField("client").setBoolean(null, true);
        Class<?> combatType = Class.forName("zombie.CombatManager");
        Object combat = combatType.getMethod("getInstance").invoke(null);
        prepared.set(null, targetConstructor.newInstance(id, 99, 0, true, false, false, true,
            5f, 10f, 3f, System.nanoTime() + 250_000_000L, other));
        RageFireOverrides.publish(id, 99, 0, true, false, false, true, 5, 10, 3);
        RageFireOverrides.beginAttack(player);
        int hit = (int) combatType.getMethod("processHit", weaponType, characterType, characterType).invoke(combat, weapon, player, other);
        check(hit == head, "Real damage processing did not return the selected head: " + hit);
        check(RageFireOverrides.bodyPartCalls == 2, "Real damage processing did not use the selected part");
        clientType.getField("client").setBoolean(null, false);
        RageFireOverrides.endAttack();
        check(RageFireOverrides.bodyPart(4, player, victim) == 4, "Ordinary hit part was modified outside an attack");
        int[] testBones = {1, 2, 13};
        String[] testParts = {"BODYPART_HEAD", "BODYPART_SPINE", "BODYPART_LEFT_UPPER_LEG"};
        for (int i = 0; i < testBones.length; i++) {
            RageFireOverrides.publish(id, 99, testBones[i], false, false, false, true, 5, 10, 3);
            prepared.set(null, targetConstructor.newInstance(id, 99, testBones[i], false, false, false, true,
                5f, 10f, 3f, System.nanoTime() + 250_000_000L, victim));
            RageFireOverrides.beginAttack(player);
            check(RageFireOverrides.bodyPart(11, player, victim) == ((Enum<?>) bodyType.getField(testParts[i]).get(null)).ordinal(),
                "Selected bone mapped to the wrong body part");
            RageFireOverrides.endAttack();
        }
        RageFireOverrides.publish(id, 99, 0, false, false, false, false, 5, 10, 3);
        prepared.set(null, targetConstructor.newInstance(id, 99, 0, false, false, false, false,
            5f, 10f, 3f, System.nanoTime() + 250_000_000L, victim));
        RageFireOverrides.beginAttack(player);
        check(RageFireOverrides.bodyPart(4, player, victim) == 4, "Original hit calculation was overridden without hit redirection");
        RageFireOverrides.endAttack();
        RageFireOverrides.publish(id, 99, 0, false, false, false, true, 5, 10, 3);
        prepared.set(null, targetConstructor.newInstance(id, 99, 0, false, false, false, true, 5f, 10f, 3f, 0L, victim));
        RageFireOverrides.beginAttack(player);
        check(RageFireOverrides.bodyPart(4, player, victim) == 4, "Expired hit override remained active");
        RageFireOverrides.endAttack();
        RageFireOverrides.clear();
        check(RageFireOverrides.prepared() == null, "Disabled firing override remained active");
        Class<?> inputType = Class.forName("zombie.characters.component.CharacterInputComponent");
        java.util.HashMap<Class<?>, Object> components = new java.util.HashMap<>();
        components.put(inputType, inputType.getConstructor().newInstance());
        field(player, Class.forName("zombie.iso.IsoObject"), "ecsComponentMap", components);
        RageFireOverrides.publish(id, 99, 0, false, false, true, true, 5, 10, 3);
        RageFireOverrides.prepare();
        check((boolean) playerType.getMethod("isForceAim").invoke(player), "Automatic aim was not entered on the game thread");
        Field aiming = characterType.getDeclaredField("isAiming");
        aiming.setAccessible(true);
        check(aiming.getBoolean(player), "Automatic aiming state was not set");
        check(playerType.getField("isCharging").getBoolean(player), "Automatic charging state was not set");
        RageFireOverrides.releaseAim();
        check(!(boolean) playerType.getMethod("isForceAim").invoke(player), "Automatic aim was not released");
        System.out.println("Actual game classes: muzzle redirection, head damage processing, attack scope and player isolation passed.");
    }
}
