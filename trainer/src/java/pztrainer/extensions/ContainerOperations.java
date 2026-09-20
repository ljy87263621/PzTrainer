package pztrainer.extensions;

import java.util.ArrayDeque;
import java.util.ArrayList;
import zombie.characters.IsoPlayer;
import zombie.inventory.InventoryItem;
import zombie.inventory.ItemContainer;
import zombie.iso.IsoGridSquare;
import zombie.iso.IsoObject;
import zombie.network.GameClient;
import zombie.network.PacketTypes.PacketType;
import zombie.network.packets.RemoveInventoryItemFromContainerPacket;

// Uses the same remove / clear-explored / request-items sequence as PienZ ContainerJobs.
final class ContainerOperations {
    private record Target(IsoGridSquare square, IsoObject object, ItemContainer container) {}
    private static final ArrayDeque<Target> targets = new ArrayDeque<>();
    private static IsoPlayer owner;
    private static Object connection;
    private static boolean reroll;
    private static int completed, skipped;
    private static long nextStep;

    static void cancel() { targets.clear(); owner = null; connection = null; }

    static String begin(IsoPlayer player, int radius, boolean refill) {
        if (owner != null) throw new IllegalStateException("请等待当前容器操作完成");
        owner = player; connection = GameClient.connection; reroll = refill;
        completed = skipped = 0; nextStep = 0;
        int px = (int)Math.floor(player.getX()), py = (int)Math.floor(player.getY()), z = (int)Math.floor(player.getZ());
        for (int y = py - radius; y <= py + radius; y++) for (int x = px - radius; x <= px + radius; x++) {
            if ((x - px) * (x - px) + (y - py) * (y - py) > radius * radius) continue;
            var square = player.getCell().getGridSquare(x, y, z);
            if (square == null) continue;
            for (var object : square.getObjects()) for (int i = 0; i < object.getContainerCount(); i++) {
                var container = object.getContainerByIndex(i);
                if (container != null) targets.add(new Target(square, object, container));
            }
        }
        return "容器操作已排队：" + targets.size();
    }

    static String update(IsoPlayer player) {
        if (owner == null) return null;
        if (player != owner || !NetworkTools.connected() || GameClient.connection != connection) {
            cancel(); return "会话已变化，容器操作已取消";
        }
        long now = System.nanoTime();
        if (now < nextStep) return null;
        nextStep = now + 100_000_000L;
        try {
            for (int n = 0; n < 4 && !targets.isEmpty(); n++) {
                var target = targets.peek();
                var square = target.square;
                int index = target.object.getObjectIndex();
                int slot = target.object.getContainerIndex(target.container);
                if (player.getCell().getGridSquare(square.x, square.y, square.z) != square ||
                    (int)Math.floor(player.getZ()) != square.z || index < 0 || slot < 0 ||
                    target.object.getSquare() != square || target.container.getParent() != target.object ||
                    target.object.getContainerByIndex(slot) != target.container ||
                    (reroll && (square.getRoom() == null || square.getRoom().getRoomDef() == null))) {
                    targets.remove(); skipped++; continue;
                }
                var items = target.container.getItems();
                if (!items.isEmpty()) {
                    var batch = new ArrayList<InventoryItem>(items.subList(0, Math.min(128, items.size())));
                    var packet = new RemoveInventoryItemFromContainerPacket();
                    packet.setData(target.container, batch);
                    packet.sendToServer(PacketType.RemoveInventoryItemFromContainer);
                    packet.processClient(null);
                    if (!items.isEmpty()) continue;
                }
                if (reroll) {
                    NetworkTools.command(player, "object", "clearContainerExplore", "x", (double)square.x,
                        "y", (double)square.y, "z", (double)square.z, "index", (double)index, "containerIndex", (double)slot);
                    target.container.setExplored(false);
                    NetworkTools.send(PacketType.RequestItemsForContainer, target.container);
                }
                targets.remove(); completed++;
            }
            String result = "容器请求已提交：" + completed + "；跳过：" + skipped + "；剩余：" + targets.size();
            if (targets.isEmpty()) { cancel(); result += "；最终结果以服务器同步为准"; }
            return result;
        } catch (RuntimeException | LinkageError error) { cancel(); throw error; }
    }
}
