import java.lang.classfile.ClassFile;
import java.lang.classfile.instruction.InvokeInstruction;
import java.util.Arrays;
import java.util.jar.JarFile;
import pztrainer.player.CarryOverrides;
import pztrainer.player.GodModeConnection;
import pztrainer.player.PlayerTransforms;

public final class PlayerOverridesTest {
    private static int checks;
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
        checks++;
    }
    public static void main(String[] args) throws Exception {
        check(CarryOverrides.scale(8, 95.9f) == 767, "multiplier changes the actual capacity");
        check(CarryOverrides.scale(Integer.MAX_VALUE, 100) == Integer.MAX_VALUE, "no integer overflow");
        check(CarryOverrides.scaledWeight(8, null) == 8, "inactive carry preserves original values");
        check(CarryOverrides.rootCapacity(50, null) == 50, "inactive containers remain unchanged");
        for (int flag = 0; flag < 256; flag++) {
            check((GodModeConnection.flags((byte)flag, true) & 255) == (flag | 1), "only godmode bit is changed");
            check(GodModeConnection.flags((byte)flag, false) == (byte)flag, "original connection flags preserved");
        }
        try (JarFile jar = new JarFile(args[0])) {
            for (String name : new String[]{"zombie/characters/IsoGameCharacter", "zombie/inventory/ItemContainer",
                "zombie/network/packets/connection/ConnectPacket", "zombie/network/packets/connection/ConnectedPacket",
                "zombie/network/packets/ExtraInfoPacket"}) {
                byte[] before;
                try (var stream = jar.getInputStream(jar.getJarEntry(name + ".class"))) { before = stream.readAllBytes(); }
                byte[] after = PlayerTransforms.transform(before);
                check(!Arrays.equals(before, after), "real game class transformed: " + name);
                check(ClassFile.of().verify(after).isEmpty(), "JVM bytecode validation: " + name);
                check(Arrays.equals(after, PlayerTransforms.transform(after)), "transform is idempotent: " + name);
                long hooks = ClassFile.of().parse(after).methods().stream().flatMap(m -> m.code().stream())
                    .flatMap(c -> c.elementList().stream()).filter(e -> e instanceof InvokeInstruction call
                        && call.owner().asInternalName().startsWith("pztrainer/player/")).count();
                check(hooks > 0, "hook inserted into actual method: " + name);
            }
        }
        System.out.println("Player overrides: " + checks + " checks passed against actual game classes.");
    }
}
