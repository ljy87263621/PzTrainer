package pztrainer.extensions;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import zombie.MainThread;
import zombie.characters.IsoPlayer;
import zombie.characters.Capability;
import zombie.characters.Role;
import zombie.network.GameClient;
import zombie.network.GameServer;
import zombie.Lua.LuaManager;

public final class ExtensionRuntime {
    private static final AtomicBoolean queued = new AtomicBoolean();
    private static final AtomicReference<Command> pending = new AtomicReference<>();
    private static final InventoryTools inventory = new InventoryTools();
    private static final ZombieTools zombies = new ZombieTools();
    private static volatile int flags, range = 10;
    private static volatile String status = "等待角色或角色创建界面";
    private static volatile String access = "";
    private static volatile String session = "";
    private static volatile String catalogue = "";
    private static IsoPlayer previousPlayer;
    private static Object previousConnection;
    private static volatile boolean blocked;
    private static int previousFlags;
    private static long nextMaintenance;
    private record Command(String action, String payload, int radius, IsoPlayer player, Object environment, Object connection) {}

    private ExtensionRuntime() {}

    public static boolean command(String action, String payload, int radius) {
        if (action == null || payload == null || payload.length() > 16384) return false;
        return pending.compareAndSet(null, new Command(action, payload, Math.clamp(radius, 1, 15), IsoPlayer.getInstance(), LuaManager.env, GameClient.connection));
    }

    public static String snapshot() { return access + "\u001d" + session + "\n" + status + "\n" + VehiclePhysics.status() + "\u001e" + catalogue; }

    public static void update(int requestedFlags, int requestedRange, boolean controlsBlocked) {
        flags = requestedFlags & 2047;
        blocked = controlsBlocked;
        range = Math.clamp(requestedRange, 1, 30);
        if (!MainThread.isRunning()) { access = ""; status = "游戏主线程尚未就绪"; return; }
        if (!queued.compareAndSet(false, true)) return;
        try { MainThread.queueInvokeOnMainThread(ExtensionRuntime::run); }
        catch (RuntimeException | LinkageError error) { queued.set(false); status = "提交失败：" + error; }
    }

    private static void run() {
        try {
            Command command = pending.getAndSet(null);
            IsoPlayer player = IsoPlayer.getInstance();
            boolean online = GameClient.client;
            boolean connected = !GameServer.server && (!online || NetworkTools.connected());
            boolean ready = connected && player != null && !player.isDead() && player.getCell() != null;
            var vehicle = ready ? player.getVehicle() : null;
            boolean driver = vehicle != null && vehicle.isDriver(player);
            var permissions = new ExtensionAccess.State(online, connected, ready,
                ready && Role.hasCapability(player, Capability.CanModifyBodyStats),
                ready && Role.hasCapability(player, Capability.ToggleInvisibleHimself), driver,
                driver && vehicle.isEngineRunning() && vehicle.isLocalPhysicSim() &&
                    vehicle.getVehicleTowing() == null && vehicle.getVehicleTowedBy() == null);
            access = permissions.snapshot();
            int effectiveFlags = permissions.filter(flags);
            VehiclePhysics.request((effectiveFlags & 1024) != 0, blocked);
            session = online ? "会话：联机；修改请求以服务器结果为准" : ready ? "会话：角色已就绪" : "会话：等待角色，可在创建界面编辑";
            if (ready && status.equals("等待角色或角色创建界面")) status = "功能已就绪";
            CreationSkills.maintain(ready);
            if (previousPlayer != player || previousConnection != GameClient.connection || !ready) {
                inventory.restore(); zombies.restore(); ContainerOperations.cancel();
                previousPlayer = ready ? player : null; previousConnection = GameClient.connection; previousFlags = -1;
            }
            if (command != null) {
                String reason = permissions.reason(command.action);
                if (!reason.isEmpty()) throw new IllegalStateException(reason);
                if (command.player != player || command.environment != LuaManager.env || command.connection != GameClient.connection)
                    throw new IllegalStateException("会话已改变，操作已取消");
                if (command.action.startsWith("creator_")) {
                    catalogue = CharacterCreation.execute(command.action.substring(8), command.payload);
                    status = "角色创建面板已更新";
                } else {
                    if (!ready) throw new IllegalStateException("请先进入存档");
                    if (command.action.startsWith("vehicle_")) status = VehicleTools.execute(player, command.action);
                    else if (command.action.equals("buckets")) status = "已补水的桶：" + InventoryTools.visit(player.getInventory(), true);
                    else status = WorldTools.execute(player, command.action, command.radius);
                }
            }
            if (ready) {
                int currentFlags = effectiveFlags;
                long now = System.nanoTime();
                if (currentFlags != previousFlags || now >= nextMaintenance) {
                    inventory.update(player, currentFlags);
                    zombies.update(player, currentFlags, range);
                    VehicleTools.update(player, currentFlags);
                    previousFlags = currentFlags;
                    nextMaintenance = now + 250_000_000L;
                }
                String progress = ContainerOperations.update(player);
                if (progress != null) status = progress;
            }
        } catch (RuntimeException | LinkageError error) {
            status = "操作失败：" + error.getMessage();
        } finally { queued.set(false); }
    }
}
