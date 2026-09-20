import java.nio.ByteBuffer;
import zombie.characters.IsoPlayer;
import zombie.inventory.ItemContainer;
import zombie.network.GameClient;
import zombie.core.raknet.UdpConnection;
import zombie.core.network.ByteBufferWriter;
import zombie.network.packets.connection.ConnectPacket;

public final class PlayerOverridesJvmTest {
    private static java.net.URLClassLoader bridgeLoader;
    public static Class<?> loadEmbedded(String name) throws ClassNotFoundException {
        return bridgeLoader.loadClass(name);
    }
    private static native String install();
    private static void field(Class<?> type, String name, Object value) throws Exception {
        var field = type.getDeclaredField(name); field.setAccessible(true); field.set(null, value);
    }
    private static Object allocate(Class<?> type) throws Exception {
        Class<?> unsafe = Class.forName("sun.misc.Unsafe");
        var field = unsafe.getDeclaredField("theUnsafe"); field.setAccessible(true);
        return unsafe.getMethod("allocateInstance", Class.class).invoke(field.get(null), type);
    }
    private static void carryState(Class<?> carry, int expected, String reason) throws Exception {
        var update = carry.getDeclaredMethod("update"); update.setAccessible(true); update.invoke(null);
        var state = carry.getDeclaredField("state"); state.setAccessible(true);
        String message = (String)carry.getMethod("message").invoke(null);
        if (state.getInt(null) != expected || (!reason.isEmpty() && !message.equals(reason)))
            throw new AssertionError("Carry session state: expected " + expected + " / " + reason
                + ", actual " + state.getInt(null) + " / " + message);
    }
    public static void main(String[] args) throws Exception {
        zombie.core.random.RandStandard.INSTANCE.init();
        bridgeLoader = new java.net.URLClassLoader(new java.net.URL[]{
            java.nio.file.Path.of(args[1]).toUri().toURL()}, ClassLoader.getSystemClassLoader());
        Class<?> runtime = args.length > 2 ? null : loadEmbedded("pztrainer.extensions.ExtensionRuntime");
        if (runtime != null) Class.forName(runtime.getName(), true, bridgeLoader);
        System.load(args[0]);
        String error = install();
        if (!error.isEmpty()) throw new AssertionError(error);
        if (!install().isEmpty()) throw new AssertionError("Repeated install failed");
        if (runtime == null) runtime = loadEmbedded("pztrainer.extensions.ExtensionRuntime");
        var run = runtime.getDeclaredMethod("run"); run.setAccessible(true); run.invoke(null);
        String snapshot = (String)runtime.getMethod("snapshot").invoke(null);
        if (!snapshot.contains("会话：等待角色") || snapshot.contains("操作失败"))
            throw new AssertionError("Main-menu extension update failed: " + snapshot);
        try (var jar = new java.util.jar.JarFile(args[1])) {
            for (var entry : jar.stream().filter(e -> e.getName().endsWith(".class")).toList()) {
                String name = entry.getName().replace('/', '.').replaceAll("\\.class$", "");
                if (loadEmbedded(name).getClassLoader() != bridgeLoader)
                    throw new AssertionError("Bridge class escaped its loader: " + name);
                try {
                    ClassLoader.getSystemClassLoader().loadClass(name);
                    throw new AssertionError("Bridge class leaked into the system loader: " + name);
                } catch (ClassNotFoundException expected) {}
            }
        }
        System.out.println("Main-menu extension update and all bridge class loaders passed: " + (args.length > 2 ? "player first" : "extensions first"));
        ClassLoader loader = ClassLoader.getSystemClassLoader();
        for (String name : new String[]{"pztrainer.player.CarryOverrides", "pztrainer.player.GodModeConnection", "pztrainer.player.PlayerTransforms"})
            if (Class.forName(name).getClassLoader() != loader) throw new AssertionError("Game cannot resolve helper: " + name);
        var carry = Class.forName("pztrainer.player.CarryOverrides");
        if (!carry.getMethod("scale", int.class, float.class).invoke(null, 8, 10f).equals(80)) throw new AssertionError("Carry helper failed");
        IsoPlayer player = (IsoPlayer)allocate(IsoPlayer.class);
        player.cheats = new zombie.characters.PlayerCheats();
        IsoPlayer.numPlayers = 1; IsoPlayer.players[0] = player;
        player.setHealth(100); player.setMaxWeight(8);
        ItemContainer inventory = new ItemContainer(); player.setInventory(inventory);
        field(carry, "target", player); field(carry, "requested", true); field(carry, "multiplier", 10f);
        if (player.getMaxWeight() != 80 || inventory.getEffectiveCapacity(player) != Integer.MAX_VALUE)
            throw new AssertionError("Transformed carry getters did not apply");
        field(carry, "lastWeight", 80); player.setMaxWeight(80);
        if (player.getMaxWeight() != 80) throw new AssertionError("Server echo multiplied twice");
        IsoPlayer other = (IsoPlayer)allocate(IsoPlayer.class); other.setMaxWeight(8); other.setHealth(100);
        if (other.getMaxWeight() != 8) throw new AssertionError("Other player's weight changed");
        field(carry, "requested", false); player.setMaxWeight(8);
        if (player.getMaxWeight() != 8 || inventory.getEffectiveCapacity(player) == Integer.MAX_VALUE)
            throw new AssertionError("Carry disable did not restore getters");
        IsoPlayer.setInstance(player);
        GameClient.client = true; GameClient.connection = (UdpConnection)allocate(UdpConnection.class);
        GameClient.instance.connected = true;
        if (GameClient.connection.isFullyConnected()) throw new AssertionError("Expected unset server-side connection flag");
        carryState(carry, 13, "");
        // Keep packet transmission out of this isolated JVM; exercise the real update and getters.
        field(carry, "requested", true); field(carry, "lastWeight", 80); field(carry, "nextSync", Long.MAX_VALUE);
        carryState(carry, 15, "");
        if (player.getMaxWeight() != 80 || inventory.getEffectiveCapacity(player) != Integer.MAX_VALUE)
            throw new AssertionError("Connected local player cannot use carry overrides");
        GameClient.instance.connected = false;
        carryState(carry, 8, "等待客户端连接就绪");
        if (inventory.getEffectiveCapacity(player) == Integer.MAX_VALUE)
            throw new AssertionError("Disconnected player retained capacity override");
        GameClient.instance.connected = true; GameClient.connection = null;
        carryState(carry, 8, "等待客户端连接就绪");
        GameClient.connection = (UdpConnection)allocate(UdpConnection.class);
        field(carry, "requested", false); player.setHealth(0);
        carryState(carry, 8, "角色已死亡");
        player.setHealth(100); IsoPlayer.setInstance(other);
        carryState(carry, 8, "等待本机可操作角色");
        IsoPlayer.setInstance(null);
        carryState(carry, 8, "等待角色进入游戏");
        IsoPlayer.setInstance(player); GameClient.connection = null; GameClient.client = false;
        carryState(carry, 5, "");
        field(carry, "requested", true); carryState(carry, 7, "");
        field(carry, "requested", false); carryState(carry, 5, "");
        if (player.getMaxWeight() != 8 || inventory.getEffectiveCapacity(player) == Integer.MAX_VALUE)
            throw new AssertionError("Single-player carry did not restore");
        zombie.network.GameServer.server = true;
        carryState(carry, 0, "当前为服务器进程");
        zombie.network.GameServer.server = false; IsoPlayer.setInstance(null);
        System.out.println("Carry session update: connected client with unset server flag, enable, disconnect, missing connection, dead/remote/missing player, single-player restore and server exclusion passed.");
        var god = Class.forName("pztrainer.player.GodModeConnection");
        var configure = god.getMethod("configure", boolean.class);
        GameClient.client = true; GameClient.connection = (UdpConnection)allocate(UdpConnection.class);
        GameClient.instance.playerConnectSent = false;
        if (!configure.invoke(null, true).equals(3)) throw new AssertionError("Godmode could not arm before connection");
        ConnectPacket packet = new ConnectPacket();
        var flags = ConnectPacket.class.getDeclaredField("extraInfoFlags"); flags.setAccessible(true); flags.setByte(packet, (byte)0x14);
        ByteBuffer buffer = ByteBuffer.allocate(16); packet.write(new ByteBufferWriter(buffer));
        if (buffer.get(2) != 0x15) throw new AssertionError("Actual connection packet did not preserve flags and add godmode");
        GameClient.instance.playerConnectSent = true;
        if (!configure.invoke(null, false).equals(2)) throw new AssertionError("Connected godmode was not locked");
        GameClient.connection = null; GameClient.client = false;
        if (!configure.invoke(null, false).equals(1)) throw new AssertionError("Disconnect did not unlock godmode");
        System.out.println("Production JVMTI installer: five game classes retransformed; system-loader helpers resolve; repeated install passed.");
        System.out.println("Transformed game methods: carry/root-capacity, server echo, other-player isolation, disable restoration and connection-packet godmode lifecycle passed (isolated objects, no server).");
    }
}
