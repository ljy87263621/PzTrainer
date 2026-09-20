package pztrainer.extensions;

import zombie.characters.IsoPlayer;
import zombie.core.Core;
import zombie.core.physics.Bullet;
import zombie.input.GameKeyboard;
import zombie.input.JoypadManager;
import zombie.network.GameClient;
import zombie.network.GameServer;
import zombie.network.ServerOptions;
import zombie.vehicles.BaseVehicle;
import zombie.vehicles.VehicleManager;

public final class VehiclePhysics {
    private static volatile boolean requested, controlsBlocked;
    private static volatile long heartbeat;
    private static volatile String status = "车辆物理：原版";
    private static BaseVehicle retained;
    private static boolean originalStatic, publish;
    private static long originalFlags, lastStep;
    private static float height;
    private static final float[] physics = new float[27];
    private static native long setGhost(BaseVehicle vehicle, boolean isStatic, long restoreFlags);

    static void request(boolean enabled, boolean blocked) {
        requested = enabled; controlsBlocked = blocked; heartbeat = System.nanoTime();
    }

    static String status() { return status; }

    static void validateState(float[] state) {
        for (float value : state) if (!Float.isFinite(value)) throw new IllegalStateException("车辆物理数据无效");
        // Bullet encodes wheel count as count + 0.1f and truncates it when reading back.
        // Preserve the encoded value in the transfer buffer for setOwnVehiclePhysics.
        int wheels = (int)state[10];
        if (wheels < 1 || wheels > 4) throw new IllegalStateException("仅支持 1–4 轮车辆（读取轮数：" + wheels + "）");
        double norm = 0;
        for (int i = 3; i <= 6; i++) norm += state[i] * state[i];
        if (norm < 0.99 || norm > 1.01) throw new IllegalStateException("车辆旋转数据无效");
    }

    private static void restore() {
        if (retained == null) return;
        if (VehicleManager.instance.getVehicleByID(retained.getId()) == retained && !retained.serverRemovedFromWorld) {
            if (setGhost(retained, originalStatic, originalFlags) < 0) throw new IllegalStateException("车辆物理恢复失败，等待重试");
        }
        retained = null;
        publish = false;
    }

    public static void beforeStep(float seconds) {
        publish = false;
        try {
            if (!Float.isFinite(seconds) || seconds <= 0) { restore(); return; }
            long now = System.nanoTime();
            IsoPlayer player = IsoPlayer.getInstance();
            BaseVehicle vehicle = player == null ? null : player.getVehicle();
            if (!requested || now - heartbeat > 2_000_000_000L || GameServer.server ||
                (GameClient.client && !NetworkTools.connected()) ||
                player == null || player.isDead() || vehicle == null || !vehicle.isDriver(player) ||
                !vehicle.isEngineRunning() || !vehicle.isLocalPhysicSim() || vehicle.serverRemovedFromWorld ||
                VehicleManager.instance.getVehicleByID(vehicle.getId()) != vehicle ||
                vehicle.getVehicleTowing() != null || vehicle.getVehicleTowedBy() != null) {
                restore(); status = requested ? "车辆物理：等待本机驾驶，拖挂车辆不支持" : "车辆物理：原版"; return;
            }
            if (retained != null && retained != vehicle) restore();
            if (Bullet.getOwnVehiclePhysics(vehicle.getId(), physics) != 0) throw new IllegalStateException("无法读取车辆物理状态");
            validateState(physics);
            if (retained == null) {
                originalStatic = vehicle.isStatic;
                long captured = setGhost(vehicle, true, -1);
                if (captured < 0) {
                    Bullet.setVehicleStatic(vehicle, originalStatic);
                    throw new IllegalStateException("当前 Bullet 版本无法启用无碰撞车辆");
                }
                originalFlags = captured; retained = vehicle; height = physics[2]; lastStep = now;
            }
            float right = 0, down = 0;
            boolean brake = false;
            int joypad = vehicle.getJoypad();
            if (joypad >= 0) {
                right = JoypadManager.instance.getMovementAxisX(joypad);
                down = JoypadManager.instance.getMovementAxisY(joypad);
                brake = JoypadManager.instance.isBPressed(joypad);
            } else if (vehicle.isKeyboardControlled()) {
                right = (GameKeyboard.isKeyDown("Right") ? 1 : 0) - (GameKeyboard.isKeyDown("Left") ? 1 : 0);
                down = (GameKeyboard.isKeyDown("Backward") ? 1 : 0) - (GameKeyboard.isKeyDown("Forward") ? 1 : 0);
                brake = GameKeyboard.isKeyDown("Brake");
            }
            boolean blocked = controlsBlocked || Core.getInstance().isDoingTextEntry() || player.isBlockMovement() || GameKeyboard.noEventsWhileLoading;
            double x = right + 2.0 * down, z = -right + 2.0 * down;
            double length = Math.sqrt(x * x + z * z);
            double elapsed = Math.min(Math.max(0, (now - lastStep) / 1e9), 0.1);
            lastStep = now;
            double yaw = Math.atan2(2 * (physics[3] * physics[5] + physics[6] * physics[4]),
                1 - 2 * (physics[3] * physics[3] + physics[4] * physics[4]));
            if (!blocked && !brake && Double.isFinite(length) && length > 0.1) {
                // 70 km/h, converted to physics metres per second.
                double speed = GameClient.client ? Math.min(70.0, ServerOptions.instance.speedLimit.getValue()) : 70.0;
                if (!Double.isFinite(speed) || speed <= 0) throw new IllegalStateException("服务器车辆限速不可用");
                double distance = speed / 3.6 * Math.min(elapsed, Math.max(0, seconds));
                physics[0] += (float)(x / length * distance);
                physics[1] += (float)(z / length * distance);
                yaw = Math.atan2(x, z);
            }
            physics[2] = height;
            physics[3] = physics[5] = 0;
            physics[4] = (float)Math.sin(yaw / 2); physics[6] = (float)Math.cos(yaw / 2);
            physics[7] = physics[8] = physics[9] = 0;
            Bullet.controlVehicle(vehicle.getId(), 0, 0, 0);
            write(); publish = true; status = "车辆物理：Instant 已启用，联机遵循服务器限速";
        } catch (RuntimeException | LinkageError error) {
            status = "车辆物理失败：" + error.getMessage();
            try { restore(); } catch (RuntimeException | LinkageError restoreError) { status += "；" + restoreError.getMessage(); }
        }
    }

    private static void write() {
        if (setGhost(retained, true, -1) < 0) throw new IllegalStateException("无法保持车辆物理模式");
        retained.jniIsCollide = false;
        if (Bullet.setOwnVehiclePhysics(retained.getId(), physics, false) < 0) throw new IllegalStateException("无法写入车辆物理状态");
    }

    public static void afterStep() {
        if (!publish || retained == null) return;
        try { write(); }
        catch (RuntimeException | LinkageError error) {
            status = "车辆物理失败：" + error.getMessage();
            try { restore(); } catch (RuntimeException | LinkageError restoreError) { status += "；" + restoreError.getMessage(); }
        }
    }
}
