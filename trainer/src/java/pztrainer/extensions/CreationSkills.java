package pztrainer.extensions;

import java.util.LinkedHashMap;
import zombie.Lua.LuaManager;
import zombie.characters.SurvivorDesc;
import zombie.characters.skills.PerkFactory;
import se.krka.kahlua.vm.JavaFunction;
import se.krka.kahlua.vm.KahluaTable;

final class CreationSkills {
    private static SurvivorDesc descriptor;
    private static KahluaTable target, environment;
    private static Object original, ownOriginal;
    private static JavaFunction callback;
    private static final LinkedHashMap<PerkFactory.Perk, Integer> levels = new LinkedHashMap<>();

    private static KahluaTable table(Object value) {
        if (!(value instanceof KahluaTable result)) throw new IllegalStateException("角色创建表不可用");
        return result;
    }

    static void clear() {
        if (target != null && target.rawget("initPlayer") == callback) target.rawset("initPlayer", ownOriginal);
        target = null; descriptor = null; environment = null; original = null; ownOriginal = null; callback = null;
        levels.clear();
    }

    static void maintain(boolean inGame) {
        if (target == null) return;
        var mainClass = LuaManager.env == null ? null : LuaManager.env.rawget("MainScreen");
        var instance = mainClass instanceof KahluaTable main ? main.rawget("instance") : null;
        var current = instance instanceof KahluaTable main ? main.rawget("desc") : null;
        if (inGame || environment != LuaManager.env || current != descriptor) clear();
    }

    static void set(String payload, Object appearance, Object currentDescriptor) {
        String[] parts = payload.split("\t", -1);
        if (parts.length != 2) throw new IllegalArgumentException("技能参数无效");
        int level = Integer.parseInt(parts[1]);
        if (level < 0 || level > 10) throw new IllegalArgumentException("出生技能等级必须为 0–10");
        PerkFactory.Perk perk = null;
        for (var item : PerkFactory.PerkList) if (item.getId().equals(parts[0])) { perk = item; break; }
        if (perk == null) throw new IllegalArgumentException("技能已不存在");
        if (!(currentDescriptor instanceof SurvivorDesc current)) throw new IllegalStateException("角色描述对象不可用");
        if (descriptor != null && descriptor != current) clear();
        if (target == null) {
            target = table(appearance);
            descriptor = current;
            environment = LuaManager.env;
            ownOriginal = target.rawget("initPlayer");
            original = ownOriginal == null ? table(LuaManager.env.rawget("CharacterCreationMain")).rawget("initPlayer") : ownOriginal;
            if (original == null) { clear(); throw new IllegalStateException("原版角色初始化接口不可用"); }
            final Object delegate = original;
            final SurvivorDesc actor = descriptor;
            callback = (frame, count) -> {
                Object[] arguments = new Object[count];
                for (int i = 0; i < count; i++) arguments[i] = frame.get(i);
                var result = LuaManager.caller.protectedCall(LuaManager.thread, delegate, arguments);
                if (!result.isSuccess()) throw new IllegalStateException(result.getErrorString());
                if (descriptor == actor && environment == LuaManager.env) actor.getXPBoostMap().putAll(levels);
                for (int i = 0; i < result.size(); i++) frame.push(result.get(i));
                return result.size();
            };
            target.rawset("initPlayer", callback);
        }
        levels.put(perk, level);
    }

    static String catalogue() {
        StringBuilder result = new StringBuilder();
        for (var perk : PerkFactory.PerkList) {
            if (perk.getParent() == null || "None".equals(perk.getParent().getId())) continue;
            String label = perk.getName();
            if (levels.containsKey(perk)) label += " [" + levels.get(perk) + "]";
            result.append("\nskill\t").append(perk.getId()).append('\t').append(label.replace('\t', ' ').replace('\n', ' '));
        }
        return result.toString();
    }
}
