import java.nio.file.Files;
import java.nio.file.Path;
import java.util.zip.ZipFile;
import pztrainer.player.aim.RageFireTransforms;

public class RageFireTransformsTest {
    public static void main(String[] args) throws Exception {
        try (ZipFile game = new ZipFile(args[0])) {
            for (String name : new String[]{"zombie/core/physics/BallisticsController", "zombie/CombatManager"}) {
                byte[] original = game.getInputStream(game.getEntry(name + ".class")).readAllBytes();
                byte[] patched = RageFireTransforms.transform(original);
                if (java.util.Arrays.equals(original, patched)) throw new AssertionError("No firing transformation: " + name);
                Path output = Path.of(args[1], name + ".class");
                Files.createDirectories(output.getParent());
                Files.write(output, patched);
                System.out.println("Verified actual game firing bytecode: " + name);
            }
        }
    }
}
