import java.lang.reflect.Field;
import pztrainer.player.visual.ViewpointModelChams;

public class ViewpointModelChamsTest {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        ViewpointModelChams.validate();
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
