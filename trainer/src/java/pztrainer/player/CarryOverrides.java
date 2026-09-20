package pztrainer.player;

import java.util.concurrent.atomic.AtomicBoolean;
import zombie.MainThread;
import zombie.characters.IsoGameCharacter;
import zombie.characters.IsoPlayer;
import zombie.inventory.ItemContainer;
import zombie.network.GameClient;
import zombie.network.GameServer;

public final class CarryOverrides {
    private static final AtomicBoolean queued = new AtomicBoolean();
    private static volatile boolean requested;
    private static volatile float multiplier = 1.85f;
    private static volatile IsoPlayer target;
    private static volatile int state;
    private static volatile String message = "等待角色负重状态";
    private static IsoPlayer previous;
    private static Object connection;
    private static volatile int lastWeight = -1, baseWeight;
    private static long nextSync;
    private static boolean active;

    public static int scaledWeight(int original, IsoGameCharacter character) {
        if (!requested || character == null || character != target) return original;
        // PlayerDamage may echo our last synchronized value; do not multiply it again.
        if (original != lastWeight) baseWeight = original;
        return scale(baseWeight, multiplier);
    }

    public static int scale(int original, float factor) {
        return (int)Math.min(Integer.MAX_VALUE, Math.max(0, Math.floor(original * (double)factor)));
    }

    public static int rootCapacity(int original, ItemContainer inventory) {
        IsoPlayer player = target;
        return requested && player != null && player.isLocalPlayer() && !player.isDead()
            && inventory == player.getInventory() ? Integer.MAX_VALUE : original;
    }

    public static int configure(boolean enabled, float factor) {
        requested = enabled;
        multiplier = Float.isFinite(factor) ? Math.clamp(factor, 1, 100) : 1.85f;
        if (!MainThread.isRunning()) { target = null; state = 0; message = "等待游戏主线程"; return state; }
        if (queued.compareAndSet(false, true)) {
            try { MainThread.queueInvokeOnMainThread(CarryOverrides::update); }
            catch (RuntimeException error) { queued.set(false); state = 0; message = "无法提交负重更新"; }
        }
        return state;
    }

    private static void update() {
        try {
            IsoPlayer player = IsoPlayer.getInstance();
            boolean online = GameClient.client;
            String unavailable = "";
            if (GameServer.server) unavailable = "当前为服务器进程";
            else if (player == null) unavailable = "等待角色进入游戏";
            else if (player.isDead()) unavailable = "角色已死亡";
            else if (!player.isLocalPlayer()) unavailable = "等待本机可操作角色";
            // UdpConnection.fullyConnected is set on the server, not the local client.
            else if (online && (GameClient.instance == null || !GameClient.instance.connected || GameClient.connection == null))
                unavailable = "等待客户端连接就绪";
            if (!unavailable.isEmpty()) {
                target = null; previous = null; active = false; state = online ? 8 : 0;
                message = unavailable; return;
            }
            if (previous != player || connection != GameClient.connection) {
                previous = player; connection = GameClient.connection; active = false; nextSync = 0; lastWeight = -1;
            }
            if (active && !requested) player.setMaxWeight(baseWeight);
            target = requested ? player : null;
            int weight = player.getMaxWeight();
            long now = System.nanoTime();
            if (online && ((requested && (weight != lastWeight || now >= nextSync)) || (active && !requested))) {
                GameClient.sendPlayerDamage(player);
                lastWeight = weight; nextSync = now + 1_000_000_000L;
            }
            active = requested;
            state = 1 | 4 | (online ? 8 : 0) | (requested ? 2 : 0);
            message = requested ? "人物负重倍率已应用，主背包容量限制已解除；联机同步以服务器结果为准" : "负重与背包容量已恢复原版";
        } catch (RuntimeException | LinkageError error) {
            target = null; state = 0; message = "负重更新失败：" + error.getMessage();
        } finally { queued.set(false); }
    }

    public static String message() { return message; }
}
