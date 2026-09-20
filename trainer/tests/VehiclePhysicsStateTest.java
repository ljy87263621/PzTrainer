package pztrainer.extensions;

import java.util.Arrays;

public final class VehiclePhysicsStateTest {
    private static int checks;

    private static float[] state(float encodedWheels) {
        float[] values = new float[27];
        values[0] = 100; values[1] = 200; values[2] = 0.5f;
        values[6] = 1; values[10] = encodedWheels;
        for (int i = 11; i < values.length; i++) values[i] = i / 10f;
        return values;
    }

    private static void accepts(float encodedWheels) {
        float[] values = state(encodedWheels), original = values.clone();
        VehiclePhysics.validateState(values);
        if (!Arrays.equals(values, original)) throw new AssertionError("Validation changed the transfer buffer");
        checks++;
    }

    private static void rejects(float[] values, String reason) {
        try { VehiclePhysics.validateState(values); }
        catch (IllegalStateException error) {
            if (!error.getMessage().contains(reason)) throw new AssertionError(error);
            checks++; return;
        }
        throw new AssertionError("Invalid physics state accepted");
    }

    public static void main(String[] args) {
        for (int wheels = 1; wheels <= 4; wheels++) {
            accepts(wheels + 0.1f); // Actual native Bullet encoding, including the reported four-wheel car.
            accepts(wheels);
        }
        for (float encoded : new float[]{-1, 0.1f, 5.1f, Float.MAX_VALUE}) rejects(state(encoded), "读取轮数");
        for (float encoded : new float[]{Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY})
            rejects(state(encoded), "数据无效");
        float[] values = state(4.1f); values[11] = Float.NaN;
        rejects(values, "数据无效");
        values = state(4.1f); values[6] = 0;
        rejects(values, "旋转数据无效");
        System.out.println("Vehicle physics state: " + checks + " checks passed; four-wheel encoding and transfer buffer preserved.");
    }
}
