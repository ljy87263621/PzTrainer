package pztrainer.extensions;

import java.util.IdentityHashMap;
import java.util.HashSet;
import zombie.SystemDisabler;
import zombie.characters.IsoPlayer;
import zombie.characters.IsoZombie;
import zombie.network.GameClient;
import zombie.network.PacketTypes.PacketType;
import zombie.network.packets.character.ZombieSimulationPacket;

final class ZombieTools {
    private final IdentityHashMap<IsoZombie, Boolean> originals = new IdentityHashMap<>();
    private final HashSet<IsoZombie> pending = new HashSet<>();
    private boolean passiveApplied, passiveOriginal;
    private IsoPlayer invisiblePlayer;
    private boolean invisibleOriginal;
    private Object retainedConnection;

    void update(IsoPlayer player, int flags, int radius) {
        retainedConnection = GameClient.connection;
        boolean passive = (flags & 32) != 0;
        if (passive && !passiveApplied) {
            passiveOriginal = SystemDisabler.zombiesDontAttack;
            passiveApplied = true;
        }
        if (passiveApplied) {
            SystemDisabler.zombiesDontAttack = passive || passiveOriginal;
            if (!passive) passiveApplied = false;
        }
        boolean invisible = (flags & 256) != 0;
        if (invisiblePlayer != null && (invisiblePlayer != player || !invisible)) {
            invisiblePlayer.setInvisible(invisibleOriginal);
            publishInvisible(invisiblePlayer);
            invisiblePlayer = null;
        }
        if (invisible) {
            if (invisiblePlayer == null) { invisibleOriginal = player.isInvisible(); invisiblePlayer = player; }
            if (!player.isInvisible()) { player.setInvisible(true); publishInvisible(player); }
        }
        var zombies = player.getCell().getZombieList();
        for (var it = originals.entrySet().iterator(); it.hasNext();) {
            var entry = it.next();
            if ((flags & 64) == 0 || !zombies.contains(entry.getKey()) || !owned(entry.getKey())) {
                if (owned(entry.getKey())) {
                    entry.getKey().setUseless(entry.getValue());
                    pending.add(entry.getKey());
                }
                it.remove();
            }
        }
        if ((flags & (64 | 128)) == 0) { publish(); return; }
        for (IsoZombie zombie : zombies.toArray(new IsoZombie[0])) {
            if (zombie == null || zombie.isDead() || !owned(zombie)) continue;
            if ((flags & 64) != 0) {
                originals.putIfAbsent(zombie, zombie.isUseless());
                if (!zombie.isUseless()) { zombie.setUseless(true); pending.add(zombie); }
            }
            float dx = zombie.getX() - player.getX(), dy = zombie.getY() - player.getY();
            if ((flags & 128) != 0 && (radius == 30 || dx * dx + dy * dy <= radius * radius)) {
                if (GameClient.client) { zombie.setHealth(-1); pending.add(zombie); }
                else zombie.Kill(player);
            }
        }
        publish();
    }

    void restore() {
        for (var entry : originals.entrySet()) {
            if (owned(entry.getKey())) { entry.getKey().setUseless(entry.getValue()); pending.add(entry.getKey()); }
        }
        originals.clear();
        if (NetworkTools.connected() && retainedConnection == GameClient.connection) publish();
        else pending.clear();
        if (passiveApplied) SystemDisabler.zombiesDontAttack = passiveOriginal;
        passiveApplied = false;
        if (invisiblePlayer != null) { invisiblePlayer.setInvisible(invisibleOriginal); publishInvisible(invisiblePlayer); }
        invisiblePlayer = null;
    }

    private static boolean owned(IsoZombie zombie) {
        return !GameClient.client || (zombie.onlineId != -1 && zombie.isLocal());
    }

    private void publishInvisible(IsoPlayer player) {
        if (GameClient.client && NetworkTools.connected() && retainedConnection == GameClient.connection && player == IsoPlayer.getInstance())
            GameClient.sendPlayerExtraInfo(player);
    }

    private void publish() {
        if (!GameClient.client) { pending.clear(); return; }
        if (!NetworkTools.connected()) return;
        pending.removeIf(zombie -> !owned(zombie));
        if (pending.isEmpty()) return;
        var packet = new ZombieSimulationPacket();
        for (IsoZombie zombie : pending) {
            if (packet.sendQueue.size() >= 300) break;
            var network = zombie.getNetworkCharacterAI();
            network.targetX = zombie.realx = zombie.getX();
            network.targetY = zombie.realy = zombie.getY();
            network.targetZ = zombie.realz = (byte)zombie.getZi();
            packet.sendQueue.add(zombie);
        }
        var sent = new HashSet<>(packet.sendQueue);
        var connection = GameClient.connection;
        var type = PacketType.ZombieSimulationReliable;
        var writer = connection.startPacket();
        try {
            type.doPacket(writer);
            packet.write(writer);
            if (connection.isLimitExceeded(type)) { connection.cancelPacket(); return; }
            connection.endPacket(type.packetPriority, type.packetReliability, type.orderingChannel);
            pending.removeAll(sent);
        } catch (RuntimeException | LinkageError error) { connection.cancelPacket(); throw error; }
    }
}
