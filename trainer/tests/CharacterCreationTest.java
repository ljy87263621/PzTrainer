import java.nio.file.Files;
import java.nio.file.Path;
import se.krka.kahlua.j2se.J2SEPlatform;
import se.krka.kahlua.vm.KahluaThread;
import se.krka.kahlua.vm.KahluaTable;
import se.krka.kahlua.luaj.compiler.LuaCompiler;
import pztrainer.lua.Utf8ByteReader;

public final class CharacterCreationTest {
    public static void main(String[] args) throws Exception {
        var platform = J2SEPlatform.getInstance();
        KahluaTable env = platform.newEnvironment();
        var thread = new KahluaThread(platform, env);
        thread.debugOwnerThread = Thread.currentThread();
        try (var stream = Files.newInputStream(Path.of(args[0]))) {
            env.rawset("editor", LuaCompiler.loadis(stream, "character_creation", env));
        }
        String tests = Files.readString(Path.of(args[1]));
        Object[] result = thread.pcall(LuaCompiler.loadis(new Utf8ByteReader(tests), "creation tests", env), new Object[0]);
        if (!Boolean.TRUE.equals(result[0])) throw new AssertionError(java.util.Arrays.toString(result));
        System.out.println(result[1]);
    }
}
