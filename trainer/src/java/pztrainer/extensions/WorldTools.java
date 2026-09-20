package pztrainer.extensions;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Set;
import zombie.characters.IsoPlayer;
import zombie.inventory.ItemContainer;
import zombie.inventory.ItemPickerJava;
import zombie.iso.IsoGridSquare;
import zombie.iso.IsoObject;
import zombie.iso.objects.IsoDeadBody;
import zombie.network.GameClient;

final class WorldTools {
    static String execute(IsoPlayer player, String action, int radius) {
        if (GameClient.client && (action.equals("containers_clear") || action.equals("containers_reroll")))
            return ContainerOperations.begin(player, radius, action.equals("containers_reroll"));
        if (!action.equals("corpses") && !action.equals("containers_clear") && !action.equals("containers_reroll"))
            throw new IllegalArgumentException("未知世界操作");
        int count = 0;
        Set<ItemContainer> seen = Collections.newSetFromMap(new IdentityHashMap<>());
        int px = (int)Math.floor(player.getX()), py = (int)Math.floor(player.getY()), pz = (int)Math.floor(player.getZ());
        for (int y = py - radius; y <= py + radius; y++) for (int x = px - radius; x <= px + radius; x++) {
            if ((x - px) * (x - px) + (y - py) * (y - py) > radius * radius) continue;
            IsoGridSquare square = player.getCell().getGridSquare(x, y, pz);
            if (square == null) continue;
            if (action.equals("corpses")) {
                for (var object : new ArrayList<>(square.getStaticMovingObjects())) {
                    if (object instanceof IsoDeadBody body && body.isZombie()) { square.removeCorpse(body, false); count++; }
                }
            } else if (action.equals("containers_clear") || action.equals("containers_reroll")) {
                for (IsoObject object : square.getObjects()) for (int i = 0; i < object.getContainerCount(); i++) {
                    ItemContainer container = object.getContainerByIndex(i);
                    if (container == null || !seen.add(container)) continue;
                    container.clear();
                    if (action.equals("containers_reroll")) {
                        container.setExplored(false);
                        ItemPickerJava.fillContainer(container, player);
                    }
                    container.setExplored(true);
                    container.setDrawDirty(true);
                    count++;
                }
            } else throw new IllegalArgumentException("未知世界操作");
        }
        return "已处理：" + count + "（当前楼层，半径 " + radius + " 格）";
    }

}
