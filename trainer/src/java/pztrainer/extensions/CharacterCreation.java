package pztrainer.extensions;

import java.io.IOException;
import zombie.Lua.LuaManager;
import se.krka.kahlua.vm.KahluaTable;
import se.krka.kahlua.vm.LuaClosure;
import se.krka.kahlua.luaj.compiler.LuaCompiler;

final class CharacterCreation {
    private static KahluaTable environment;
    private static LuaClosure closure;

    static String execute(String action, String payload) {
        try {
            if (closure == null || environment != LuaManager.env) {
                try (var stream = CharacterCreation.class.getResourceAsStream("character_creation.lua")) {
                    if (stream == null) throw new IllegalStateException("角色创建资源缺失");
                    closure = LuaCompiler.loadis(stream, "PZSA character creation", LuaManager.env);
                    environment = LuaManager.env;
                }
            }
            boolean skills = action.equals("skill") || action.equals("skills_clear");
            var result = LuaManager.caller.protectedCall(LuaManager.thread, closure, new Object[]{skills ? "refresh" : action, payload});
            if (!result.isSuccess()) throw new IllegalStateException(result.getErrorString());
            if (action.equals("skill")) CreationSkills.set(payload, result.getSecond(), result.getThird());
            if (action.equals("skills_clear")) CreationSkills.clear();
            return String.valueOf(result.getFirst()) + CreationSkills.catalogue();
        } catch (IOException error) { throw new IllegalStateException("无法读取角色创建脚本", error); }
    }
}
