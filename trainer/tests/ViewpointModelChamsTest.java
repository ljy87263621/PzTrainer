import java.lang.reflect.Field;
import pztrainer.player.visual.ViewpointModelChams;

public class ViewpointModelChamsTest {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    private static void ownershipWithoutOutline() throws Exception {
        Class<?> unsafeType = Class.forName("sun.misc.Unsafe");
        Field singleton = unsafeType.getDeclaredField("theUnsafe"); singleton.setAccessible(true);
        Object unsafe = singleton.get(null);
        var allocate = unsafeType.getMethod("allocateInstance", Class.class);
        var zombie = (zombie.iso.IsoMovingObject)allocate.invoke(unsafe, zombie.characters.IsoZombie.class);
        var player = (zombie.iso.IsoMovingObject)allocate.invoke(unsafe, zombie.characters.IsoPlayer.class);
        var root = new zombie.core.skinnedmodel.model.ModelInstance(); root.object = zombie;
        var clothing = new zombie.core.skinnedmodel.model.ModelInstance(); clothing.parent = root;
        var local = new zombie.core.skinnedmodel.model.ModelInstance(); local.object = player;
        var instances = new zombie.core.skinnedmodel.model.ModelInstance[]{root, clothing, local, null};
        var match = ViewpointModelChams.class.getMethod("groupsByOwner", instances.getClass(), int.class, int[].class, int[].class);
        int[] actual = (int[])match.invoke(null, instances, 4, new int[]{System.identityHashCode(zombie)}, new int[]{1});
        check(actual[0] == 1 && actual[1] == 1, "Unoutlined first-person character or clothing lost ownership");
        check(actual[2] == -1 && actual[3] == -1, "Local player or scenery inherited zombie material");
        check(((int[])match.invoke(null, instances, 4, new int[0], new int[0]))[0] == -1, "Stale entity was selected");
        check(((int[])match.invoke(null,instances,4,new int[]{System.identityHashCode(zombie),System.identityHashCode(zombie)},new int[]{1,3}))[0]==-1,
            "Conflicting owner identities must not paint another entity group");
        clothing.parent = clothing;
        check(((int[])match.invoke(null, instances, 4, new int[]{System.identityHashCode(zombie)}, new int[]{1}))[1] == -1,
            "Cyclic parent graph must terminate without assigning an owner");
    }
    public static void main(String[] args) throws Exception {
        ViewpointModelChams.validate();
        ownershipWithoutOutline();
        Class<?> type = Class.forName("viewpoint.render.ModelDraws");
        Object draws = type.getConstructor().newInstance();
        float[] markers = new float[48];
        for (int group = 0; group < 12; group++) {
            int at = group * 4;
            markers[at] = 0.05f * (group + 1);
            markers[at + 1] = 0.9f - 0.03f * group;
            markers[at + 2] = 0.1f + 0.02f * group;
            type.getMethod("outline", int.class, float.class, float.class, float.class, float.class, boolean.class)
                .invoke(draws, group, markers[at], markers[at + 1], markers[at + 2], 0f, true);
        }
        Field values = type.getDeclaredField("values");
        values.setAccessible(true);
        float[] records = (float[]) values.get(draws);
        int[] groups = ViewpointModelChams.groups(records, 13, markers);
        for (int group = 0; group < 12; group++) check(groups[group] == group, "Transparent capture marker was lost: " + group);
        check(groups[12] == -1, "Unmarked scenery was selected for model chams");
        check(ViewpointModelChams.groups(records, 0, markers).length == 0, "Empty frame was not empty");
        for (int stride : new int[]{60,68}) {
            float[] layout=new float[stride*2];
            System.arraycopy(markers,0,layout,stride+32,3); layout[stride+35]=0.001f;
            int[] mapped=ViewpointModelChams.groups(layout,2,markers,stride,32);
            check(mapped[0]==-1 && mapped[1]==0,"Record-stride compatibility failed: "+stride);
        }
        float[] wrong = records.clone();
        wrong[32] += 0.02f;
        check(ViewpointModelChams.groups(wrong, 1, markers)[0] == -1, "Unrelated model colour was selected");
        var reset = type.getDeclaredMethod("reset");
        reset.setAccessible(true);
        reset.invoke(draws);
        Class<?> matrixType = Class.forName("org.joml.Matrix4f");
        type.getMethod("add", Class.forName("zombie.core.skinnedmodel.model.ModelMesh"),
            Class.forName("zombie.core.skinnedmodel.model.ModelInstance"), Class.forName("zombie.core.textures.Texture"),
            matrixType, int.class, int.class, int.class, int.class)
            .invoke(draws, null, null, null, matrixType.getConstructor().newInstance(), -1, -1, 0, 1);
        records = (float[]) values.get(draws);
        check(records[32] == markers[0] && records[35] == 0f, "Actual model reuse did not retain RGB and clear outline validity");
        check(ViewpointModelChams.groups(records, 1, markers)[0] == -1, "Reused scenery record inherited character chams");
        try {
            ViewpointModelChams.groups(new float[59], 1, markers);
            throw new AssertionError("Changed record layout was accepted");
        } catch (IllegalArgumentException expected) {}
        System.out.println("Actual Viewpoint renderer API and model records: all 12 transparent markers, scenery isolation and empty frames passed.");
    }
}
