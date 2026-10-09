package pztrainer.items;

import zombie.characters.IsoPlayer;
import zombie.core.network.ByteBufferWriter;
import zombie.inventory.types.Radio;
import zombie.network.GameClient;
import zombie.network.PacketTypes.PacketType;
import zombie.network.packets.NetTimedActionPacket;

public final class MediaItemPayload {
    private static final short PAYLOAD_MEDIA_INDEX = Short.MAX_VALUE;

    private MediaItemPayload() {}

    public static Radio findEquippedRadio(IsoPlayer player) {
        if (player == null) return null;
        if (player.getPrimaryHandItem() instanceof Radio radio) return radio;
        if (player.getSecondaryHandItem() instanceof Radio radio) return radio;
        return null;
    }

    public static boolean sendState(IsoPlayer player, Radio radio, String fullType) {
        if (!isUsable(player, radio) || fullType == null || fullType.isEmpty() ||
                GameClient.connection == null) return false;
        var device = radio.getDeviceData();
        if (device.hasMedia() && device.getMediaIndex() != PAYLOAD_MEDIA_INDEX) return false;

        device.setMediaIndex(PAYLOAD_MEDIA_INDEX);

        byte hand = player.getPrimaryHandItem() == radio ? (byte)1 : (byte)2;
        var connection = GameClient.connection;
        var type = PacketType.RadioDeviceDataState;
        ByteBufferWriter writer = connection.startPacket();
        try {
            type.doPacket(writer);
            writer.putByte((byte)0);
            writer.putByte(player.playerIndex);
            writer.putByte(hand);
            writer.putShort((short)7);
            writer.putShort(PAYLOAD_MEDIA_INDEX);
            writer.putBoolean(true);
            writer.putUTF(fullType);
            if (connection.isLimitExceeded(type)) {
                connection.cancelPacket();
                device.setMediaIndex((short)-1);
                return false;
            }
            connection.endPacket(
                type.packetPriority, type.packetReliability, type.orderingChannel);
            return true;
        } catch (RuntimeException | LinkageError error) {
            connection.cancelPacket();
            device.setMediaIndex((short)-1);
            throw error;
        }
    }

    public static boolean sendRemove(IsoPlayer player, Radio radio) {
        if (!isUsable(player, radio)) return false;
        NetTimedActionPacket.createNewAndSend(
            "ISDeviceMediaAction", player,
            player, true, null, radio.getID());
        return true;
    }

    public static int countInventory(IsoPlayer player, String fullType) {
        if (player == null || player.getInventory() == null || fullType == null) return -1;
        return player.getInventory().getItemCount(fullType);
    }

    private static boolean isUsable(IsoPlayer player, Radio radio) {
        return player != null && radio != null && radio.getDeviceData() != null &&
            (player.getPrimaryHandItem() == radio ||
             player.getSecondaryHandItem() == radio);
    }

}
