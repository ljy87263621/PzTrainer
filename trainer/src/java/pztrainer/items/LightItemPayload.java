package pztrainer.items;

import zombie.characters.IsoPlayer;
import zombie.iso.IsoGridSquare;
import zombie.iso.IsoObject;
import zombie.iso.objects.IsoLightSwitch;
import zombie.network.packets.NetTimedActionPacket;

public final class LightItemPayload {
    private static final int SEARCH_RADIUS = 2;

    private LightItemPayload() {}

    public static IsoLightSwitch findEmptyCarrier(IsoPlayer player) {
        if (player == null || player.getCell() == null) return null;
        int px = (int)Math.floor(player.getX());
        int py = (int)Math.floor(player.getY());
        int pz = (int)Math.floor(player.getZ());
        for (int y = py - SEARCH_RADIUS; y <= py + SEARCH_RADIUS; y++) {
            for (int x = px - SEARCH_RADIUS; x <= px + SEARCH_RADIUS; x++) {
                int dx = x - px;
                int dy = y - py;
                if (dx * dx + dy * dy > SEARCH_RADIUS * SEARCH_RADIUS) continue;
                IsoGridSquare square = player.getCell().getGridSquare(x, y, pz);
                if (square == null) continue;
                for (IsoObject object : square.getObjects()) {
                    if (object instanceof IsoLightSwitch light &&
                            light.getObjectIndex() != -1 &&
                            light.getCanBeModified() && !light.getLights().isEmpty() &&
                            !light.hasLightBulb()) {
                        return light;
                    }
                }
            }
        }
        return null;
    }

    public static boolean sendState(
            IsoPlayer player, IsoLightSwitch light, String fullType) {
        if (!isUsable(player, light) || fullType == null || fullType.isEmpty() ||
                light.hasLightBulb()) return false;
        light.setBulbItemRaw(fullType);
        light.syncCustomizedSettings(null);
        return true;
    }

    public static boolean sendRemove(IsoPlayer player, IsoLightSwitch light) {
        if (!isUsable(player, light) || !light.hasLightBulb()) return false;
        NetTimedActionPacket.createNewAndSend(
            "ISLightActions", player,
            "RemoveLightBulb", player, light, null);
        return true;
    }

    public static int countInventory(IsoPlayer player, String fullType) {
        if (player == null || player.getInventory() == null || fullType == null) return -1;
        return player.getInventory().getItemCount(fullType);
    }

    private static boolean isUsable(IsoPlayer player, IsoLightSwitch light) {
        if (player == null || light == null || light.getSquare() == null ||
                light.getObjectIndex() == -1 || !light.getCanBeModified() ||
                light.getLights().isEmpty()) return false;
        int dx = light.getSquare().getX() - (int)Math.floor(player.getX());
        int dy = light.getSquare().getY() - (int)Math.floor(player.getY());
        int dz = light.getSquare().getZ() - (int)Math.floor(player.getZ());
        return dz == 0 && dx * dx + dy * dy <= SEARCH_RADIUS * SEARCH_RADIUS;
    }
}
