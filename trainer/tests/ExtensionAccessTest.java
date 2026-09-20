package pztrainer.extensions;

public final class ExtensionAccessTest {
    private static int count;
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
        count++;
    }
    public static void main(String[] args) {
        var solo = new ExtensionAccess.State(false, true, true, false, false, true, true);
        var online = new ExtensionAccess.State(true, true, true, false, false, true, true);
        var privileged = new ExtensionAccess.State(true, true, true, true, true, true, true);
        var disconnected = new ExtensionAccess.State(true, false, false, false, false, false, false);
        var walking = new ExtensionAccess.State(true, true, true, false, false, false, false);
        check(solo.filter(2047) == 2047, "solo features remain available");
        check(online.filter(2047) == (2047 & ~(16 | 32 | 256)), "ordinary online session masks unsupported state");
        check(privileged.filter(2047) == (2047 & ~32), "server-granted capabilities enable body and invisibility");
        check(disconnected.filter(2047) == 0, "disconnect suppresses all continuous gameplay writes");
        check(!walking.reason("flag:1024").isEmpty(), "physics requires local vehicle authority");
        check(!walking.reason("vehicle_start").isEmpty(), "online start requires driving seat");
        check(walking.reason("vehicle_repair").isEmpty(), "nearby vehicle repair remains available");
        check(online.reason("buckets").isEmpty(), "bucket synchronization online");
        check(online.reason("containers_reroll").isEmpty(), "batched container requests online");
        check(online.reason("creator_name").isEmpty(), "multiplayer creation allowed");
        check(online.reason("creator_skill").isEmpty(), "creation skills are not cleared just for client mode");
        check(!online.reason("creator_world").isEmpty(), "server controls world parameters");
        check(solo.reason("creator_world").isEmpty(), "offline new-world editing allowed");
        check(!online.reason("flag:2048").isEmpty(), "unknown flags rejected");
        check(!online.reason("unknown").isEmpty(), "unknown commands rejected");
        for (String removed : new String[]{"tile_inspect", "tile_toggle", "tile_unlock", "tile_remove", "tile_glass", "tile_fire", "tile_throwable", "animal_remove", "animal_rename", "sound"})
            check(online.reason(removed).equals("未知功能"), "removed command rejected: " + removed);
        check(online.snapshot().contains("flag:32\t该全局开关"), "UI receives exact disabled reason");
        check(solo.snapshot().contains("flag:32\t\n"), "returning offline clears disabled reason");
        System.out.println("Extension session access: " + count + " checks passed");
    }
}
