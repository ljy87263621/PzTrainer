package pztrainer.extensions;

import zombie.characters.IsoPlayer;
import zombie.vehicles.BaseVehicle;
import zombie.network.GameClient;

final class VehicleTools {
    private static long nextStart;
    static BaseVehicle target(IsoPlayer player) {
        if (player.getVehicle() != null) return player.getVehicle();
        BaseVehicle nearest = null;
        double best = 400;
        for (BaseVehicle vehicle : player.getCell().getVehicles()) {
            double dx = vehicle.getX() - player.getX(), dy = vehicle.getY() - player.getY();
            double distance = dx * dx + dy * dy;
            if (Math.abs(vehicle.getZ() - player.getZ()) < 1 && distance < best) { best = distance; nearest = vehicle; }
        }
        if (nearest == null) throw new IllegalStateException("20 格内没有已加载车辆");
        return nearest;
    }

    static String execute(IsoPlayer player, String action) {
        BaseVehicle vehicle = target(player);
        if (GameClient.client && !action.equals("vehicle_teleport")) return executeOnline(player, vehicle, action);
        switch (action) {
            case "vehicle_repair": vehicle.repair(); return "车辆已修复";
            case "vehicle_fuel": {
                int count = 0;
                var parts = vehicle.getParts();
                for (int i = 0; i < parts.size(); i++) {
                    var part = parts.get(i);
                    if ("Gasoline".equals(part.getContainerContentType())) {
                        part.setContainerContentAmount(part.getContainerCapacity()); count++;
                    }
                }
                return "已加满汽油容器：" + count;
            }
            case "vehicle_start": vehicle.engineDoRunning(); return "已启动目标车辆引擎";
            case "vehicle_teleport": {
                if (player.getVehicle() != null) throw new IllegalStateException("请先下车");
                var cell = player.getCell();
                for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
                    var square = cell.getGridSquare((int)vehicle.getX() + dx, (int)vehicle.getY() + dy, (int)vehicle.getZ());
                    if (square != null && square.isFree(false)) {
                        player.setX(square.getX() + 0.5f); player.setY(square.getY() + 0.5f); player.setZ(square.getZ());
                        player.setLastX(player.getX()); player.setLastY(player.getY()); player.setLastZ(player.getZ());
                        player.setCurrentSquareFromPosition();
                        if (GameClient.client) GameClient.instance.sendPlayer(player);
                        return "已传送至车辆旁空地";
                    }
                }
                throw new IllegalStateException("车辆旁没有可站立的已加载格子");
            }
            default: throw new IllegalArgumentException("未知车辆操作");
        }
    }

    static void update(IsoPlayer player, int flags) {
        if ((flags & 512) == 0 || player.getVehicle() == null || !player.getVehicle().isDriver(player) || player.getVehicle().isEngineRunning()) return;
        if (GameClient.client) {
            long now = System.nanoTime();
            if (now < nextStart) return;
            nextStart = now + 1_000_000_000L;
            NetworkTools.command(player, "vehicle", "startEngine", "haveKey", true);
        } else player.getVehicle().engineDoRunning();
    }

    private static String executeOnline(IsoPlayer player, BaseVehicle vehicle, String action) {
        if (action.equals("vehicle_start")) {
            if (!vehicle.isDriver(player)) throw new IllegalStateException("请先坐到驾驶位");
            NetworkTools.command(player, "vehicle", "startEngine", "haveKey", true);
            return "已请求服务器启动引擎";
        }
        int count = 0;
        var parts = vehicle.getParts();
        for (int i = 0; i < parts.size(); i++) {
            var part = parts.get(i);
            if (part.isInventoryItemUninstalled()) continue;
            if (action.equals("vehicle_repair")) {
                if (part.getInventoryItem() != null && part.getCondition() < 100) {
                    NetworkTools.command(player, "vehicle", "fixPart", "vehicle", (double)vehicle.getId(),
                        "part", part.getId(), "condition", 100.0, "haveBeenRepaired", 0.0);
                    count++;
                }
                if (part.getWheelIndex() >= 0 && part.getContainerContentAmount() < part.getContainerCapacity()) {
                    NetworkTools.command(player, "vehicle", "setTirePressure", "vehicle", (double)vehicle.getId(),
                        "part", part.getId(), "psi", (double)part.getContainerCapacity());
                    count++;
                }
            } else if (action.equals("vehicle_fuel") && "Gasoline".equals(part.getContainerContentType())) {
                NetworkTools.command(player, "vehicle", "setContainerContentAmount", "vehicle", (double)vehicle.getId(),
                    "part", part.getId(), "amount", (double)part.getContainerCapacity());
                count++;
            }
        }
        return "已提交车辆部件请求：" + count + "；等待服务器同步（不会补装缺失部件）";
    }
}
