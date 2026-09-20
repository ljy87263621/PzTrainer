package pztrainer.player;

import zombie.characters.IsoPlayer;
import zombie.network.GameClient;

// PienZ's godmode is a connection option, latched when ConnectPacket is written.
public final class GodModeConnection {
    private static volatile boolean requested, sent, acknowledged;
    private static Object connection;

    public static synchronized int configure(boolean enabled) {
        boolean locked = GameClient.client && GameClient.connection != null && GameClient.instance != null
            && GameClient.instance.playerConnectSent;
        if (connection != GameClient.connection || !GameClient.client) {
            connection = GameClient.connection; sent = false; acknowledged = false;
        }
        if (!locked && !sent) requested = enabled;
        return ((!locked && !sent) ? 1 : 0) | (requested ? 2 : 0);
    }

    public static synchronized byte connectFlags(byte original) {
        connection = GameClient.connection;
        acknowledged = false; sent = true;
        return flags(original, requested);
    }

    public static byte flags(byte original, boolean enabled) { return (byte)(enabled ? original | 1 : original); }

    public static void received() {
        if (!requested || !GameClient.client) return;
        IsoPlayer player = IsoPlayer.getInstance();
        if (player == null || !player.isLocalPlayer()) return;
        // Match PienZ: the server holds the connection flag; avoid resending it as a local toggle.
        if (player.isGodMod()) { acknowledged = true; player.setGodMod(false, true); }
    }

    public static String message() {
        if (GameClient.client && GameClient.instance != null && GameClient.instance.playerConnectSent)
            return requested ? (acknowledged ? "服务器已回传无敌标志；断开连接后可更改" : "已在连接时请求无敌，等待服务器确认；断开连接后可更改")
                : "请先断开服务器，在主菜单开启无敌后重新连接";
        return requested ? "无敌已准备，将在下次连接服务器时应用" : "联机无敌需在连接服务器前开启";
    }
}
