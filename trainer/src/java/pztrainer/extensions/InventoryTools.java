package pztrainer.extensions;

import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Set;
import zombie.characters.IsoPlayer;
import zombie.inventory.InventoryItem;
import zombie.inventory.ItemContainer;
import zombie.inventory.types.Clothing;
import zombie.inventory.types.HandWeapon;
import zombie.inventory.types.InventoryContainer;
import zombie.entity.components.fluids.Fluid;
import zombie.scripting.objects.ItemTag;
import zombie.network.GameClient;
import zombie.network.PacketTypes.PacketType;

final class InventoryTools {
    private HandWeapon weapon;
    private float critical, multiplier, extra, minimum;
    private boolean criticalApplied, minimumApplied;

    void update(IsoPlayer player, int flags) {
        HandWeapon current = player.getPrimaryHandItem() instanceof HandWeapon value
            && !value.isBareHands() ? value : null;
        if (weapon != current) { restore(); weapon = current; }
        if (weapon != null) {
            boolean crit = (flags & 1) != 0;
            if (crit && !criticalApplied) {
                critical = weapon.getCriticalChance();
                multiplier = weapon.getCriticalDamageMultiplier();
                extra = weapon.getExtraDamage();
                criticalApplied = true;
            }
            if (criticalApplied) {
                weapon.setCriticalChance(crit ? 100 : critical);
                weapon.setCriticalDamageMultiplier(crit ? 100 : multiplier);
                weapon.setExtraDamage(crit ? 50 : extra);
                if (!crit) criticalApplied = false;
            }
            boolean close = (flags & 2) != 0 && !weapon.isRanged();
            if (close && !minimumApplied) { minimum = weapon.getMinRange(); minimumApplied = true; }
            if (minimumApplied) {
                weapon.setMinRange(close ? 0 : minimum);
                if (!close) minimumApplied = false;
            }
        }
        if ((flags & 4) != 0) visit(player.getInventory(), false);
        if ((flags & 8) != 0) repairClothes(player);
        if ((flags & 16) != 0) {
            boolean changed = false;
            for (var part : player.getBodyDamage().getBodyParts()) {
                if (part.getStiffness() != 0) { part.setStiffness(0); changed = true; }
            }
            if (changed && GameClient.client) GameClient.sendPlayerDamage(player);
        }
    }

    void restore() {
        if (weapon != null) {
            if (criticalApplied) {
                weapon.setCriticalChance(critical);
                weapon.setCriticalDamageMultiplier(multiplier);
                weapon.setExtraDamage(extra);
            }
            if (minimumApplied) weapon.setMinRange(minimum);
        }
        weapon = null;
        criticalApplied = minimumApplied = false;
    }

    static int visit(ItemContainer root, boolean water) {
        Set<ItemContainer> seen = Collections.newSetFromMap(new IdentityHashMap<>());
        return visit(root, water, seen, 0, new int[]{0});
    }

    private static int visit(ItemContainer root, boolean water, Set<ItemContainer> seen, int depth, int[] visited) {
        if (root == null || !seen.add(root)) return 0;
        if (depth > 16) throw new IllegalStateException("背包嵌套超过 16 层");
        int changed = 0;
        for (InventoryItem item : root.getItems()) {
            if (++visited[0] > 4096) throw new IllegalStateException("背包物品超过 4096 件，已停止");
            if (water) {
                var fluid = item.getFluidContainer();
                if (item.hasTag(ItemTag.BUCKET) && fluid != null &&
                    (fluid.isEmpty() || fluid.isWaterOnlySource()) && !fluid.isFull() && fluid.canAddFluid(Fluid.Water)) {
                    fluid.addFluid(Fluid.Water, fluid.getCapacity() - fluid.getAmount());
                    if (GameClient.client) NetworkTools.send(PacketType.ItemStats, item.getContainer(), item);
                    changed++;
                }
            } else if (item.getCondition() < item.getConditionMax()) {
                item.setConditionNoSound(item.getConditionMax());
                if (GameClient.client) item.syncItemFields();
                changed++;
            }
            if (item instanceof InventoryContainer bag) changed += visit(bag.getInventory(), water, seen, depth + 1, visited);
        }
        return changed;
    }

    private static void repairClothes(IsoPlayer player) {
        boolean changed = false;
        var worn = player.getWornItems();
        for (int i = 0; i < worn.size(); i++) {
            if (!(worn.getItemByIndex(i) instanceof Clothing clothing)) continue;
            var visual = clothing.getVisual();
            if (clothing.getCondition() == clothing.getConditionMax() && clothing.getDirtiness() == 0 &&
                clothing.getBloodLevel() == 0 && clothing.getPatchesNumber() == 0 &&
                (visual == null || visual.getHolesNumber() == 0)) continue;
            clothing.setConditionNoSound(clothing.getConditionMax());
            clothing.setDirtiness(0);
            clothing.setBloodLevel(0);
            clothing.removeAllPatches();
            if (visual != null) {
                for (int part = 0; part < zombie.characterTextures.BloodBodyPartType.MAX.index(); part++) {
                    visual.removePatch(part);
                    visual.removeHole(part);
                }
                visual.removeBlood();
                visual.removeDirt();
            }
            changed = true;
            if (GameClient.client) clothing.syncItemFields();
        }
        if (changed) {
            player.resetModelNextFrame();
            if (GameClient.client) player.syncVisuals();
        }
    }
}
