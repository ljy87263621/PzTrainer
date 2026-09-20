package pztrainer.player;

import java.lang.classfile.*;
import java.lang.classfile.instruction.*;
import java.lang.constant.ClassDesc;
import java.lang.constant.MethodTypeDesc;

public final class PlayerTransforms {
    private static final ClassFile FORMAT = ClassFile.of(ClassFile.ClassHierarchyResolverOption.of(
        ClassHierarchyResolver.ofClassLoading(PlayerTransforms.class.getClassLoader())));
    private static final ClassDesc CARRY = ClassDesc.of("pztrainer.player.CarryOverrides");
    private static final ClassDesc GOD = ClassDesc.of("pztrainer.player.GodModeConnection");

    public static byte[] transform(byte[] bytes) {
        ClassModel model = FORMAT.parse(bytes);
        String owner = model.thisClass().asInternalName();
        String method, descriptor;
        switch (owner) {
            case "zombie/characters/IsoGameCharacter": method = "getMaxWeight"; descriptor = "()I"; break;
            case "zombie/inventory/ItemContainer": method = "getEffectiveCapacity"; descriptor = "(Lzombie/characters/IsoGameCharacter;)I"; break;
            case "zombie/network/packets/connection/ConnectPacket": method = "write"; descriptor = "(Lzombie/core/network/ByteBufferWriter;)V"; break;
            case "zombie/network/packets/connection/ConnectedPacket": method = "parse"; descriptor = "(Lzombie/core/network/ByteBufferReader;Lzombie/network/IConnection;)V"; break;
            case "zombie/network/packets/ExtraInfoPacket": method = "processClient"; descriptor = "(Lzombie/core/raknet/UdpConnection;)V"; break;
            default: throw new IllegalArgumentException("不支持的玩家接口：" + owner);
        }
        var matches = model.methods().stream().filter(m -> m.methodName().equalsString(method)
            && m.methodType().equalsString(descriptor) && m.code().isPresent()).toList();
        if (matches.size() != 1) throw new IllegalArgumentException("游戏玩家接口已变化：" + method);
        var target = matches.getFirst();
        if (target.code().orElseThrow().elementList().stream().anyMatch(e -> e instanceof InvokeInstruction call
            && (call.owner().asSymbol().equals(CARRY) || call.owner().asSymbol().equals(GOD)))) return bytes;
        int[] edits = {0};
        byte[] result = FORMAT.transformClass(model, ClassTransform.transformingMethodBodies(m -> m == target, (builder, element) -> {
            if (element instanceof ReturnInstruction ret && ret.opcode() == Opcode.IRETURN) {
                if (owner.equals("zombie/characters/IsoGameCharacter")) {
                    builder.aload(0).invokestatic(CARRY, "scaledWeight", MethodTypeDesc.ofDescriptor("(ILzombie/characters/IsoGameCharacter;)I")); edits[0]++;
                } else if (owner.equals("zombie/inventory/ItemContainer")) {
                    builder.aload(0).invokestatic(CARRY, "rootCapacity", MethodTypeDesc.ofDescriptor("(ILzombie/inventory/ItemContainer;)I")); edits[0]++;
                }
            }
            if (element instanceof ReturnInstruction ret && ret.opcode() == Opcode.RETURN
                && (method.equals("parse") || method.equals("processClient"))) {
                builder.invokestatic(GOD, "received", MethodTypeDesc.ofDescriptor("()V")); edits[0]++;
            }
            builder.with(element);
            if (method.equals("write") && element instanceof FieldInstruction field && field.opcode() == Opcode.GETFIELD
                && field.owner().asInternalName().equals(owner) && field.name().equalsString("extraInfoFlags")
                && field.typeSymbol().equals(java.lang.constant.ConstantDescs.CD_byte)) {
                builder.invokestatic(GOD, "connectFlags", MethodTypeDesc.ofDescriptor("(B)B")); edits[0]++;
            }
        }));
        if (edits[0] == 0 || (method.equals("write") && edits[0] != 1) || !FORMAT.verify(result).isEmpty())
            throw new VerifyError("玩家接口校验失败：" + method);
        return result;
    }
}
