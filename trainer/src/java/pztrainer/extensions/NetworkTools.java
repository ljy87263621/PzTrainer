package pztrainer.extensions;

import zombie.characters.IsoPlayer;
import zombie.network.GameClient;
import zombie.network.PacketTypes.PacketType;
import zombie.network.packets.INetworkPacket;

final class NetworkTools {
    static boolean connected() {
        return GameClient.instance != null && GameClient.instance.connected && GameClient.connection != null;
    }

    static void send(PacketType type, Object... arguments) {
        if (!connected()) throw new IllegalStateException("联机连接已断开");
        INetworkPacket.send(type, arguments);
    }

    static void command(IsoPlayer player, String module, String action, Object... values) {
        if (!connected()) throw new IllegalStateException("联机连接已断开");
        GameClient.instance.sendClientCommandV(player, module, action, values);
    }
}
