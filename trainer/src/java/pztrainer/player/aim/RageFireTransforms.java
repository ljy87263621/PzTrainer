package pztrainer.player.aim;

import java.lang.constant.ClassDesc;
import java.lang.constant.MethodTypeDesc;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.util.List;
import java.util.function.Predicate;

public final class RageFireTransforms {
    private static final ClassDesc HELPER = ClassDesc.of("pztrainer.player.aim.RageFireOverrides");

    private static Class<?> api(String name) throws ClassNotFoundException {
        return Class.forName("java.lang.classfile." + name);
    }

    private static boolean matches(Object model, String name, String descriptor) {
        try {
            Object methodName = api("MethodModel").getMethod("methodName").invoke(model);
            Object methodType = api("MethodModel").getMethod("methodType").invoke(model);
            var equalsString = api("constantpool.Utf8Entry").getMethod("equalsString", String.class);
            return (boolean) equalsString.invoke(methodName, name)
                && (boolean) equalsString.invoke(methodType, descriptor);
        } catch (ReflectiveOperationException error) {
            throw new IllegalStateException(error);
        }
    }

    private static byte[] method(byte[] bytes, String name, String descriptor, int kind) throws Exception {
        Class<?> formatType = api("ClassFile");
        Object format = formatType.getMethod("of").invoke(null);
        Object model = formatType.getMethod("parse", byte[].class).invoke(format, (Object) bytes);
        List<?> methods = (List<?>) api("ClassModel").getMethod("methods").invoke(model);
        if (methods.stream().filter(m -> matches(m, name, descriptor)).count() != 1) {
            throw new IllegalArgumentException("Rage firing method changed: " + name);
        }
        Class<?> builderType = api("CodeBuilder");
        var aload = builderType.getMethod("aload", int.class);
        var invoke = builderType.getMethod("invokestatic", ClassDesc.class, String.class, MethodTypeDesc.class);
        var with = api("ClassFileBuilder").getMethod("with", api("ClassFileElement"));
        Class<?> returns = api("instruction.ReturnInstruction");
        Class<?> calls = api("instruction.InvokeInstruction");
        boolean[] first = {true};
        int[] edits = {0};
        Object code = Proxy.newProxyInstance(RageFireTransforms.class.getClassLoader(),
            new Class<?>[]{api("CodeTransform")}, (proxy, called, arguments) -> {
                if (called.isDefault()) return InvocationHandler.invokeDefault(proxy, called, arguments);
                if (!called.getName().equals("accept")) throw new UnsupportedOperationException(called.toString());
                Object builder = arguments[0], element = arguments[1];
                if (first[0] && (kind == 1 || kind == 2)) {
                    aload.invoke(builder, 1);
                    if (kind == 2) aload.invoke(builder, 2);
                    invoke.invoke(builder, HELPER, kind == 1 ? "beginAttack" : "markShot",
                        MethodTypeDesc.ofDescriptor(kind == 1 ? "(Ljava/lang/Object;)V"
                            : "(Ljava/lang/Object;Ljava/lang/Object;)V"));
                    first[0] = false;
                    edits[0]++;
                }
                if (returns.isInstance(element)) {
                    String opcode = returns.getMethod("opcode").invoke(element).toString();
                    if (kind == 0 && opcode.equals("FRETURN")) {
                        aload.invoke(builder, 0);
                        aload.invoke(builder, 1);
                        aload.invoke(builder, 2);
                        invoke.invoke(builder, HELPER, "adjustMuzzle", MethodTypeDesc.ofDescriptor(
                            "(FLjava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)F"));
                        edits[0]++;
                    } else if (kind == 1 && opcode.equals("RETURN")) {
                        invoke.invoke(builder, HELPER, "endAttack", MethodTypeDesc.ofDescriptor("()V"));
                        edits[0]++;
                    } else if (kind == 3 && opcode.equals("RETURN")) {
                        aload.invoke(builder, 1);
                        invoke.invoke(builder, HELPER, "finishHitList", MethodTypeDesc.ofDescriptor("(Ljava/lang/Object;)V"));
                        edits[0]++;
                    }
                }
                with.invoke(builder, element);
                if (kind == 4 && calls.isInstance(element)) {
                    String owner = (String) api("constantpool.ClassEntry").getMethod("asInternalName")
                        .invoke(calls.getMethod("owner").invoke(element));
                    String calledName = calls.getMethod("name").invoke(element).toString();
                    String helper = null, type = null;
                    if (owner.equals("zombie/characters/IsoGameCharacter") && calledName.equals("getVariableString")) {
                        helper = "hitReaction"; type = "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/String;";
                    } else if (owner.equals("zombie/core/physics/BallisticsController") && calledName.equals("isCameraTarget")) {
                        helper = "cameraHit"; type = "(ZLjava/lang/Object;Ljava/lang/Object;)Z";
                    } else if (owner.equals("zombie/core/physics/BallisticsController") && calledName.equals("getCachedTargetedBodyPart")) {
                        helper = "bodyPart"; type = "(ILjava/lang/Object;Ljava/lang/Object;)I";
                    }
                    if (helper != null) {
                        aload.invoke(builder, 2);
                        aload.invoke(builder, 3);
                        invoke.invoke(builder, HELPER, helper, MethodTypeDesc.ofDescriptor(type));
                        edits[0]++;
                    }
                }
                return null;
            });
        Predicate<Object> predicate = m -> matches(m, name, descriptor);
        Object transform = api("ClassTransform").getMethod("transformingMethodBodies", Predicate.class,
            api("CodeTransform")).invoke(null, predicate, code);
        byte[] result = (byte[]) formatType.getMethod("transformClass", api("ClassModel"), api("ClassTransform"))
            .invoke(format, model, transform);
        if (edits[0] == 0 || (kind == 4 && edits[0] != 3) || !((List<?>) formatType.getMethod("verify", byte[].class)
            .invoke(format, (Object) result)).isEmpty()) throw new VerifyError("Invalid Rage firing patch: " + name);
        return result;
    }

    public static byte[] transform(byte[] bytes) throws Exception {
        Object format = api("ClassFile").getMethod("of").invoke(null);
        Object model = api("ClassFile").getMethod("parse", byte[].class).invoke(format, (Object) bytes);
        Object owner = api("ClassModel").getMethod("thisClass").invoke(model);
        String name = (String) api("constantpool.ClassEntry").getMethod("asInternalName").invoke(owner);
        if (name.equals("zombie/core/physics/BallisticsController")) {
            return method(bytes, "calculateMuzzlePosition", "(Lzombie/iso/Vector3;Lzombie/iso/Vector3;)F", 0);
        }
        if (name.equals("zombie/CombatManager")) {
            bytes = method(bytes, "attackCollisionCheck", "(Lzombie/characters/IsoGameCharacter;Lzombie/inventory/types/HandWeapon;Lzombie/ai/states/SwipeStatePlayer;Lzombie/AttackType;)V", 1);
            bytes = method(bytes, "calculateHitInfoList", "(Lzombie/characters/IsoGameCharacter;)V", 3);
            bytes = method(bytes, "processHit", "(Lzombie/inventory/types/HandWeapon;Lzombie/characters/IsoGameCharacter;Lzombie/characters/IsoGameCharacter;)I", 4);
            return method(bytes, "fireWeapon", "(Lzombie/inventory/types/HandWeapon;Lzombie/characters/IsoGameCharacter;)V", 2);
        }
        throw new IllegalArgumentException("Unsupported Rage firing class: " + name);
    }
}
