package pztrainer.extensions;

// Shared capability keys are also consumed by the native menu. Empty reason means available.
final class ExtensionAccess {
    static final String[] ACTIONS = {
        "buckets", "vehicle_start", "vehicle_repair", "vehicle_fuel", "vehicle_teleport",
        "corpses", "containers_clear", "containers_reroll",
        "creator_refresh", "creator_spawn", "creator_world", "creator_profession", "creator_trait_add",
        "creator_trait_remove", "creator_points", "creator_skill", "creator_skills_clear", "creator_name",
        "creator_gender", "creator_hair", "creator_beard", "creator_voice", "creator_voice_pitch",
        "creator_clothing", "creator_clothing_texture", "creator_skin", "creator_hair_color",
        "creator_save_profession", "creator_load_profession", "creator_save_appearance", "creator_load_appearance"
    };

    record State(boolean online, boolean connected, boolean playerReady, boolean bodyPermission,
                 boolean invisiblePermission, boolean driver, boolean localPhysics) {
        String reason(String key) {
            boolean known = false;
            for (int flag = 1; flag <= 1024; flag <<= 1) if (key.equals("flag:" + flag)) { known = true; break; }
            if (!known) for (String action : ACTIONS) if (action.equals(key)) { known = true; break; }
            if (!known) return "未知功能";
            if (key.startsWith("creator_")) {
                if (key.equals("creator_world") && online) return "联机世界名称与种子由服务器决定";
                return "";
            }
            if (!connected) return "等待游戏会话连接";
            if (!playerReady) return "等待可操作的存活角色";
            if (online && key.equals("flag:32")) return "该全局开关仅影响本地模拟，联机时不可用；可使用下方的僵尸 AI 控制";
            if (online && key.equals("flag:16") && !bodyPermission) return "服务器未授予修改身体状态权限";
            if (online && key.equals("flag:256") && !invisiblePermission) return "服务器未授予自身隐身权限";
            if (online && key.equals("vehicle_start") && !driver) return "联机启动引擎需要坐在驾驶位";
            if (key.equals("flag:1024") && !localPhysics) return "需要驾驶引擎已启动、由本机模拟且没有拖挂的车辆";
            return "";
        }

        int filter(int requested) {
            int allowed = 0;
            for (int flag = 1; flag <= 1024; flag <<= 1)
                if ((requested & flag) != 0 && reason("flag:" + flag).isEmpty()) allowed |= flag;
            return allowed;
        }

        String snapshot() {
            StringBuilder result = new StringBuilder();
            for (int flag = 1; flag <= 1024; flag <<= 1)
                result.append("flag:").append(flag).append('\t').append(reason("flag:" + flag)).append('\n');
            for (String action : ACTIONS) result.append(action).append('\t').append(reason(action)).append('\n');
            return result.toString();
        }
    }
}
